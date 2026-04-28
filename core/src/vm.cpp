// src/vm.cpp
#include <numkit/core/vm.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/span.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>
#include <sstream>
#include <stdexcept>

// Forward-declarations from libs/builtin/src/backends/BinaryOpsLoops.hpp.
// Linked from the same numkit_m library; lets the VM bypass the public
// builtin::plus()/etc. wrappers (which always allocate a fresh result
// Value) when it can write straight into a uniquely-owned destination
// buffer of the right shape.
namespace numkit::builtin::detail {
void plusLoop   (const double *a, const double *b, double *out, std::size_t n);
void minusLoop  (const double *a, const double *b, double *out, std::size_t n);
void timesLoop  (const double *a, const double *b, double *out, std::size_t n);
void rdivideLoop(const double *a, const double *b, double *out, std::size_t n);
} // namespace numkit::builtin::detail

namespace numkit {

// Output-reuse fast path for `dst = lhs op rhs` where both inputs and
// the destination are real heap doubles of the same shape and dst has
// unique ownership. Skips the N-element alloc + free that the public
// builtin::plus()/etc. wrappers would otherwise do per call — at
// N ≥ 256k that allocation hits Windows VirtualAlloc / page-commit
// and starts costing 3× more than the SIMD kernel itself
// (see benchmarks BM_PlusAlloc / BM_PlusKernel for the breakdown).
//
// Aliasing notes: the four supported ops (+ − .* ./) are independent
// per element — output may safely alias either input (e.g. `z = z + z`
// or `z = x + z`) because plusLoop reads (a[i], b[i]) before writing
// out[i]. Cross-output sharing is the dangerous case; the
// heapRefCount() == 1 guard rules it out (any other variable holding
// dst's buffer would bump it to ≥2).
static bool tryInPlaceBinaryOp(Value &dst, OpCode op,
                               const Value &lhs, const Value &rhs)
{
    if (!dst.isHeapDouble() || dst.heapRefCount() != 1) return false;
    if (!lhs.isHeapDouble() || !rhs.isHeapDouble())     return false;
    if (!(dst.dims() == lhs.dims()) || !(dst.dims() == rhs.dims())) return false;

    const std::size_t n = dst.numel();
    const double *a = lhs.doubleData();
    const double *b = rhs.doubleData();
    double       *o = dst.doubleDataMut();   // refCount==1 → no detach copy
    switch (op) {
    case OpCode::ADD:   builtin::detail::plusLoop   (a, b, o, n); return true;
    case OpCode::SUB:   builtin::detail::minusLoop  (a, b, o, n); return true;
    case OpCode::EMUL:  builtin::detail::timesLoop  (a, b, o, n); return true;
    case OpCode::ERDIV: builtin::detail::rdivideLoop(a, b, o, n); return true;
    default: return false;
    }
}

// ============================================================
// Shared helper: resolve an index operand to 0-based indices
// ============================================================

VM::VM(Engine &engine)
    : engine_(engine)
{
    regStack_.resize(kRegStackSize);
}


// Fast scalar check for VM arithmetic — accepts double scalars AND logical scalar tags
static inline bool isArithScalar(const Value &v)
{
    return v.isDoubleScalar() || v.isLogicalScalar();
}

static inline double asScalar(const Value &v)
{
    return v.fastScalarVal();
}

static inline size_t checkedIndex(double idx, size_t numel)
{
    if (idx < 1.0 || idx != std::floor(idx))
        throw std::runtime_error("Index must be a positive integer, got " + std::to_string(idx));
    size_t i = static_cast<size_t>(idx) - 1;
    if (i >= numel)
        throw std::runtime_error("Index " + std::to_string((size_t) idx) + " exceeds array size "
                                 + std::to_string(numel));
    return i;
}

// ============================================================
// Public API
// ============================================================

Value VM::execute(const BytecodeChunk &chunk, const Value *args, uint8_t nargs)
{
    // RAII: export variables and cleanup on ANY exit path (success, exception, debug stop).
    // Guarantees MATLAB-like behavior: variables assigned before an error survive in workspace.
    struct ExecuteGuard
    {
        VM &vm;
        ExecuteGuard(VM &v)
            : vm(v)
        {}
        ~ExecuteGuard()
        {
            if (!vm.frames_.empty())
                vm.exportTopLevelVariables();
            vm.frames_.clear();
            vm.forStack_.clear();
            vm.tryStack_.clear();
            vm.regStackTop_ = 0;
            vm.R_ = nullptr;
        }
        ExecuteGuard(const ExecuteGuard &) = delete;
        ExecuteGuard &operator=(const ExecuteGuard &) = delete;
    } guard(*this);

    ExecStatus status = startExecution(chunk, args, nargs);

    if (status == ExecStatus::Paused) {
        // Legacy API: convert pause to exception (guard will clean up)
        throw DebugStopException();
    }

    return std::move(lastResult_);
}

// ── Paused state save/restore (for debug eval) ────────────

std::unique_ptr<VM::PausedState> VM::savePausedState()
{
    auto s = std::make_unique<PausedState>();
    s->frames = frames_;
    s->forStack = forStack_;
    s->tryStack = tryStack_;
    s->regStackTop = regStackTop_;
    s->lastResult = lastResult_;
    // Snapshot only the used portion of the register stack
    s->regSnapshot.assign(regStack_.begin(), regStack_.begin() + regStackTop_);
    return s;
}

void VM::restorePausedState(std::unique_ptr<PausedState> s)
{
    if (!s) return;
    frames_ = std::move(s->frames);
    forStack_ = std::move(s->forStack);
    tryStack_ = std::move(s->tryStack);
    regStackTop_ = s->regStackTop;
    lastResult_ = std::move(s->lastResult);
    // Restore registers
    std::copy(s->regSnapshot.begin(), s->regSnapshot.end(), regStack_.begin());
    // Fix R pointers in frames (they pointed into regStack_)
    R_ = regStack_.data();
    for (auto &f : frames_)
        f.R = &regStack_[f.regBase];
    // Fix ForState data pointers (they point into ForState::range).
    // Lazy ranges (FOR_INIT_RANGE) don't have a backing range Value —
    // the iteration value is recomputed from lazyStart/lazyStep each
    // step — so skip the pointer fixup. Calling doubleData() on a
    // default-constructed empty Value would throw because empty
    // MValues hold the emptyTag() sentinel rather than nullptr.
    for (auto &fs : forStack_) {
        if (fs.lazy)
            continue;
        if (fs.rangeType == ValueType::DOUBLE && fs.range.doubleData())
            fs.data = fs.range.doubleData();
        else
            fs.rawData = fs.range.rawData();
    }
}

// ── Debug-aware execution API ───────────────────────────────

ExecStatus VM::startExecution(const BytecodeChunk &chunk, const Value *args, uint8_t nargs,
                              DebugAction initialAction)
{
    chunkCallCache_.clear();
    frames_.clear();
    forStack_.clear();
    tryStack_.clear();
    regStackTop_ = 0;
    returnCount_ = 0;
    lastResult_ = Value::empty();

    // Allocate registers for top-level frame
    uint8_t nregs = chunk.numRegisters;
    R_ = regStack_.data();

    for (uint8_t i = 0; i < nregs; ++i)
        R_[i] = Value::empty();

    if (args) {
        uint8_t pc = std::min(nargs, chunk.numParams);
        for (uint8_t i = 0; i < pc; ++i)
            R_[i] = args[i];
    }

    regStackTop_ = nregs;

    // Push initial call frame
    CallFrame cf;
    cf.chunk = &chunk;
    cf.ip = chunk.code.data();
    cf.R = R_;
    cf.regBase = 0;
    cf.nregs = nregs;
    cf.forStackBase = 0;
    cf.tryStackBase = 0;
    frames_.push_back(cf);

    // Debug: push top-level debug frame
    if (auto *ctl = debugCtl()) {
        ctl->reset(initialAction);
        StackFrame sf;
        sf.functionName = chunk.name;
        sf.chunk = &chunk;
        sf.registers = R_;
        ctl->pushFrame(std::move(sf));
    }

    return dispatchLoop();
}

ExecStatus VM::resumeExecution()
{
    if (frames_.empty())
        return ExecStatus::Completed;

    return dispatchLoop();
}

// ============================================================
// Internal dispatch — Value directly, scalar fast paths
// ============================================================

#if defined(__GNUC__) || defined(__clang__)
__attribute__((flatten))
#endif
// Forward declarations for helpers defined later in this translation unit
// but called from dispatchLoop's catch blocks.
static std::string describeInstruction(const Instruction &instr,
                                       const BytecodeChunk &chunk);

ExecStatus VM::dispatchLoop()
{
enter_frame:
    if (frames_.empty())
        return ExecStatus::Completed;

    {
        CallFrame &frame = frames_.back();
        const Instruction *ip = frame.ip;
        const BytecodeChunk &chunk = *frame.chunk;
        const Instruction *end = chunk.code.data() + chunk.code.size();
        auto *R = frame.R;
        auto &resolvedFuncs = chunkCallCache_[frame.chunk];

    try {
        auto *dbgCtl = debugCtl(); // hoist out of hot loop
        while (ip < end) {
            // ── Debug hook: check for line change, breakpoints ──
            if (dbgCtl) {
                size_t idx = static_cast<size_t>(ip - chunk.code.data());
                if (idx < chunk.sourceMap.size()) {
                    auto &loc = chunk.sourceMap[idx];
                    if (loc.line > 0) {
                        if (auto *f = dbgCtl->currentFrame())
                            f->registers = R;
                        if (!dbgCtl->checkLine(loc.line, loc.col, callDepth())) {
                            frame.ip = ip;
                            return ExecStatus::Paused;
                        }
                    }
                }
            }

            const Instruction &I = *ip;

            switch (I.op) {
            // ── Data movement ────────────────────────────────────
            case OpCode::LOAD_CONST: {
                const Value &cv = chunk.constants[I.d];
                if (cv.isDoubleScalar()) {
                    if (R[I.a].isDoubleScalar())
                        R[I.a].setScalarFast(cv.scalarVal());
                    else
                        R[I.a].setScalarVal(cv.scalarVal());
                } else {
                    R[I.a] = cv;
                }
                break;
            }
            case OpCode::LOAD_EMPTY:
                R[I.a] = Value::matrix(0, 0, ValueType::DOUBLE, &engine_.allocator_);
                break;
            case OpCode::LOAD_STRING:
                R[I.a] = Value::fromString(chunk.strings[I.d], &engine_.allocator_);
                break;
            case OpCode::MOVE:
                if (R[I.b].isDoubleScalar()) {
                    if (R[I.a].isDoubleScalar())
                        R[I.a].setScalarFast(R[I.b].scalarVal());
                    else
                        R[I.a].setScalarVal(R[I.b].scalarVal());
                } else {
                    R[I.a] = R[I.b];
                }
                break;
            case OpCode::COLON_ALL:
                R[I.a] = Value::fromString(":", &engine_.allocator_);
                break;
            case OpCode::LOAD_END: {
                // a=dst, b=arrReg, c=dim (0-based), d=ndims
                const Value &arr = R[I.b];
                size_t sz;
                int ndims = I.d;
                if (ndims <= 1) {
                    // Linear indexing: end = numel
                    sz = arr.numel();
                } else {
                    // Dimensional indexing: end = size along dimension c
                    sz = arr.dims().dimSize(I.c);
                }
                R[I.a] = Value::scalar(static_cast<double>(sz), &engine_.allocator_);
                break;
            }

            // ── Scalar arithmetic ────────────────────────────────
            case OpCode::ADD:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setScalarFast(asScalar(R[I.b]) + asScalar(R[I.c]));
                } else if (!tryInPlaceBinaryOp(R[I.a], OpCode::ADD, R[I.b], R[I.c])) {
                    R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]);
                }
                break;
            case OpCode::SUB:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setScalarFast(asScalar(R[I.b]) - asScalar(R[I.c]));
                } else if (!tryInPlaceBinaryOp(R[I.a], OpCode::SUB, R[I.b], R[I.c])) {
                    R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]);
                }
                break;
            case OpCode::MUL:  // matrix multiply — output reuse skipped (different shape rules)
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setScalarFast(asScalar(R[I.b]) * asScalar(R[I.c]));
                } else
                    R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]);
                break;
            case OpCode::EMUL:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setScalarFast(asScalar(R[I.b]) * asScalar(R[I.c]));
                } else if (!tryInPlaceBinaryOp(R[I.a], OpCode::EMUL, R[I.b], R[I.c])) {
                    R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]);
                }
                break;
            case OpCode::RDIV:  // matrix right divide — output reuse skipped
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setScalarFast(asScalar(R[I.b]) / asScalar(R[I.c]));
                } else
                    R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]);
                break;
            case OpCode::ERDIV:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setScalarFast(asScalar(R[I.b]) / asScalar(R[I.c]));
                } else if (!tryInPlaceBinaryOp(R[I.a], OpCode::ERDIV, R[I.b], R[I.c])) {
                    R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]);
                }
                break;
            case OpCode::LDIV:
            case OpCode::ELDIV:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setScalarFast(asScalar(R[I.c]) / asScalar(R[I.b]));
                } else
                    R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]);
                break;
            case OpCode::POW:
            case OpCode::EPOW:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    double base = asScalar(R[I.b]);
                    double exp = asScalar(R[I.c]);
                    double result;
                    if (exp == 2.0)
                        result = base * base;
                    else if (exp == 3.0)
                        result = base * base * base;
                    else if (exp == 0.5)
                        result = std::sqrt(base);
                    else if (exp == -1.0)
                        result = 1.0 / base;
                    else
                        result = std::pow(base, exp);
                    R[I.a].setScalarFast(result);
                } else
                    R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]);
                break;

            // ── Scalar-specialized arithmetic (no type checks) ────
            case OpCode::ADD_SS:
                R[I.a].setScalarFast(R[I.b].scalarVal() + R[I.c].scalarVal());
                break;
            case OpCode::SUB_SS:
                R[I.a].setScalarFast(R[I.b].scalarVal() - R[I.c].scalarVal());
                break;
            case OpCode::MUL_SS:
                R[I.a].setScalarFast(R[I.b].scalarVal() * R[I.c].scalarVal());
                break;
            case OpCode::RDIV_SS:
                R[I.a].setScalarFast(R[I.b].scalarVal() / R[I.c].scalarVal());
                break;
            case OpCode::POW_SS: {
                double base = R[I.b].scalarVal();
                double exp = R[I.c].scalarVal();
                double result;
                if (exp == 2.0)
                    result = base * base;
                else if (exp == 3.0)
                    result = base * base * base;
                else if (exp == 0.5)
                    result = std::sqrt(base);
                else if (exp == -1.0)
                    result = 1.0 / base;
                else
                    result = std::pow(base, exp);
                R[I.a].setScalarFast(result);
                break;
            }
            case OpCode::NEG_S:
                R[I.a].setScalarFast(-R[I.b].scalarVal());
                break;

            // ── Comparison ───────────────────────────────────────
            case OpCode::EQ:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setLogicalFast(asScalar(R[I.b]) == asScalar(R[I.c]));
                } else
                    R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]);
                break;
            case OpCode::NE:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setLogicalFast(asScalar(R[I.b]) != asScalar(R[I.c]));
                } else
                    R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]);
                break;
            case OpCode::LT:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setLogicalFast(asScalar(R[I.b]) < asScalar(R[I.c]));
                } else
                    R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]);
                break;
            case OpCode::GT:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setLogicalFast(asScalar(R[I.b]) > asScalar(R[I.c]));
                } else
                    R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]);
                break;
            case OpCode::LE:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setLogicalFast(asScalar(R[I.b]) <= asScalar(R[I.c]));
                } else
                    R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]);
                break;
            case OpCode::GE:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setLogicalFast(asScalar(R[I.b]) >= asScalar(R[I.c]));
                } else
                    R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]);
                break;
            case OpCode::AND:
                if (R[I.b].isDoubleScalar() && R[I.c].isDoubleScalar()) {
                    R[I.a].setLogicalFast(
                        R[I.b].scalarVal() != 0.0 && R[I.c].scalarVal() != 0.0);
                } else
                    R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]);
                break;
            case OpCode::OR:
                if (R[I.b].isDoubleScalar() && R[I.c].isDoubleScalar()) {
                    R[I.a].setLogicalFast(
                        R[I.b].scalarVal() != 0.0 || R[I.c].scalarVal() != 0.0);
                } else
                    R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]);
                break;

            // ── Unary ────────────────────────────────────────────
            case OpCode::NEG:
                if (R[I.b].isDoubleScalar()) {
                    R[I.a].setScalarFast(-R[I.b].scalarVal());
                } else
                    R[I.a] = unarySlowPath(I.op, R[I.b]);
                break;
            case OpCode::UPLUS:
                R[I.a] = R[I.b];
                break;
            case OpCode::NOT:
                if (R[I.b].isDoubleScalar()) {
                    R[I.a].setLogicalFast(R[I.b].scalarVal() == 0.0);
                } else
                    R[I.a] = unarySlowPath(I.op, R[I.b]);
                break;
            case OpCode::CTRANSPOSE:
            case OpCode::TRANSPOSE:
                if (R[I.b].isDoubleScalar()) {
                    R[I.a] = R[I.b];
                } else
                    R[I.a] = unarySlowPath(I.op, R[I.b]);
                break;

            // ── Control flow ─────────────────────────────────────
            case OpCode::JMP:
                ip += I.d;
                continue;
            case OpCode::JMP_TRUE:
                if (R[I.a].toBool()) {
                    ip += I.d;
                    continue;
                }
                break;
            case OpCode::JMP_FALSE:
                if (!R[I.a].toBool()) {
                    ip += I.d;
                    continue;
                }
                break;

            // ── For-loop ─────────────────────────────────────────
            case OpCode::FOR_INIT: {
                ForState fs;
                fs.index = 0;
                if (R[I.b].isDoubleScalar()) {
                    fs.range = R[I.b];
                    fs.count = 1;
                    fs.rows = 0;
                    fs.data = nullptr;
                    fs.rangeType = ValueType::DOUBLE;
                } else if (R[I.b].isEmpty()) {
                    fs.count = 0;
                } else {
                    fs.range = R[I.b];
                    fs.rangeType = fs.range.type();
                    auto &dims = fs.range.dims();
                    fs.count = dims.cols();
                    fs.rows = dims.rows();
                    if (fs.rangeType == ValueType::DOUBLE)
                        fs.data = fs.range.doubleData();
                    else
                        fs.rawData = fs.range.rawData();
                }
                if (fs.count == 0) {
                    ip += I.d;
                    continue;
                }
                forStack_.push_back(std::move(fs));
                forSetVar(R[I.a], forStack_.back());
                break;
            }
            case OpCode::FOR_INIT_RANGE: {
                // Fused `for v = start:stop` / `for v = start:step:stop`.
                // No COLON allocation: start/step/count live in the
                // ForState, the loop var is recomputed as start + index*step
                // each iteration.
                const double start = R[I.b].toScalar();
                const double stop  = R[I.c].toScalar();
                const double step  = (I.e == 0xFF) ? 1.0
                                                   : R[I.e].toScalar();
                const size_t count = Value::colonCount(start, step, stop);
                if (count == 0) {
                    ip += I.d;
                    continue;
                }
                ForState fs;
                fs.index = 0;
                fs.count = count;
                fs.rows  = 1;
                fs.rangeType = ValueType::DOUBLE;
                fs.lazy = true;
                fs.lazyStart = start;
                fs.lazyStep  = step;
                forStack_.push_back(std::move(fs));
                forSetVar(R[I.a], forStack_.back());
                break;
            }
            case OpCode::FOR_NEXT: {
                auto &fs = forStack_.back();
                fs.index++;
                if (fs.index < fs.count) {
                    forSetVar(R[I.a], fs);
                    ip += I.d;
                    continue;
                }
                forStack_.pop_back();
                break;
            }

            // ── Colon ────────────────────────────────────────────
            case OpCode::COLON: {
                double start = R[I.b].toScalar(), stop = R[I.c].toScalar();
                R[I.a] = Value::colonRange(start, stop, &engine_.allocator_);
                break;
            }
            case OpCode::COLON3: {
                double start = R[I.b].toScalar(), step = R[I.c].toScalar(),
                       stop = R[I.e].toScalar();
                R[I.a] = Value::colonRange(start, step, stop, &engine_.allocator_);
                break;
            }

            // ── Array construction ───────────────────────────────
            case OpCode::HORZCAT:
                R[I.a] = Value::horzcat(&R[I.b], I.c, &engine_.allocator_);
                break;
            case OpCode::HORZCAT_APPEND: {
                // Specialised `dst = [dst, val]` — emitted by the
                // compiler when it sees `A = [A, x]` (the canonical
                // MATLAB grow-by-one anti-pattern). Routes through
                // appendScalar (geometric capacity → amortised O(1))
                // when dst is empty / row-vector heap double, val is a
                // real scalar, and dst's heap is uniquely owned.
                // Anything else falls back to the generic two-element
                // horzcat — same shape semantics as before, just slow.
                Value &dst = R[I.a];
                const Value &val = R[I.b];
                if (val.isScalar() && !val.isComplex()
                    && dst.heapRefCount() == 1
                    && (dst.isEmpty()
                        || (dst.isHeapDouble() && dst.dims().rows() == 1))) {
                    dst.appendScalar(val.toScalar(), &engine_.allocator_);
                    break;
                }
                Value elems[2] = { dst, val };
                R[I.a] = Value::horzcat(elems, 2, &engine_.allocator_);
                break;
            }
            case OpCode::VERTCAT:
                R[I.a] = Value::vertcat(&R[I.b], I.c, &engine_.allocator_);
                break;

            // ── Array indexing ───────────────────────────────────
            case OpCode::INDEX_GET: {
                const Value &mv = R[I.b];
                const Value &ix = R[I.c];
                if (mv.isCell()) {
                    // Cell () indexing always returns sub-cell
                    auto indices = Value::resolveIndices(ix, mv.numel());
                    R[I.a] = mv.indexGet(indices.data(), indices.size(),
                                         &engine_.allocator_);
                } else if (mv.isScalar() && ix.isDoubleScalar()) {
                    checkedIndex(ix.scalarVal(), 1); // validate bounds
                    R[I.a] = mv;
                } else if (ix.isDoubleScalar()) {
                    size_t i = checkedIndex(ix.scalarVal(), mv.numel());
                    R[I.a] = mv.elemAt(i, &engine_.allocator_);
                } else if (ix.isLogical()) {
                    R[I.a] = mv.logicalIndex(ix.logicalData(), ix.numel(),
                                             &engine_.allocator_);
                } else {
                    auto indices = Value::resolveIndices(ix, mv.numel());
                    R[I.a] = mv.indexGet(indices.data(), indices.size(), &engine_.allocator_);
                }
                break;
            }
            case OpCode::INDEX_GET_2D: {
                const Value &mv = R[I.b];
                // ── Scalar fast path: A(i,j) with scalar double indices ──
                if (R[I.c].isDoubleScalar() && R[I.e].isDoubleScalar()
                    && mv.isHeapDouble()) {
                    size_t r = static_cast<size_t>(R[I.c].scalarVal()) - 1;
                    size_t c = static_cast<size_t>(R[I.e].scalarVal()) - 1;
                    R[I.a].setScalarFast(mv.doubleDataFast()[mv.heapDims().sub2ind(r, c)]);
                    break;
                }
                auto rowIds = Value::resolveIndices(R[I.c], mv.dims().rows());
                auto colIds = Value::resolveIndices(R[I.e], mv.dims().cols());
                R[I.a] = mv.indexGet2D(rowIds.data(), rowIds.size(),
                                        colIds.data(), colIds.size(),
                                        &engine_.allocator_);
                break;
            }
            case OpCode::INDEX_SET: {
                const Value &ix = R[I.b];
                if (ix.isDoubleScalar() || ix.isLogicalScalar()) {
                    // Fast path: scalar index
                    size_t i = (size_t) ix.toScalar() - 1;
                    Value &dst = R[I.a];
                    const Value &rhs = R[I.c];

                    // Amortised O(1) grow-by-one: `A(end+1) = scalar` or
                    // `A(i) = scalar` where i == numel(A). Uses Value's
                    // appendScalar, which keeps a geometric capacity so
                    // the classic incremental-build loop runs in amortised
                    // O(1) instead of O(N) per iteration. Applies to
                    // empty / row-vector heap double targets with unique
                    // ownership only — everything else falls through to
                    // the generic ensureSize + elemSet path.
                    if (rhs.isScalar() && !rhs.isComplex()
                        && i == dst.numel()
                        && dst.heapRefCount() == 1
                        && (dst.isEmpty()
                            || (dst.isHeapDouble() && dst.dims().rows() == 1))) {
                        dst.appendScalar(rhs.toScalar(), &engine_.allocator_);
                        break;
                    }

                    if (dst.isEmpty() || dst.isScalar() || i >= dst.numel())
                        dst.ensureSize(i, &engine_.allocator_);
                    dst.elemSet(i, rhs);
                } else if (ix.isChar() && ix.numel() == 1 && ix.charData()[0] == ':') {
                    // Colon linear-assign: z(:) = rhs writes across every
                    // element of z without changing its shape. Four fast
                    // paths cover the common MATLAB patterns; everything
                    // else falls through to the generic indexSet.
                    Value &dst = R[I.a];
                    Value &rhs = R[I.c];   // mutable so the buffer-steal
                                            // fast path can absorb rhs's
                                            // heap when it's a unique temp
                    const size_t n = dst.numel();
                    const bool sameCount = (rhs.numel() == n);

                    // Scalar broadcast into heap double: tight std::fill.
                    if (rhs.isScalar() && !rhs.isComplex()
                        && dst.type() == ValueType::DOUBLE && dst.isHeapDouble()) {
                        const double v = rhs.toScalar();
                        double *d = dst.doubleDataMut();
                        std::fill_n(d, n, v);
                        break;
                    }

                    // Buffer-steal fast path. When rhs is a uniquely-owned
                    // heap value matching dst's shape (typically a freshly
                    // computed temp from `sin(x)`, `x + y`, etc.), swap
                    // their data buffers in place — dst keeps its dims,
                    // rhs gets the old buffer which is freed on its next
                    // overwrite. Skips the O(N) memcpy that would otherwise
                    // copy rhs into dst. Saves ~2 ms per call at N=1M.
                    if (sameCount
                        && dst.hasHeap() && rhs.hasHeap()
                        && dst.heapRefCount() == 1 && rhs.heapRefCount() == 1
                        && dst.type() == rhs.type()
                        && (dst.type() == ValueType::DOUBLE
                            || dst.type() == ValueType::COMPLEX)
                        && dst.isComplex() == rhs.isComplex()) {
                        dst.swapHeapBufferUnchecked(rhs);
                        break;
                    }

                    // double → double, matching count: bulk memcpy.
                    if (sameCount
                        && dst.type() == ValueType::DOUBLE && dst.isHeapDouble()
                        && rhs.type() == ValueType::DOUBLE && !rhs.isComplex()) {
                        std::memcpy(dst.doubleDataMut(),
                                    rhs.doubleData(),
                                    n * sizeof(double));
                        break;
                    }

                    // complex → complex, matching count: bulk memcpy.
                    if (sameCount && dst.isComplex() && rhs.isComplex()) {
                        std::memcpy(dst.complexDataMut(),
                                    rhs.complexData(),
                                    n * sizeof(Complex));
                        break;
                    }

                    // Generic fallback — handles type promotion
                    // (double→complex), complex→double, logical/char,
                    // and raises the size-mismatch error for bad rhs.
                    auto indices = Value::resolveIndices(ix, n);
                    dst.indexSet(indices.data(), indices.size(), rhs);
                } else {
                    // Vector or logical index
                    auto indices = Value::resolveIndicesUnchecked(ix);
                    // Auto-grow if needed
                    size_t maxIdx = 0;
                    for (size_t idx : indices) maxIdx = std::max(maxIdx, idx);
                    if (R[I.a].isEmpty() || maxIdx >= R[I.a].numel())
                        R[I.a].ensureSize(maxIdx, &engine_.allocator_);
                    R[I.a].indexSet(indices.data(), indices.size(), R[I.c]);
                }
                break;
            }
            case OpCode::INDEX_SET_2D: {
                const Value &ri = R[I.b];
                const Value &ci = R[I.c];
                const Value &val = R[I.e];

                // ── Scalar fast path: Z(i,j) = scalar ──
                if (ri.isDoubleScalar() && ci.isDoubleScalar()
                    && val.isDoubleScalar() && R[I.a].isHeapDouble()) {
                    size_t r = static_cast<size_t>(ri.scalarVal()) - 1;
                    size_t c = static_cast<size_t>(ci.scalarVal()) - 1;
                    const auto &d = R[I.a].heapDims();
                    if (r < d.rows() && c < d.cols()) {
                        // In-bounds: direct write, skip detach overhead
                        R[I.a].doubleDataMutFast()[d.sub2ind(r, c)] = val.scalarVal();
                        break;
                    }
                    // Out-of-bounds: grow then write
                    size_t newR = std::max(d.rows(), r + 1);
                    size_t newC = std::max(d.cols(), c + 1);
                    R[I.a].resize(newR, newC, &engine_.allocator_);
                    R[I.a].doubleDataMutFast()[R[I.a].heapDims().sub2ind(r, c)] = val.scalarVal();
                    break;
                }

                bool riIsColon = ri.isChar() && ri.numel() == 1 && ri.charData()[0] == ':';
                bool ciIsColon = ci.isChar() && ci.numel() == 1 && ci.charData()[0] == ':';
                // Resolve without bounds check — auto-expand may be needed
                auto rowIds = riIsColon ? std::vector<size_t>()
                                        : Value::resolveIndicesUnchecked(ri);
                auto colIds = ciIsColon ? std::vector<size_t>()
                                        : Value::resolveIndicesUnchecked(ci);

                // Grow if needed
                size_t maxR = R[I.a].dims().rows(), maxC = R[I.a].dims().cols();
                for (size_t r : rowIds) maxR = std::max(maxR, r + 1);
                for (size_t c : colIds) maxC = std::max(maxC, c + 1);
                if (maxR > R[I.a].dims().rows() || maxC > R[I.a].dims().cols())
                    R[I.a].resize(maxR, maxC, &engine_.allocator_);

                // Resolve colon-all after resize (needs final dims)
                if (riIsColon) rowIds = Value::resolveIndices(ri, R[I.a].dims().rows());
                if (ciIsColon) colIds = Value::resolveIndices(ci, R[I.a].dims().cols());

                R[I.a].indexSet2D(rowIds.data(), rowIds.size(),
                                  colIds.data(), colIds.size(), val);
                break;
            }

            // ── ND array/cell indexing (3D+) ─────────────────────
            case OpCode::INDEX_GET_ND: {
                // a=dst, b=arr/cell, c=base, e=ndims
                uint8_t base = I.c, ndims = I.e;
                const Value &mv = R[I.b];
                if (ndims == 3) {
                    auto rowIds = Value::resolveIndices(R[base], mv.dims().rows());
                    auto colIds = Value::resolveIndices(R[base + 1], mv.dims().cols());
                    auto pageIds = Value::resolveIndices(R[base + 2], mv.dims().pages());
                    R[I.a] = mv.indexGet3D(rowIds.data(), rowIds.size(),
                                           colIds.data(), colIds.size(),
                                           pageIds.data(), pageIds.size(),
                                           &engine_.allocator_);
                } else {
                    // ND read (≥4): CELL handled by indexGetND directly now.
                    const int nd = static_cast<int>(ndims);
                    std::vector<std::vector<size_t>> idxLists(nd);
                    std::vector<const size_t *> idxPtrs(nd);
                    std::vector<size_t> idxCounts(nd);
                    for (int i = 0; i < nd; ++i) {
                        const size_t lim = (i < mv.dims().ndim()) ? mv.dims().dim(i) : 1;
                        idxLists[i] = Value::resolveIndices(R[base + i], lim);
                        idxPtrs[i] = idxLists[i].data();
                        idxCounts[i] = idxLists[i].size();
                    }
                    R[I.a] = mv.indexGetND(idxPtrs.data(), idxCounts.data(), nd,
                                           &engine_.allocator_);
                }
                break;
            }
            case OpCode::INDEX_SET_ND: {
                // a=arr/cell, b=base, c=ndims, e=val
                uint8_t base = I.b, ndims = I.c;
                if (ndims == 3) {
                    auto isColon = [](const Value &v) {
                        return v.isChar() && v.numel() == 1 && v.charData()[0] == ':';
                    };
                    bool riColon = isColon(R[base]);
                    bool ciColon = isColon(R[base + 1]);
                    bool piColon = isColon(R[base + 2]);

                    // Resolve non-colon indices (unchecked for auto-expand)
                    auto rowIds = riColon ? std::vector<size_t>()
                                          : Value::resolveIndicesUnchecked(R[base]);
                    auto colIds = ciColon ? std::vector<size_t>()
                                          : Value::resolveIndicesUnchecked(R[base + 1]);
                    auto pageIds = piColon ? std::vector<size_t>()
                                           : Value::resolveIndicesUnchecked(R[base + 2]);

                    // Grow if needed
                    size_t maxR = R[I.a].dims().rows();
                    size_t maxC = R[I.a].dims().cols();
                    size_t maxP = R[I.a].dims().pages();
                    for (size_t r : rowIds) maxR = std::max(maxR, r + 1);
                    for (size_t c : colIds) maxC = std::max(maxC, c + 1);
                    for (size_t p : pageIds) maxP = std::max(maxP, p + 1);
                    if (maxR > R[I.a].dims().rows() || maxC > R[I.a].dims().cols()
                        || maxP > R[I.a].dims().pages())
                        R[I.a].resize3d(maxR, maxC, maxP, &engine_.allocator_);

                    // Resolve colon-all after resize (needs final dims)
                    if (riColon) rowIds = Value::resolveIndices(R[base], R[I.a].dims().rows());
                    if (ciColon) colIds = Value::resolveIndices(R[base + 1], R[I.a].dims().cols());
                    if (piColon) pageIds = Value::resolveIndices(R[base + 2], R[I.a].dims().pages());

                    R[I.a].indexSet3D(rowIds.data(), rowIds.size(),
                                      colIds.data(), colIds.size(),
                                      pageIds.data(), pageIds.size(),
                                      R[I.e]);
                } else {
                    // ND write (≥4): grow the target *first* so the
                    // subsequent resolveIndices bounds checks succeed.
                    const int nd = static_cast<int>(ndims);
                    const int curNd = R[I.a].dims().ndim();
                    const int newNd = std::max(nd, curNd);
                    std::vector<size_t> need(newNd, 1);
                    for (int i = 0; i < newNd; ++i)
                        need[i] = (i < curNd) ? R[I.a].dims().dim(i) : 1;
                    auto isColon = [](const Value &v) {
                        return v.isChar() && v.numel() == 1 && v.charData()[0] == ':';
                    };
                    for (int i = 0; i < nd; ++i) {
                        const Value &iv = R[base + i];
                        if (isColon(iv) || iv.isLogical()) continue;
                        if (iv.isDoubleScalar()) {
                            size_t v = static_cast<size_t>(iv.toScalar());
                            if (v > need[i]) need[i] = v;
                        } else if (iv.type() == ValueType::DOUBLE) {
                            const double *d = iv.doubleData();
                            for (size_t k = 0; k < iv.numel(); ++k) {
                                size_t v = static_cast<size_t>(d[k]);
                                if (v > need[i]) need[i] = v;
                            }
                        }
                    }
                    bool grow = (newNd > curNd);
                    for (int i = 0; i < curNd && !grow; ++i)
                        if (need[i] > R[I.a].dims().dim(i)) grow = true;
                    if (grow)
                        R[I.a].resizeND(need.data(), newNd, &engine_.allocator_);

                    std::vector<std::vector<size_t>> idxLists(nd);
                    std::vector<const size_t *> idxPtrs(nd);
                    std::vector<size_t> idxCounts(nd);
                    for (int i = 0; i < nd; ++i) {
                        const size_t lim = (i < R[I.a].dims().ndim()) ? R[I.a].dims().dim(i) : 1;
                        idxLists[i] = Value::resolveIndices(R[base + i], lim);
                        idxPtrs[i] = idxLists[i].data();
                        idxCounts[i] = idxLists[i].size();
                    }
                    R[I.a].indexSetND(idxPtrs.data(), idxCounts.data(), nd, R[I.e]);
                }
                break;
            }

            // ── Index delete (v(idx) = []) ──────────────────────
            case OpCode::INDEX_DELETE: {
                // a=arr, b=idx
                auto indices = Value::resolveIndicesUnchecked(R[I.b]);
                R[I.a].indexDelete(indices.data(), indices.size(), &engine_.allocator_);
                break;
            }
            case OpCode::INDEX_DELETE_2D: {
                // a=arr, b=row, c=col
                size_t Rows = R[I.a].dims().rows(), Cols = R[I.a].dims().cols();
                auto rowIdx = Value::resolveIndices(R[I.b], Rows);
                auto colIdx = Value::resolveIndices(R[I.c], Cols);
                R[I.a].indexDelete2D(rowIdx.data(), rowIdx.size(),
                                     colIdx.data(), colIdx.size(),
                                     &engine_.allocator_);
                break;
            }

            case OpCode::INDEX_DELETE_ND: {
                // a=arr, b=base, c=ndims
                uint8_t base = I.b, ndims = I.c;
                std::vector<std::vector<size_t>> perDim(ndims);
                std::vector<const size_t *> perDimPtrs(ndims);
                std::vector<size_t> perDimCount(ndims);
                const auto &srcDims = R[I.a].dims();
                const int srcNd = srcDims.ndim();
                for (uint8_t i = 0; i < ndims; ++i) {
                    const size_t lim = (i < srcNd) ? srcDims.dim(i) : 1;
                    perDim[i] = Value::resolveIndices(R[base + i], lim);
                    perDimPtrs[i]  = perDim[i].data();
                    perDimCount[i] = perDim[i].size();
                }
                R[I.a].indexDeleteND(perDimPtrs.data(), perDimCount.data(),
                                     ndims, &engine_.allocator_);
                break;
            }

            // ── Struct field access ──────────────────────────────
            case OpCode::FIELD_GET: {
                // a=dst, b=obj, d=nameIdx — strict: throws if field missing
                const std::string &fname = chunk.strings[I.d];
                if (!R[I.b].isStruct())
                    throw std::runtime_error("Dot indexing requires a struct");
                if (!R[I.b].hasField(fname))
                    throw std::runtime_error("Reference to non-existent field '" + fname + "'");
                R[I.a] = R[I.b].field(fname);
                break;
            }
            case OpCode::FIELD_GET_OR_CREATE: {
                // a=dst, b=obj, d=nameIdx — lvalue: auto-creates struct and field
                const std::string &fname = chunk.strings[I.d];
                if (R[I.b].isEmpty())
                    R[I.b] = Value::structure();
                if (!R[I.b].isStruct())
                    throw std::runtime_error("Dot indexing requires a struct");
                R[I.a] = R[I.b].field(fname); // field() auto-creates if missing
                break;
            }
            case OpCode::FIELD_SET: {
                // a=obj, b=val, d=nameIdx
                const std::string &fname = chunk.strings[I.d];
                // Auto-create struct if empty
                if (R[I.a].isEmpty()) {
                    R[I.a] = Value::structure();
                }
                if (!R[I.a].isStruct())
                    throw std::runtime_error("Dot indexing requires a struct");
                R[I.a].field(fname) = R[I.b];
                break;
            }
            case OpCode::FIELD_GET_DYN: {
                // a=dst, b=obj, c=nameReg — s.(R[nameReg])
                std::string fname = R[I.c].toString();
                if (!R[I.b].isStruct())
                    throw std::runtime_error("Dot indexing requires a struct");
                if (!R[I.b].hasField(fname))
                    throw std::runtime_error("Reference to non-existent field '" + fname + "'");
                R[I.a] = R[I.b].field(fname);
                break;
            }
            case OpCode::FIELD_SET_DYN: {
                // a=obj, b=nameReg, c=val — s.(R[nameReg]) = R[val]
                std::string fname = R[I.b].toString();
                if (R[I.a].isEmpty())
                    R[I.a] = Value::structure();
                if (!R[I.a].isStruct())
                    throw std::runtime_error("Dot indexing requires a struct");
                R[I.a].field(fname) = R[I.c];
                break;
            }

            // ── Cell operations ──────────────────────────────────
            case OpCode::CELL_LITERAL: {
                uint8_t base = I.b, count = I.c;
                auto cell = Value::cell(1, count);
                for (uint8_t i = 0; i < count; ++i)
                    cell.cellAt(i) = R[base + i];
                R[I.a] = std::move(cell);
                break;
            }
            case OpCode::CELL_GET: {
                if (!R[I.b].isCell())
                    throw std::runtime_error("Cell indexing requires a cell array");
                size_t i = (size_t) R[I.c].toScalar() - 1;
                R[I.a] = R[I.b].cellAt(i);
                break;
            }
            case OpCode::CELL_GET_2D: {
                if (!R[I.b].isCell())
                    throw std::runtime_error("Cell indexing requires a cell array");
                size_t r = (size_t) R[I.c].toScalar() - 1, c = (size_t) R[I.e].toScalar() - 1;
                R[I.a] = R[I.b].cellAt(R[I.b].dims().sub2ind(r, c));
                break;
            }
            case OpCode::CELL_GET_MULTI: {
                // a=outBase, b=cell, c=idx, e=nout
                if (!R[I.b].isCell())
                    throw std::runtime_error("Cell indexing requires a cell array");
                auto indices = Value::resolveIndices(R[I.c], R[I.b].numel());
                uint8_t outBase = I.a, nout = I.e;
                for (uint8_t i = 0; i < nout && i < indices.size(); ++i)
                    R[outBase + i] = R[I.b].cellAt(indices[i]);
                break;
            }
            case OpCode::CELL_SET: {
                if (R[I.a].isEmpty())
                    R[I.a] = Value::cell(0, 0);
                if (!R[I.a].isCell())
                    throw std::runtime_error("Cell indexing requires a cell array");
                size_t i = (size_t) R[I.b].toScalar() - 1;
                if (i >= R[I.a].numel()) {
                    size_t ns = i + 1;
                    auto nc = Value::cell(1, ns);
                    for (size_t k = 0; k < R[I.a].numel(); ++k)
                        nc.cellAt(k) = R[I.a].cellAt(k);
                    R[I.a] = std::move(nc);
                }
                R[I.a].cellAt(i) = R[I.c];
                break;
            }
            case OpCode::CELL_SET_2D: {
                if (R[I.a].isEmpty())
                    R[I.a] = Value::cell(0, 0);
                if (!R[I.a].isCell())
                    throw std::runtime_error("Cell indexing requires a cell array");
                size_t r = (size_t) R[I.b].toScalar() - 1, c = (size_t) R[I.c].toScalar() - 1;
                // Auto-grow if needed
                size_t nr = R[I.a].dims().rows(), nc = R[I.a].dims().cols();
                if (r + 1 > nr || c + 1 > nc) {
                    size_t newR = std::max(nr, r + 1), newC = std::max(nc, c + 1);
                    auto grown = Value::cell(newR, newC);
                    for (size_t cc = 0; cc < nc; ++cc)
                        for (size_t rr = 0; rr < nr; ++rr)
                            grown.cellAt(cc * newR + rr) = R[I.a].cellAt(cc * nr + rr);
                    R[I.a] = std::move(grown);
                }
                R[I.a].cellAt(R[I.a].dims().sub2ind(r, c)) = R[I.e];
                break;
            }
            case OpCode::CELL_GET_ND: {
                // a=dst, b=cell, c=base, e=ndims
                if (!R[I.b].isCell())
                    throw std::runtime_error("Cell indexing requires a cell array");
                uint8_t base = I.c, ndims = I.e;
                if (ndims == 3) {
                    size_t r = (size_t) R[base].toScalar() - 1;
                    size_t c = (size_t) R[base + 1].toScalar() - 1;
                    size_t p = (size_t) R[base + 2].toScalar() - 1;
                    size_t idx = R[I.b].dims().sub2ind(r, c, p);
                    R[I.a] = R[I.b].cellAt(idx);
                } else {
                    // General ND (≥4): column-major linear index from
                    // per-axis subscripts using actual dim(i).
                    const auto &d = R[I.b].dims();
                    size_t idx = 0, stride = 1;
                    for (uint8_t i = 0; i < ndims; ++i) {
                        size_t si = (size_t) R[base + i].toScalar() - 1;
                        idx += si * stride;
                        stride *= d.dim(i);
                    }
                    R[I.a] = R[I.b].cellAt(idx);
                }
                break;
            }
            case OpCode::CELL_SET_ND: {
                // a=cell, b=base, c=ndims, e=val
                if (R[I.a].isEmpty())
                    R[I.a] = Value::cell(0, 0);
                if (!R[I.a].isCell())
                    throw std::runtime_error("Cell indexing requires a cell array");
                uint8_t base = I.b, ndims = I.c;
                if (ndims == 3) {
                    size_t r = (size_t) R[base].toScalar() - 1;
                    size_t c = (size_t) R[base + 1].toScalar() - 1;
                    size_t p = (size_t) R[base + 2].toScalar() - 1;
                    // Auto-grow if needed
                    size_t nr = R[I.a].dims().rows(), nc = R[I.a].dims().cols(), np = R[I.a].dims().pages();
                    if (r + 1 > nr || c + 1 > nc || p + 1 > np) {
                        size_t newR = std::max(nr, r + 1);
                        size_t newC = std::max(nc, c + 1);
                        size_t newP = std::max(np, p + 1);
                        auto grown = Value::cell3D(newR, newC, newP);
                        // Copy existing elements
                        for (size_t pp = 0; pp < np; ++pp)
                            for (size_t cc = 0; cc < nc; ++cc)
                                for (size_t rr = 0; rr < nr; ++rr) {
                                    size_t oldIdx = rr + cc * nr + pp * nr * nc;
                                    size_t newIdx = rr + cc * newR + pp * newR * newC;
                                    grown.cellAt(newIdx) = R[I.a].cellAt(oldIdx);
                                }
                        R[I.a] = std::move(grown);
                    }
                    size_t idx = R[I.a].dims().sub2ind(r, c, p);
                    R[I.a].cellAt(idx) = R[I.e];
                } else {
                    // General ND (≥4): auto-grow on out-of-range subscripts
                    // (rank ↑ for new trailing axes, axis-size ↑ otherwise),
                    // then column-major linear-index assign.
                    std::vector<size_t> coords(ndims);
                    for (uint8_t i = 0; i < ndims; ++i)
                        coords[i] = (size_t) R[base + i].toScalar() - 1;
                    const int curNd = R[I.a].dims().ndim();
                    const int newNd = std::max(static_cast<int>(ndims), curNd);
                    std::vector<size_t> need(newNd, 1);
                    for (int i = 0; i < newNd; ++i)
                        need[i] = (i < curNd) ? R[I.a].dims().dim(i) : 1;
                    bool grow = (static_cast<int>(ndims) > curNd);
                    for (uint8_t i = 0; i < ndims; ++i) {
                        if (coords[i] + 1 > need[i]) {
                            need[i] = coords[i] + 1;
                            grow = true;
                        }
                    }
                    if (grow)
                        R[I.a].resizeND(need.data(), newNd, &engine_.allocator_);
                    const auto &d = R[I.a].dims();
                    size_t idx = 0, stride = 1;
                    for (uint8_t i = 0; i < ndims; ++i) {
                        idx += coords[i] * stride;
                        stride *= d.dim(i);
                    }
                    R[I.a].cellAt(idx) = R[I.e];
                }
                break;
            }

            // ── Inline scalar builtins ───────────────────────────
            case OpCode::CALL_BUILTIN:
                execCallBuiltin(I, R);
                break;

            // ── General function calls ───────────────────────────
            case OpCode::CALL: {
                uint8_t argBase = I.b, na = I.c;
                uint8_t nargout_val = I.e; // 0=statement, 1=expression
                int16_t funcIdx = I.d;

                // Try resolved cache first
                const BytecodeChunk *targetChunk = nullptr;
                if (funcIdx < (int16_t) resolvedFuncs.size() && resolvedFuncs[funcIdx])
                    targetChunk = resolvedFuncs[funcIdx];

                if (!targetChunk) {
                    const std::string &funcName = chunk.strings[funcIdx];
                    const BytecodeChunk *found = findCompiledFunc(funcName);
                    if (found) {
                        if (funcIdx >= (int16_t) resolvedFuncs.size())
                            resolvedFuncs.resize(funcIdx + 1, nullptr);
                        resolvedFuncs[funcIdx] = found;
                        targetChunk = found;
                    }
                }

                if (targetChunk) {
                    // User function — push frame and enter
                    frame.ip = ip + 1;
                    pushCallFrame(*targetChunk, &R[argBase], na, I.a, nargout_val);
                    goto enter_frame;
                }

                // External function — call directly (no frame push)
                {
                    const std::string &funcName = chunk.strings[funcIdx];
                    auto extIt = engine_.externalFuncs_.find(funcName);
                    if (extIt != engine_.externalFuncs_.end()) {
                        Span<const Value> as(&R[argBase], na);
                        Value ob[1];
                        // Output-reuse hint: when no argument register
                        // aliases the destination (so reading the args
                        // can't observe a moved-out R[I.a]), hand the
                        // destination's current value to the adapter
                        // via outs[0]. Adapters that opt in (the
                        // NK_UNARY_ADAPTER_HINT macro for abs/sin/cos/
                        // exp/log) check for a uniquely-owned heap
                        // double of matching shape and write straight
                        // into its buffer instead of allocating fresh.
                        // Adapters that don't opt in just overwrite
                        // outs[0] — same observable behaviour, the
                        // moved-out heap is freed when ob[0] is
                        // overwritten.
                        bool canHint = R[I.a].hasHeap();
                        for (uint8_t i = 0; i < na && canHint; ++i)
                            if (&R[argBase + i] == &R[I.a])
                                canHint = false;
                        if (canHint)
                            ob[0] = std::move(R[I.a]);
                        Span<Value> os(ob, 1);
                        CallContext ctx{&engine_, &engine_.workspaceEnv()};
                        extIt->second(as, nargout_val, os, ctx);
                        R[I.a] = std::move(ob[0]);
                        break;
                    }
                    throw std::runtime_error("VM: undefined function '" + funcName + "'");
                }
            }

            // ── Multi-return function call ──────────────────────
            case OpCode::CALL_MULTI: {
                // a=outBase, b=argBase, c=nargs, d=funcIdx, e=nout
                uint8_t outBase = I.a, argBase = I.b, na = I.c, nout = I.e;
                int16_t funcIdx = I.d;
                const std::string &funcName = chunk.strings[funcIdx];

                // Try compiled user function
                if (const BytecodeChunk *found = findCompiledFunc(funcName)) {
                    frame.ip = ip + 1;
                    returnCount_ = 0;
                    pushCallFrame(*found, &R[argBase], na,
                                  0, nout, true, outBase, nout);
                    goto enter_frame;
                }
                // External function with nout — call directly
                {
                    auto extIt = engine_.externalFuncs_.find(funcName);
                    if (extIt != engine_.externalFuncs_.end()) {
                        std::vector<Value> outBuf(nout);
                        Span<const Value> as(&R[argBase], na);
                        Span<Value> os(outBuf.data(), nout);
                        CallContext ctx{&engine_, &engine_.workspaceEnv()};
                        extIt->second(as, nout, os, ctx);
                        for (size_t i = 0; i < nout; ++i)
                            R[outBase + i] = std::move(outBuf[i]);
                        break;
                    }
                    throw std::runtime_error("VM: undefined function '" + funcName + "'");
                }
            }

            // ── Indirect function call (func handle) or array indexing ─
            case OpCode::CALL_INDIRECT:
                if (execCallIndirect(I, R, frame, ip))
                    goto enter_frame;
                break;

            // ── Display ──────────────────────────────────────────
            case OpCode::DISPLAY:
                execDisplay(I, R, chunk);
                break;

            // ── Return ───────────────────────────────────────────
            case OpCode::RET:
                popCallFrame(R[I.a]);
                goto enter_frame;
            case OpCode::RET_MULTI: {
                // a=base, b=count — store return values in returnBuf_
                uint8_t base = I.a, count = I.b;
                returnCount_ = count;
                for (uint8_t i = 0; i < count && i < kMaxReturns; ++i)
                    returnBuf_[i] = R[base + i];
                popCallFrame(R[base]); // first value as primary return
                goto enter_frame;
            }
            case OpCode::RET_EMPTY:
                popCallFrame(Value::empty());
                goto enter_frame;
            case OpCode::HALT:
                popCallFrame(Value::empty());
                goto enter_frame;
            case OpCode::NOP:
                if (I.a == 1 && !forStack_.empty())
                    forStack_.pop_back(); // break from for-loop
                break;

            case OpCode::ASSERT_DEF:
                if (R[I.a].isUnset() || R[I.a].isDeleted()) {
                    // Fallback: check dynamic variables (debug eval, runtime eval)
                    if (frame.dynVars) {
                        auto it = frame.dynVars->find(chunk.strings[I.d]);
                        if (it != frame.dynVars->end() && !it->second.isDeleted()) {
                            R[I.a] = it->second;
                            break;
                        }
                    }
                    const std::string &n = chunk.strings[I.d];
                    if (n == "nargin" || n == "nargout")
                        throw std::runtime_error(
                            "You can only call nargin/nargout from within a MATLAB function.");
                    throw std::runtime_error("Undefined function or variable '" + n + "'");
                }
                break;

            case OpCode::CLEAR_VAR:
                R[I.a] = Value::deleted();
                break;

            case OpCode::CLEAR_DYN: {
                std::string varName = R[I.a].toString();
                for (auto &[vn, reg] : chunk.varMap) {
                    if (vn == varName && reg < chunk.numRegisters) {
                        R[reg] = Value::deleted();
                        break;
                    }
                }
                break;
            }

            case OpCode::EXIST_VAR: {
                // a=dst, b=nameReg, c=filterReg (0 = no filter)
                std::string varName = R[I.b].toString();
                std::string filter;
                if (I.c != 0 && !R[I.c].isEmpty())
                    filter = R[I.c].toString();

                double code = 0;

                if (filter.empty()) {
                    // No filter: check variables first, then functions
                    for (auto &[vn, reg] : chunk.varMap) {
                        if (vn == varName && reg < chunk.numRegisters) {
                            if (!R[reg].isUnset() && !R[reg].isDeleted())
                                code = 1;
                            break;
                        }
                    }
                    if (code == 0 && engine_.hasFunction(varName))
                        code = 5;
                } else if (filter == "var") {
                    // Only check local variables
                    for (auto &[vn, reg] : chunk.varMap) {
                        if (vn == varName && reg < chunk.numRegisters) {
                            if (!R[reg].isUnset() && !R[reg].isDeleted())
                                code = 1;
                            break;
                        }
                    }
                } else if (filter == "builtin") {
                    if (engine_.hasExternalFunction(varName))
                        code = 5;
                } else if (filter == "file" || filter == "dir" || filter == "class") {
                    // Not supported — return 0
                    if (engine_.outputFunc_)
                        engine_.outputFunc_("Warning: exist(name, '" + filter
                                            + "') is not yet supported.\n");
                }

                R[I.a] = Value::scalar(code, &engine_.allocator_);
                break;
            }

            case OpCode::WHO:
                execWho(I, R, chunk);
                break;

            case OpCode::WHOS:
                execWhos(I, R, chunk);
                break;

            // ── Try/catch ────────────────────────────────────────
            case OpCode::TRY_BEGIN: {
                // I.d = offset to catch block, I.a = exception register
                TryHandler th;
                th.catchIp = ip + I.d;
                th.exReg = I.a;
                th.forStackSize = forStack_.size();
                th.frameIndex = frames_.size() - 1;
                tryStack_.push_back(th);
                break;
            }
            case OpCode::TRY_END:
                if (!tryStack_.empty())
                    tryStack_.pop_back();
                break;

            case OpCode::THROW: {
                // a = register containing error message (string or struct)
                if (R[I.a].isChar() || R[I.a].isString())
                    throw Error(R[I.a].toString());
                if (R[I.a].isStruct()) {
                    std::string msg = R[I.a].hasField("message")
                                          ? R[I.a].field("message").toString()
                                          : "User error";
                    std::string id = R[I.a].hasField("identifier")
                                         ? R[I.a].field("identifier").toString()
                                         : "";
                    throw Error(msg, 0, 0, "", "", id);
                }
                throw Error("User error");
            }

            default:
                throw std::runtime_error("VM: unimplemented opcode " + std::to_string((int) I.op));

            } // switch
            ++ip;
        } // while
    } catch (const DebugStopException &) {
        throw; // pass through — not a user error
    } catch (Error &mle) {
        std::string id = mle.identifier().empty() ? "m:error" : mle.identifier();
        if (dispatchTryCatch(mle.what(), id.c_str()))
            goto enter_frame;
        // Enrich with current instruction's source location if missing
        // (e.g. Error thrown from a public C++ library API that didn't
        // know its call site).
        size_t instrIdx = static_cast<size_t>(ip - chunk.code.data());
        if (instrIdx < chunk.sourceMap.size() && chunk.sourceMap[instrIdx].line > 0) {
            mle.attachIfMissing(chunk.sourceMap[instrIdx].line,
                                chunk.sourceMap[instrIdx].col,
                                chunk.name,
                                describeInstruction(*ip, chunk));
        }
        throw;
    } catch (const std::exception &ex) {
        if (dispatchTryCatch(ex.what(), "m:error"))
            goto enter_frame;
        enrichAndThrow(ex, ip, chunk);
    }

    // Fell off end of bytecode — implicit return empty
    popCallFrame(Value::empty());
    goto enter_frame;
    } // scope for frame locals
}

// ============================================================
// Variable export from top-level frame
// ============================================================

void VM::exportTopLevelVariables()
{
    if (frames_.empty())
        return;

    CallFrame &topFrame = frames_[0];
    lastVarMap_.clear();

    // Export only names the chunk actually wrote to — reading `pi` or any
    // other reserved name must not create a shadowing local in the base
    // workspace. assignedVars is the compile-time set of write targets.
    const auto &assigned = topFrame.chunk->assignedVars;
    for (auto &[name, reg] : topFrame.chunk->varMap) {
        if (reg < topFrame.nregs && assigned.count(name))
            lastVarMap_.push_back({name, topFrame.R[reg]});
    }

    // Export global declarations to globalsEnv (same logic as popCallFrame top-level path)
    for (auto &gname : topFrame.chunk->globalNames) {
        for (auto &[vname, reg] : topFrame.chunk->varMap) {
            if (vname == gname && reg < topFrame.nregs) {
                if (!topFrame.R[reg].isUnset() && !topFrame.R[reg].isDeleted())
                    engine_.globalsEnv_->set(gname, topFrame.R[reg]);
                Value *gsVal = engine_.globalsEnv_->get(gname);
                if (gsVal)
                    engine_.workspaceEnv_->set(gname, *gsVal);
                break;
            }
        }
    }
}

// ============================================================
// Exception helpers
// ============================================================

bool VM::dispatchTryCatch(const char *msg, const char *identifier)
{
    if (tryStack_.empty())
        return false;

    TryHandler th = tryStack_.back();
    tryStack_.pop_back();

    // Unwind call frames to the handler's frame (exception may propagate across calls)
    while (frames_.size() > th.frameIndex + 1) {
        CallFrame &f = frames_.back();
        // Pop debug frame
        if (auto *ctl = debugCtl())
            ctl->popFrame();
        // Remove try handlers from this frame
        while (!tryStack_.empty() && tryStack_.back().frameIndex >= frames_.size() - 1)
            tryStack_.pop_back();
        // Trim for-loop stack
        forStack_.resize(std::min(forStack_.size(), f.forStackBase));
        // Cleanup registers
        for (uint8_t i = 0; i < f.nregs; ++i)
            if (!f.R[i].isDoubleScalar() && !f.R[i].isEmpty())
                f.R[i] = Value::empty();
        regStackTop_ = f.regBase;
        frames_.pop_back();
    }

    // Restore for-loop stack to the state at TRY_BEGIN
    if (forStack_.size() > th.forStackSize)
        forStack_.resize(th.forStackSize);

    // Set up catch context in the handler's frame
    CallFrame &frame = frames_.back();
    R_ = frame.R;

    Value err = Value::structure();
    err.field("message") = Value::fromString(msg, &engine_.allocator_);
    err.field("identifier") = Value::fromString(identifier, &engine_.allocator_);
    frame.R[th.exReg] = std::move(err);
    frame.ip = th.catchIp;
    return true;
}

// Derive error context description from the opcode + operands at throw time.
// This is the VM equivalent of TreeWalker's describeNode() — zero storage overhead,
// only runs on the error path.
static std::string describeInstruction(const Instruction &instr,
                                       const BytecodeChunk &chunk)
{
    switch (instr.op) {
    // Function calls
    case OpCode::CALL:
    case OpCode::CALL_MULTI: {
        int16_t funcIdx = instr.d;
        if (funcIdx >= 0 && funcIdx < (int16_t)chunk.strings.size())
            return "in call to '" + chunk.strings[funcIdx] + "'";
        return "in function call";
    }
    case OpCode::CALL_BUILTIN: {
        static const char *bn[] = {
            "abs",  "floor", "ceil", "round", "fix",  "sqrt", "exp",  "log",
            "log2", "log10", "sin",  "cos",   "tan",  "sign", "isnan","isinf",
            nullptr,nullptr, nullptr,nullptr,  "mod",  "rem",  "max",  "min",
            "pow",  "atan2"
        };
        int16_t bid = instr.d;
        if (bid >= 0 && bid < 26 && bn[bid])
            return std::string("in call to '") + bn[bid] + "'";
        return "in builtin call";
    }
    case OpCode::CALL_INDIRECT:
        return "in function call";

    // Cell indexing
    case OpCode::CELL_GET:
    case OpCode::CELL_SET:
    case OpCode::CELL_GET_2D:
    case OpCode::CELL_SET_2D:
    case OpCode::CELL_GET_MULTI:
    case OpCode::CELL_GET_ND:
    case OpCode::CELL_SET_ND:
        return "in cell indexing";

    // Field access
    case OpCode::FIELD_GET:
    case OpCode::FIELD_GET_OR_CREATE:
    case OpCode::FIELD_SET: {
        int16_t nameIdx = instr.d;
        if (nameIdx >= 0 && nameIdx < (int16_t)chunk.strings.size())
            return "in field access '." + chunk.strings[nameIdx] + "'";
        return "in field access";
    }
    case OpCode::FIELD_GET_DYN:
    case OpCode::FIELD_SET_DYN:
        return "in dynamic field access";

    // Binary operators
    case OpCode::ADD:  return "in operator '+'";
    case OpCode::SUB:  return "in operator '-'";
    case OpCode::MUL:  return "in operator '*'";
    case OpCode::RDIV: return "in operator '/'";
    case OpCode::LDIV: return "in operator '\\'";
    case OpCode::POW:  return "in operator '^'";
    case OpCode::EMUL: return "in operator '.*'";
    case OpCode::ERDIV:return "in operator './'";
    case OpCode::ELDIV:return "in operator '.\\'";
    case OpCode::EPOW: return "in operator '.^'";

    // Scalar-specialized
    case OpCode::ADD_SS:  return "in operator '+'";
    case OpCode::SUB_SS:  return "in operator '-'";
    case OpCode::MUL_SS:  return "in operator '*'";
    case OpCode::RDIV_SS: return "in operator '/'";
    case OpCode::POW_SS:  return "in operator '^'";
    case OpCode::NEG_S:   return "in unary operator '-'";

    // Unary operators
    case OpCode::NEG:        return "in unary operator '-'";
    case OpCode::NOT:        return "in unary operator '~'";
    case OpCode::CTRANSPOSE: return "in transpose operator";
    case OpCode::TRANSPOSE:  return "in transpose operator";

    // Colon expressions
    case OpCode::COLON:
    case OpCode::COLON3:
        return "in colon expression";

    // Matrix/cell construction
    case OpCode::HORZCAT:
    case OpCode::HORZCAT_APPEND:
    case OpCode::VERTCAT:
        return "in matrix construction";
    case OpCode::CELL_LITERAL:
        return "in cell construction";

    // Indexing
    case OpCode::INDEX_GET:
    case OpCode::INDEX_GET_2D:
    case OpCode::INDEX_GET_ND:
    case OpCode::INDEX_SET:
    case OpCode::INDEX_SET_2D:
    case OpCode::INDEX_SET_ND:
    case OpCode::INDEX_DELETE:
    case OpCode::INDEX_DELETE_2D:
    case OpCode::INDEX_DELETE_ND:
        return "in array indexing";

    default:
        return "";
    }
}

[[noreturn]] void VM::enrichAndThrow(const std::exception &ex,
                                     const Instruction *ip,
                                     const BytecodeChunk &chunk)
{
    size_t instrIdx = static_cast<size_t>(ip - chunk.code.data());
    if (instrIdx < chunk.sourceMap.size() && chunk.sourceMap[instrIdx].line > 0) {
        std::string context = describeInstruction(*ip, chunk);
        throw Error(ex.what(),
                        chunk.sourceMap[instrIdx].line,
                        chunk.sourceMap[instrIdx].col,
                        chunk.name,
                        context);
    }
    // No source location available — wrap in Error anyway for consistency
    throw Error(ex.what());
}

// ============================================================
// Debugger helpers
// ============================================================

DebugController *VM::debugCtl()
{
    return engine_.debugController_.get();
}

// ============================================================
// Frame management — non-recursive call/return
// ============================================================

void VM::pushCallFrame(const BytecodeChunk &funcChunk, const Value *args, uint8_t nargs,
                       uint8_t destReg, size_t nargout,
                       bool isMulti, uint8_t outBase, uint8_t nout)
{
    if (callDepth() >= maxRecursion_)
        throw std::runtime_error("VM: maximum recursion depth exceeded");

    if (nargs > funcChunk.numParams)
        throw std::runtime_error("Too many input arguments for function '" + funcChunk.name + "'");

    uint8_t nregs = funcChunk.numRegisters;
    if (regStackTop_ + nregs > kRegStackSize)
        throw std::runtime_error("VM: register stack overflow");

    uint8_t pc = std::min(nargs, funcChunk.numParams);

    // Defensive copy: args may point into regStack_ near regStackTop_
    std::vector<Value> argsCopy(pc);
    for (uint8_t i = 0; i < pc; ++i)
        argsCopy[i] = args[i];

    // Allocate registers
    Value *newR = regStack_.data() + regStackTop_;
    for (uint8_t i = 0; i < nregs; ++i)
        newR[i] = Value::empty();

    for (uint8_t i = 0; i < pc; ++i)
        newR[i] = std::move(argsCopy[i]);

    // Inject nargin/nargout into function scope
    for (auto &[vname, reg] : funcChunk.varMap) {
        if (vname == "nargin" && reg < nregs)
            newR[reg] = Value::scalar(static_cast<double>(nargs), nullptr);
        else if (vname == "nargout" && reg < nregs)
            newR[reg] = Value::scalar(static_cast<double>(nargout), nullptr);
    }

    // Import global variables from globalsEnv
    for (auto &gname : funcChunk.globalNames) {
        for (auto &[vname, reg] : funcChunk.varMap) {
            if (vname == gname && reg < nregs) {
                Value *gval = engine_.globalsEnv_->get(gname);
                if (gval)
                    newR[reg] = *gval;
                break;
            }
        }
    }

    // Push call frame
    CallFrame cf;
    cf.chunk = &funcChunk;
    cf.ip = funcChunk.code.data();
    cf.R = newR;
    cf.regBase = regStackTop_;
    cf.nregs = nregs;
    cf.forStackBase = forStack_.size();
    cf.tryStackBase = tryStack_.size();
    cf.destReg = destReg;
    cf.nargout = nargout;
    cf.isMultiReturn = isMulti;
    cf.outBase = outBase;
    cf.nout = nout;
    frames_.push_back(cf);

    regStackTop_ += nregs;
    R_ = newR;

    // Push debug frame
    if (auto *ctl = debugCtl()) {
        StackFrame sf;
        sf.functionName = funcChunk.name;
        sf.chunk = &funcChunk;
        sf.registers = newR;
        ctl->pushFrame(std::move(sf));
    }
}

void VM::popCallFrame(Value retVal)
{
    CallFrame &frame = frames_.back();

    // Update parent frame's registers BEFORE popFrame —
    // so onFunctionExit sees correct parent variables.
    if (auto *ctl = debugCtl()) {
        auto &stack = ctl->callStack();
        if (stack.size() >= 2 && frames_.size() >= 2)
            stack[stack.size() - 2].registers = frames_[frames_.size() - 2].R;
        ctl->popFrame();
    }

    bool isTopLevel = (frames_.size() == 1);

    // Export global variables back to globalsEnv
    for (auto &gname : frame.chunk->globalNames) {
        for (auto &[vname, reg] : frame.chunk->varMap) {
            if (vname == gname && reg < frame.nregs) {
                if (isTopLevel) {
                    // Top-level: only overwrite if assigned (not unset/deleted)
                    if (!frame.R[reg].isUnset() && !frame.R[reg].isDeleted())
                        engine_.globalsEnv_->set(gname, frame.R[reg]);
                    Value *gsVal = engine_.globalsEnv_->get(gname);
                    if (gsVal)
                        engine_.workspaceEnv_->set(gname, *gsVal);
                } else {
                    engine_.globalsEnv_->set(gname, frame.R[reg]);
                    engine_.workspaceEnv_->set(gname, frame.R[reg]);
                }
                break;
            }
        }
    }

    if (isTopLevel) {
        // Export only names the chunk actually wrote to (see
        // exportTopLevelVariables for the rationale).
        lastVarMap_.clear();
        const auto &assigned = frame.chunk->assignedVars;
        for (auto &[name, reg] : frame.chunk->varMap) {
            if (reg < frame.nregs && assigned.count(name))
                lastVarMap_.push_back({name, frame.R[reg]});
        }
    }

    // Cleanup: release heap objects in frame
    for (uint8_t i = 0; i < frame.nregs; ++i) {
        if (!frame.R[i].isDoubleScalar() && !frame.R[i].isEmpty())
            frame.R[i] = Value::empty();
    }

    // Restore stack pointers
    regStackTop_ = frame.regBase;
    forStack_.resize(std::min(forStack_.size(), frame.forStackBase));
    tryStack_.resize(std::min(tryStack_.size(), frame.tryStackBase));

    frames_.pop_back();

    if (!frames_.empty()) {
        CallFrame &caller = frames_.back();
        R_ = caller.R;

        if (frame.isMultiReturn) {
            // Multi-return: distribute returnBuf_ into caller's registers
            if (returnCount_ > 0) {
                for (size_t i = 0; i < frame.nout && i < returnCount_; ++i)
                    caller.R[frame.outBase + i] = std::move(returnBuf_[i]);
                for (uint8_t i = 0; i < returnCount_; ++i)
                    returnBuf_[i] = Value::empty();
                returnCount_ = 0;
            } else {
                // Single return via RET — store in first output slot
                caller.R[frame.outBase] = std::move(retVal);
                for (size_t i = 1; i < frame.nout; ++i)
                    caller.R[frame.outBase + i] = Value::empty();
            }
        } else {
            caller.R[frame.destReg] = std::move(retVal);
        }
    } else {
        R_ = nullptr;
        lastResult_ = std::move(retVal);
    }
}

void VM::setFrameDynVars(std::unordered_map<std::string, Value> *dv)
{
    if (!frames_.empty())
        frames_.back().dynVars = dv;
}

void VM::forSetVar(Value &varReg, const ForState &fs)
{
    if (fs.lazy) {
        // Lazy colon range: compute scalar from start + index*step.
        // start/stop bounds are validated at FOR_INIT_RANGE time, so
        // here we just emit the value with no allocation.
        const double v = fs.lazyStart + static_cast<double>(fs.index) * fs.lazyStep;
        if (varReg.isDoubleScalar())
            varReg.setScalarFast(v);
        else
            varReg.setScalarVal(v);
        return;
    }
    if (fs.rows == 0) {
        // Scalar range — only one iteration, use safe path
        if (varReg.isDoubleScalar())
            varReg.setScalarFast(fs.range.scalarVal());
        else
            varReg.setScalarVal(fs.range.scalarVal());
        return;
    }

    // Handle non-double types
    if (fs.rangeType == ValueType::CHAR) {
        const char *src = static_cast<const char *>(fs.rawData);
        if (fs.rows == 1) {
            varReg = Value::fromString(std::string(1, src[fs.index]), &engine_.allocator_);
        } else {
            auto col = Value::matrix(fs.rows, 1, ValueType::CHAR, &engine_.allocator_);
            char *dst = col.charDataMut();
            const char *colSrc = src + fs.index * fs.rows;
            for (size_t r = 0; r < fs.rows; ++r)
                dst[r] = colSrc[r];
            varReg = std::move(col);
        }
        return;
    }
    if (fs.rangeType == ValueType::LOGICAL) {
        const uint8_t *src = static_cast<const uint8_t *>(fs.rawData);
        if (fs.rows == 1) {
            varReg = Value::logicalScalar(src[fs.index] != 0);
        } else {
            auto col = Value::matrix(fs.rows, 1, ValueType::LOGICAL, &engine_.allocator_);
            uint8_t *dst = col.logicalDataMut();
            const uint8_t *colSrc = src + fs.index * fs.rows;
            for (size_t r = 0; r < fs.rows; ++r)
                dst[r] = colSrc[r];
            varReg = std::move(col);
        }
        return;
    }
    if (fs.rangeType == ValueType::CELL) {
        if (fs.rows == 1) {
            varReg = fs.range.cellAt(fs.index);
        } else {
            auto col = Value::cell(fs.rows, 1);
            for (size_t r = 0; r < fs.rows; ++r)
                col.cellAt(r) = fs.range.cellAt(fs.index * fs.rows + r);
            varReg = std::move(col);
        }
        return;
    }

    // Double path (original)
    if (fs.rows == 1) {
        // Row vector — most common. After first iteration, varReg is always scalar.
        if (varReg.isDoubleScalar())
            varReg.setScalarFast(fs.data[fs.index]);
        else
            varReg.setScalarVal(fs.data[fs.index]);
        return;
    }
    size_t rows = fs.rows;
    auto col = Value::matrix(rows, 1, ValueType::DOUBLE, &engine_.allocator_);
    double *dst = col.doubleDataMut();
    const double *src = fs.data + fs.index * rows;
    for (size_t r = 0; r < rows; ++r)
        dst[r] = src[r];
    varReg = std::move(col);
}

// ============================================================
// Extracted dispatch helpers
// ============================================================

Value VM::binarySlowPath(OpCode op, const Value &lhs, const Value &rhs)
{
    const char *opStr = nullptr;
    switch (op) {
    case OpCode::ADD:   opStr = "+";  break;
    case OpCode::SUB:   opStr = "-";  break;
    case OpCode::MUL:   opStr = "*";  break;
    case OpCode::RDIV:  opStr = "/";  break;
    case OpCode::LDIV:  opStr = "\\"; break;
    case OpCode::POW:   opStr = "^";  break;
    case OpCode::EMUL:  opStr = ".*"; break;
    case OpCode::ERDIV: opStr = "./"; break;
    case OpCode::ELDIV: opStr = ".\\"; break;
    case OpCode::EPOW:  opStr = ".^"; break;
    case OpCode::EQ:    opStr = "=="; break;
    case OpCode::NE:    opStr = "~="; break;
    case OpCode::LT:    opStr = "<";  break;
    case OpCode::GT:    opStr = ">";  break;
    case OpCode::LE:    opStr = "<="; break;
    case OpCode::GE:    opStr = ">="; break;
    case OpCode::AND:   opStr = "&";  break;
    case OpCode::OR:    opStr = "|";  break;
    default: break;
    }
    if (opStr) {
        auto it = engine_.binaryOps_.find(opStr);
        if (it != engine_.binaryOps_.end())
            return it->second(lhs, rhs);
    }
    throw std::runtime_error("VM: unsupported binary op");
}

Value VM::unarySlowPath(OpCode op, const Value &operand)
{
    const char *opStr = nullptr;
    switch (op) {
    case OpCode::NEG:        opStr = "-";  break;
    case OpCode::NOT:        opStr = "~";  break;
    case OpCode::CTRANSPOSE: opStr = "'";  break;
    case OpCode::TRANSPOSE:  opStr = ".'"; break;
    default: break;
    }
    if (opStr) {
        auto it = engine_.unaryOps_.find(opStr);
        if (it != engine_.unaryOps_.end())
            return it->second(operand);
    }
    throw std::runtime_error("VM: unsupported unary op");
}

void VM::execCallBuiltin(const Instruction &I, Value *R)
{
    uint8_t argBase = I.b, na = I.c;
    int16_t bid = I.d;

    // 1-arg scalar fast path
    if (na == 1 && R[argBase].isDoubleScalar()) {
        double v = R[argBase].scalarVal();
        double result;
        bool handled = true;
        switch (bid) {
        case 0:  result = std::fabs(v); break;
        case 1:  result = std::floor(v); break;
        case 2:  result = std::ceil(v); break;
        case 3:  result = std::round(v); break;
        case 4:  result = std::trunc(v); break;
        case 5:  result = std::sqrt(v); break;
        case 6:  result = std::exp(v); break;
        case 7:  result = std::log(v); break;
        case 8:  result = std::log2(v); break;
        case 9:  result = std::log10(v); break;
        case 10: result = std::sin(v); break;
        case 11: result = std::cos(v); break;
        case 12: result = std::tan(v); break;
        case 13:
            result = std::isnan(v) ? std::numeric_limits<double>::quiet_NaN()
                     : (v > 0)     ? 1.0
                     : (v < 0)     ? -1.0
                                   : 0.0;
            break;
        case 14: R[I.a].setLogicalFast(std::isnan(v)); return;
        case 15: R[I.a].setLogicalFast(std::isinf(v)); return;
        default: handled = false; break;
        }
        if (handled) {
            R[I.a].setScalarFast(result);
            return;
        }
    }

    // 2-arg scalar fast path
    if (na == 2 && R[argBase].isDoubleScalar() && R[argBase + 1].isDoubleScalar()) {
        double a = R[argBase].scalarVal(), b = R[argBase + 1].scalarVal();
        double result;
        bool handled = true;
        switch (bid) {
        case 20:
            result = std::fmod(a, b);
            if (result != 0.0 && ((result > 0) != (b > 0)))
                result += b;
            break;
        case 21: result = std::fmod(a, b); break;
        case 22: result = (a >= b) ? a : b; break;
        case 23: result = (a <= b) ? a : b; break;
        case 24: result = std::pow(a, b); break;
        case 25: result = std::atan2(a, b); break;
        default: handled = false; break;
        }
        if (handled) {
            R[I.a].setScalarFast(result);
            return;
        }
    }

    // Generic fallback via externalFuncs_
    static const char *bn[] = {"abs",   "floor", "ceil",  "round", "fix",   "sqrt",
                               "exp",   "log",   "log2",  "log10", "sin",   "cos",
                               "tan",   "sign",  "isnan", "isinf", nullptr, nullptr,
                               nullptr, nullptr, "mod",   "rem",   "max",   "min",
                               "pow",   "atan2"};
    const char *fname = (bid >= 0 && bid < 26) ? bn[bid] : nullptr;
    if (fname) {
        auto extIt = engine_.externalFuncs_.find(fname);
        if (extIt != engine_.externalFuncs_.end()) {
            Span<const Value> as(&R[argBase], na);
            Value ob[1];
            // Output-reuse hint — same logic as the generic CALL path.
            // Hand the destination's current value to the adapter via
            // outs[0] when no arg register aliases R[I.a]; opt-in
            // adapters (NK_UNARY_ADAPTER_HINT) reuse its buffer
            // instead of allocating fresh.
            bool canHint = R[I.a].hasHeap();
            for (uint8_t i = 0; i < na && canHint; ++i)
                if (&R[argBase + i] == &R[I.a])
                    canHint = false;
            if (canHint)
                ob[0] = std::move(R[I.a]);
            Span<Value> os(ob, 1);
            CallContext ctx{&engine_, &engine_.workspaceEnv()};
            extIt->second(as, 1, os, ctx);
            R[I.a] = std::move(ob[0]);
            return;
        }
    }
    throw std::runtime_error("VM: unsupported builtin");
}

bool VM::execCallIndirect(const Instruction &I, Value *R,
                           CallFrame &frame, const Instruction *ip)
{
    uint8_t fhReg = I.b, argBase = I.c, na = I.e;

    // Resolve function handle (plain or closure cell)
    Value funcHandleVal;
    size_t numCaptures = 0;

    if (R[fhReg].isCell() && R[fhReg].numel() >= 1
        && R[fhReg].cellAt(0).isFuncHandle()) {
        funcHandleVal = R[fhReg].cellAt(0);
        numCaptures = R[fhReg].numel() - 1;
    } else if (R[fhReg].isFuncHandle()) {
        funcHandleVal = R[fhReg];
    } else {
        // Array indexing fallback
        execIndirectIndex(I, R);
        return false;
    }

    const std::string &funcName = funcHandleVal.funcHandleName();

    // Build args: user args + captured values
    size_t totalArgsN = static_cast<size_t>(na) + numCaptures;
    std::vector<Value> argsBuf(totalArgsN);
    for (uint8_t i = 0; i < na; ++i)
        argsBuf[i] = R[argBase + i];
    for (size_t i = 0; i < numCaptures; ++i)
        argsBuf[na + i] = R[fhReg].cellAt(1 + i);
    uint8_t totalArgs = static_cast<uint8_t>(std::min(totalArgsN, size_t(255)));

    // Try compiled user function
    if (const BytecodeChunk *found = findCompiledFunc(funcName)) {
        frame.ip = ip + 1;
        pushCallFrame(*found, argsBuf.data(), totalArgs, I.a, 1);
        return true; // caller must re-enter dispatch loop
    }

    // External function
    auto extIt = engine_.externalFuncs_.find(funcName);
    if (extIt != engine_.externalFuncs_.end()) {
        Span<const Value> as(argsBuf.data(), na);
        Value ob[1];
        Span<Value> os(ob, 1);
        CallContext ctx{&engine_, &engine_.workspaceEnv()};
        extIt->second(as, 1, os, ctx);
        R[I.a] = std::move(ob[0]);
        return false;
    }
    throw std::runtime_error("VM: undefined function in handle '@" + funcName + "'");
}

void VM::execIndirectIndex(const Instruction &I, Value *R)
{
    uint8_t fhReg = I.b, argBase = I.c, na = I.e;

    if (na == 1) {
        const Value &mv = R[fhReg];
        const Value &ix = R[argBase];
        if (mv.isCell()) {
            auto indices = Value::resolveIndices(ix, mv.numel());
            R[I.a] = mv.indexGet(indices.data(), indices.size(), &engine_.allocator_);
        } else if (ix.isChar() && ix.numel() == 1 && ix.charData()[0] == ':') {
            size_t n = mv.numel();
            ValueType t = mv.type();
            auto res = Value::matrix(n, 1, t, &engine_.allocator_);
            if (n > 0) {
                size_t es = elementSize(t);
                std::memcpy(res.rawDataMut(), mv.rawData(), n * es);
            }
            R[I.a] = std::move(res);
        } else if (mv.isScalar() && ix.isDoubleScalar()) {
            checkedIndex(ix.scalarVal(), 1);
            R[I.a] = mv;
        } else if (ix.isDoubleScalar()) {
            size_t i = checkedIndex(ix.scalarVal(), mv.numel());
            R[I.a] = mv.elemAt(i, &engine_.allocator_);
        } else if (ix.isLogical()) {
            R[I.a] = mv.logicalIndex(ix.logicalData(), ix.numel(), &engine_.allocator_);
        } else {
            size_t n = ix.numel();
            const double *id = ix.doubleData();
            std::vector<size_t> indices(n);
            for (size_t k = 0; k < n; ++k)
                indices[k] = static_cast<size_t>(id[k]) - 1;
            R[I.a] = mv.indexGet(indices.data(), n, &engine_.allocator_);
        }
    } else if (na == 2) {
        const Value &mv = R[fhReg];
        const Value &ri = R[argBase];
        const Value &ci = R[argBase + 1];
        auto rowIds = Value::resolveIndices(ri, mv.dims().rows());
        auto colIds = Value::resolveIndices(ci, mv.dims().cols());
        R[I.a] = mv.indexGet2D(rowIds.data(), rowIds.size(),
                               colIds.data(), colIds.size(),
                               &engine_.allocator_);
    } else if (na == 3) {
        const Value &mv = R[fhReg];
        if (mv.isCell()) {
            size_t r = (size_t) R[argBase].toScalar() - 1;
            size_t c = (size_t) R[argBase + 1].toScalar() - 1;
            size_t p = (size_t) R[argBase + 2].toScalar() - 1;
            R[I.a] = mv.cellAt(mv.dims().sub2indChecked(r, c, p));
        } else {
            auto rowIds = Value::resolveIndices(R[argBase], mv.dims().rows());
            auto colIds = Value::resolveIndices(R[argBase + 1], mv.dims().cols());
            auto pageIds = Value::resolveIndices(R[argBase + 2], mv.dims().pages());
            R[I.a] = mv.indexGet3D(rowIds.data(), rowIds.size(),
                                   colIds.data(), colIds.size(),
                                   pageIds.data(), pageIds.size(),
                                   &engine_.allocator_);
        }
    } else {
        // ND indexing fallback for na >= 4. CELL handled by indexGetND.
        const Value &mv = R[fhReg];
        const int nd = static_cast<int>(na);
        std::vector<std::vector<size_t>> idxLists(nd);
        std::vector<const size_t *> idxPtrs(nd);
        std::vector<size_t> idxCounts(nd);
        for (int i = 0; i < nd; ++i) {
            const size_t lim = (i < mv.dims().ndim()) ? mv.dims().dim(i) : 1;
            idxLists[i] = Value::resolveIndices(R[argBase + i], lim);
            idxPtrs[i] = idxLists[i].data();
            idxCounts[i] = idxLists[i].size();
        }
        R[I.a] = mv.indexGetND(idxPtrs.data(), idxCounts.data(), nd, &engine_.allocator_);
    }
}

void VM::execDisplay(const Instruction &I, Value *R, const BytecodeChunk &chunk)
{
    if (R[I.a].isUnset())
        return;
    const std::string &name = chunk.strings[I.d];
    engine_.outputText(R[I.a].formatDisplay(name));
}

// MATLAB-parity visibility rule for `who`/`whos` over a chunk's register
// file. Hide reserved names (built-in constants, pseudo-vars, and any
// host-registered constants) UNLESS the chunk has actually assigned them
// — i.e. the user shadowed them. Mirrors DebugWorkspace::names() so both
// surfaces show the same thing.
static bool whoVisible(const Engine &engine, const std::string &name,
                       const BytecodeChunk &chunk)
{
    if (!engine.isReservedName(name))
        return true;
    return chunk.assignedVars.count(name) > 0;
}

void VM::execWho(const Instruction &I, Value *R, const BytecodeChunk &chunk)
{
    std::vector<std::string> names;
    if (I.b == 0) {
        for (auto &[vn, reg] : chunk.varMap) {
            if (reg < chunk.numRegisters && !R[reg].isUnset() && !R[reg].isDeleted()
                && whoVisible(engine_, vn, chunk))
                names.push_back(vn);
        }
    } else {
        for (uint8_t i = 0; i < I.b; ++i) {
            std::string reqName = R[I.a + i].toString();
            for (auto &[vn, reg] : chunk.varMap) {
                if (vn == reqName && reg < chunk.numRegisters && !R[reg].isUnset() && !R[reg].isDeleted()) {
                    names.push_back(vn);
                    break;
                }
            }
        }
    }
    std::sort(names.begin(), names.end());
    if (!names.empty()) {
        std::ostringstream os;
        os << "\nYour variables are:\n\n";
        for (auto &n : names)
            os << n << "  ";
        os << "\n\n";
        if (engine_.outputFunc_)
            engine_.outputFunc_(os.str());
        else
            std::cout << os.str();
    }
}

void VM::execWhos(const Instruction &I, Value *R, const BytecodeChunk &chunk)
{
    std::vector<std::string> names;
    if (I.b == 0) {
        for (auto &[vn, reg] : chunk.varMap) {
            if (reg < chunk.numRegisters && !R[reg].isUnset() && !R[reg].isDeleted()
                && whoVisible(engine_, vn, chunk))
                names.push_back(vn);
        }
    } else {
        for (uint8_t i = 0; i < I.b; ++i) {
            std::string reqName = R[I.a + i].toString();
            for (auto &[vn, reg] : chunk.varMap) {
                if (vn == reqName && reg < chunk.numRegisters && !R[reg].isUnset() && !R[reg].isDeleted()) {
                    names.push_back(vn);
                    break;
                }
            }
        }
    }
    std::sort(names.begin(), names.end());
    std::unordered_set<std::string> globalSet(chunk.globalNames.begin(),
                                              chunk.globalNames.end());
    if (!names.empty()) {
        std::ostringstream os;
        os << "  Name" << std::string(6, ' ') << "Size" << std::string(13, ' ')
           << "Bytes  Class" << std::string(5, ' ') << "Attributes\n\n";
        for (auto &n : names) {
            for (auto &[vn2, reg2] : chunk.varMap) {
                if (vn2 == n && reg2 < chunk.numRegisters) {
                    auto &val = R[reg2];
                    auto &d = val.dims();
                    std::string sizeStr = std::to_string(d.rows()) + "x"
                                          + std::to_string(d.cols());
                    if (d.is3D())
                        sizeStr += "x" + std::to_string(d.pages());
                    std::string bytesStr = std::to_string(val.rawBytes());
                    std::string classStr = mtypeName(val.type());
                    std::string attrStr;
                    if (globalSet.count(n))
                        attrStr = "global";
                    os << "  " << n;
                    for (size_t i = n.size(); i < 10; ++i)
                        os << " ";
                    os << sizeStr;
                    for (size_t i = sizeStr.size(); i < 17; ++i)
                        os << " ";
                    for (size_t i = bytesStr.size(); i < 5; ++i)
                        os << " ";
                    os << bytesStr << "  " << classStr;
                    for (size_t i = classStr.size(); i < 10; ++i)
                        os << " ";
                    os << attrStr << "\n";
                    break;
                }
            }
        }
        os << "\n";
        if (engine_.outputFunc_)
            engine_.outputFunc_(os.str());
        else
            std::cout << os.str();
    }
}

} // namespace numkit