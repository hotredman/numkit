// src/vm.cpp
#include <numkit/core/vm.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

// Forward-declarations from toolboxes/builtin/src/backends/binary_ops_loops.hpp.
// Linked from the same numkit library; lets the VM bypass the public
// builtin::plus()/etc. wrappers (which always allocate a fresh result
// Value) when it can write straight into a uniquely-owned destination
// buffer of the right shape.
namespace numkit::ops::detail {
void plusLoop   (const double *a, const double *b, double *out, std::size_t n);
void minusLoop  (const double *a, const double *b, double *out, std::size_t n);
void timesLoop  (const double *a, const double *b, double *out, std::size_t n);
void rdivideLoop(const double *a, const double *b, double *out, std::size_t n);
} // namespace numkit::ops::detail

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
    case OpCode::ADD:   ops::detail::plusLoop   (a, b, o, n); return true;
    case OpCode::SUB:   ops::detail::minusLoop  (a, b, o, n); return true;
    case OpCode::EMUL:  ops::detail::timesLoop  (a, b, o, n); return true;
    case OpCode::ERDIV: ops::detail::rdivideLoop(a, b, o, n); return true;
    default: return false;
    }
}

VM::VM(Engine &engine)
    : engine_(engine)
{
    regStack_.resize(kRegStackSize);
}

// Single source of truth for inline-builtin id → name. Must track the unary
// (0-15) and binary (20-25) ids in Compiler::resolveBuiltinId; holes (16-19)
// are nullptr. Shared by describeInstruction (error text) and execCallBuiltin
// (generic external fallback) so the two can never drift out of sync.
static const char *builtinIdName(int bid)
{
    static const char *kNames[] = {
        "abs",   "floor", "ceil",  "round", "fix",   "sqrt",  "exp",   "log",
        "log2",  "log10", "sin",   "cos",   "tan",   "sign",  "isnan", "isinf",
        nullptr, nullptr, nullptr, nullptr, "mod",   "rem",   "max",   "min",
        "pow",   "atan2",
    };
    constexpr int n = static_cast<int>(sizeof(kNames) / sizeof(kNames[0]));
    return (bid >= 0 && bid < n) ? kNames[bid] : nullptr;
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
    size_t i = Value::checkedScalarIndex(idx);
    if (i >= numel)
        throw std::runtime_error("Index " + std::to_string((size_t) idx) + " exceeds array size "
                                 + std::to_string(numel));
    return i;
}

// Resolve one assignment subscript to 0-based indices for an object-array
// slice store: ':' expands to the whole current dimension; everything else
// (scalar / range / vector / logical) is grow-friendly (unchecked).
static std::vector<size_t> resolveStoreSubscript(const Value &s, size_t curDim)
{
    if (s.isChar() && s.numel() == 1 && s.charData()[0] == ':') {
        std::vector<size_t> out(curDim);
        for (size_t i = 0; i < curDim; ++i)
            out[i] = i;
        return out;
    }
    return Value::resolveIndicesUnchecked(s);
}

// ============================================================
// Public API
// ============================================================

Value VM::execute(const BytecodeChunk &chunk, const Value *args, uint8_t nargs)
{
    // Re-entrancy: a builtin invoked during dispatch (e.g. the `run`
    // builtin or any future eval-from-handler) may call back into
    // engine.eval(), which lands here while the outer chunk is mid-
    // execution. Snapshot the running VM state, run the inner chunk
    // through the normal startExecution / dispatch path, then restore
    // so the outer dispatch loop resumes seamlessly.
    const bool reentrant = !frames_.empty();
    std::unique_ptr<PausedState> outerState;
    if (reentrant)
        outerState = savePausedState();

    // RAII: export variables and cleanup on ANY exit path (success, exception,
    // debug stop). Top-level only — re-entrant runs restore the outer frames
    // explicitly below so the dispatch loop can resume.
    struct ExecuteGuard
    {
        VM &vm;
        bool reentrant;
        ExecuteGuard(VM &v, bool r) : vm(v), reentrant(r) {}
        ~ExecuteGuard()
        {
            if (reentrant)
                return; // restoration happens on the success path below
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
    } guard(*this, reentrant);

    ExecStatus status;
    try {
        status = startExecution(chunk, args, nargs);
    } catch (...) {
        // Inner threw before producing a result. Restore the outer so the
        // outer's catch handler sees consistent VM state, then rethrow.
        if (reentrant)
            restorePausedState(std::move(outerState));
        throw;
    }

    if (status == ExecStatus::Paused) {
        // Legacy API: convert pause to exception. The non-reentrant guard
        // wipes state; the reentrant path restores the outer first.
        if (reentrant)
            restorePausedState(std::move(outerState));
        throw DebugStopException();
    }

    Value result = std::move(lastResult_);
    if (reentrant) {
        // Commit any workspace exports the inner script produced, then
        // bring back the outer frame stack so dispatch can resume.
        if (!frames_.empty())
            exportTopLevelVariables();
        restorePausedState(std::move(outerState));
    }
    return result;
}

std::vector<Value> VM::callReentrant(const BytecodeChunk &chunk, Span<const Value> args,
                                     size_t nargout, const std::string &ownerClass,
                                     bool isCtor, const Value *ctorSeed)
{
    // Save the outer VM state and run `chunk` as a fresh top-level frame, then
    // restore. Mirrors execute()'s re-entrancy (which is why it is safe: the
    // outer dispatch loop's frame/register references survive — savePausedState
    // parks the frames_ buffer and restorePausedState move-assigns it back, so
    // the buffer address is preserved). Unlike execute() we do NOT reset the
    // debug controller, so the callee's frames nest on the live debug stack.
    const bool reentrant = !frames_.empty();
    std::unique_ptr<PausedState> outerState;
    if (reentrant)
        outerState = savePausedState(); // frames_ now empty; outer buffer parked

    // popCallFrame's top-level (frames_.size()==1) branch repopulates
    // lastVarMap_ from the callee's locals; snapshot+restore so the outer's
    // pending workspace export is not clobbered by this nested run.
    std::vector<std::pair<std::string, Value>> savedVarMap = std::move(lastVarMap_);

    struct Restorer
    {
        VM &vm;
        bool reentrant;
        std::unique_ptr<PausedState> st;
        std::vector<std::pair<std::string, Value>> vm_savedVarMap;
        bool done = false;
        void run()
        {
            if (done)
                return;
            done = true;
            vm.lastVarMap_ = std::move(vm_savedVarMap);
            if (reentrant)
                vm.restorePausedState(std::move(st));
        }
        ~Restorer() { run(); }
    } restorer{*this, reentrant, std::move(outerState), std::move(savedVarMap)};

    const size_t nout = std::max<size_t>(nargout, 1);
    const bool multi = nout > 1;
    std::vector<Value> argbuf(args.begin(), args.end());

    returnCount_ = 0;
    lastResult_ = Value();
    pushCallFrame(chunk, argbuf.empty() ? nullptr : argbuf.data(),
                  static_cast<uint8_t>(argbuf.size()), /*destReg=*/0, nout,
                  /*isMulti=*/multi, /*outBase=*/0, /*nout=*/static_cast<uint8_t>(nout),
                  ownerClass, isCtor, ctorSeed);

    ExecStatus status = dispatchLoop();
    if (status == ExecStatus::Paused) {
        // A debugger pause cannot suspend across the C++ re-entry boundary.
        restorer.run(); // restore the outer before unwinding
        throw DebugStopException();
    }

    std::vector<Value> results;
    if (returnCount_ > 0) {
        results.reserve(returnCount_);
        for (uint8_t i = 0; i < returnCount_; ++i)
            results.push_back(std::move(returnBuf_[i]));
        returnCount_ = 0;
    } else if (!lastResult_.isUnset()) {
        results.push_back(std::move(lastResult_));
    }
    lastResult_ = Value();
    return results; // restorer dtor restores the outer state
}

// ── Paused state save/restore (for debug eval) ────────────

std::unique_ptr<VM::PausedState> VM::savePausedState()
{
    auto s = std::make_unique<PausedState>();
    // Move (not copy) — CallFrame holds a unique_ptr<Environment>. The
    // inner re-entrant execution will clear frames_ in startExecution()
    // anyway, so move semantics drop nothing we need.
    s->frames = std::move(frames_);
    s->forStack = std::move(forStack_);
    s->tryStack = std::move(tryStack_);
    s->regStackTop = regStackTop_;
    s->lastResult = std::move(lastResult_);
    // BUG #3: rescue chunkCallCache_ before startExecution clears it.
    // The outer dispatch loop holds a reference into one of the map's
    // value vectors; unordered_map move preserves node addresses, so
    // taking custody here keeps the outer's reference valid.
    s->chunkCallCache = std::move(chunkCallCache_);
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
    // BUG #3: restore the outer's call-target cache. The inner's
    // cache (built up during re-entry) is destroyed by this move-assign;
    // outer's nodes reclaim their original addresses, so the dispatch
    // loop's captured `resolvedFuncs` reference becomes live again.
    chunkCallCache_ = std::move(s->chunkCallCache);
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
    lastResult_ = Value();
    atErrorPause_ = false; // fresh run — drop any stale dbstop-if-error state
    pausedError_ = nullptr;

    // Allocate registers for top-level frame
    uint8_t nregs = chunk.numRegisters;
    R_ = regStack_.data();

    for (uint8_t i = 0; i < nregs; ++i)
        R_[i] = Value();

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
    // Scoped-eval read-side: caller's variable snapshot was attached
    // via setNextFrameDynVars before this startExecution call. Plug it
    // in so this top-level frame's ASSERT_DEF fallback resolves bare
    // identifiers from the caller's scope.
    if (nextFrameDynVars_) {
        cf.dynVars = nextFrameDynVars_;
        nextFrameDynVars_ = nullptr;  // single-shot
    }
    frames_.push_back(std::move(cf));

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

    // Resuming from a dbstop-if-error pause: the error was deferred, not
    // handled — let it propagate now (the debug session ends with the error).
    // Clean up the parked frames first, mirroring execute()'s guard.
    if (atErrorPause_) {
        atErrorPause_ = false;
        std::exception_ptr ep = pausedError_;
        pausedError_ = nullptr;
        frames_.clear();
        forStack_.clear();
        tryStack_.clear();
        regStackTop_ = 0;
        R_ = nullptr;
        if (ep)
            std::rethrow_exception(ep);
    }

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
                R[I.a] = Value::matrix(0, 0, ValueType::DOUBLE, engine_.mr_);
                break;
            case OpCode::LOAD_STRING:
                R[I.a] = Value::fromString(chunk.strings[I.d], engine_.mr_);
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
            case OpCode::COLLAPSE:
                // Collapse a CSL reaching a single-value sink (no-op on a non-CSL).
                // Emitted only by the dataflow pass; inert until producers make CSLs.
                R[I.a] = collapseCsl(std::move(R[I.b]));
                break;
            case OpCode::COLON_ALL:
                R[I.a] = Value::fromString(":", engine_.mr_);
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
                R[I.a] = Value::scalar(static_cast<double>(sz), engine_.mr_);
                break;
            }

            // ── Scalar arithmetic ────────────────────────────────
            case OpCode::ADD:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setScalarFast(asScalar(R[I.b]) + asScalar(R[I.c]));
                } else if (!tryInPlaceBinaryOp(R[I.a], OpCode::ADD, R[I.b], R[I.c])) {
                    { if (tryBinaryOpFrame(I.a, I.op, I.b, I.c, frame, ip)) goto enter_frame;
                          R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]); }
                }
                break;
            case OpCode::SUB:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setScalarFast(asScalar(R[I.b]) - asScalar(R[I.c]));
                } else if (!tryInPlaceBinaryOp(R[I.a], OpCode::SUB, R[I.b], R[I.c])) {
                    { if (tryBinaryOpFrame(I.a, I.op, I.b, I.c, frame, ip)) goto enter_frame;
                          R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]); }
                }
                break;
            case OpCode::MUL:  // matrix multiply — output reuse skipped (different shape rules)
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setScalarFast(asScalar(R[I.b]) * asScalar(R[I.c]));
                } else
                    { if (tryBinaryOpFrame(I.a, I.op, I.b, I.c, frame, ip)) goto enter_frame;
                          R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]); }
                break;
            case OpCode::EMUL:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setScalarFast(asScalar(R[I.b]) * asScalar(R[I.c]));
                } else if (!tryInPlaceBinaryOp(R[I.a], OpCode::EMUL, R[I.b], R[I.c])) {
                    { if (tryBinaryOpFrame(I.a, I.op, I.b, I.c, frame, ip)) goto enter_frame;
                          R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]); }
                }
                break;
            case OpCode::RDIV:  // matrix right divide — output reuse skipped
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setScalarFast(asScalar(R[I.b]) / asScalar(R[I.c]));
                } else
                    { if (tryBinaryOpFrame(I.a, I.op, I.b, I.c, frame, ip)) goto enter_frame;
                          R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]); }
                break;
            case OpCode::ERDIV:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setScalarFast(asScalar(R[I.b]) / asScalar(R[I.c]));
                } else if (!tryInPlaceBinaryOp(R[I.a], OpCode::ERDIV, R[I.b], R[I.c])) {
                    { if (tryBinaryOpFrame(I.a, I.op, I.b, I.c, frame, ip)) goto enter_frame;
                          R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]); }
                }
                break;
            case OpCode::LDIV:
            case OpCode::ELDIV:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setScalarFast(asScalar(R[I.c]) / asScalar(R[I.b]));
                } else
                    { if (tryBinaryOpFrame(I.a, I.op, I.b, I.c, frame, ip)) goto enter_frame;
                          R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]); }
                break;
            case OpCode::POW:
            case OpCode::EPOW:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    double base = asScalar(R[I.b]);
                    double exp = asScalar(R[I.c]);
                    if (base < 0.0 && exp != std::floor(exp)) {
                        // negative base ^ non-integer exp -> complex; defer to power().
                        R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]);
                    } else {
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
                    }
                } else
                    { if (tryBinaryOpFrame(I.a, I.op, I.b, I.c, frame, ip)) goto enter_frame;
                          R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]); }
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
                if (base < 0.0 && exp != std::floor(exp)) {
                    // negative base ^ non-integer exp -> complex; defer to power().
                    R[I.a] = binarySlowPath(OpCode::EPOW, R[I.b], R[I.c]);
                    break;
                }
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

            // ── Fused multiply-add/sub (loop opt #2) ──────────────
            // Fast path: all-scalar, two-step double (prod = c*e; a = b ± prod)
            // — bit-identical to the unfused MUL+ADD/SUB. Else fall back to the
            // matching product (matmul / elementwise) then sum/diff via the slow
            // path (handles arrays / complex / object operator overloads). The
            // product double is read before the dst write, so dst==src aliasing
            // is safe.
            case OpCode::MULADD:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c]) && isArithScalar(R[I.e])) {
                    double prod = asScalar(R[I.c]) * asScalar(R[I.e]);
                    R[I.a].setScalarFast(asScalar(R[I.b]) + prod);
                } else {
                    Value prod = binarySlowPath(OpCode::MUL, R[I.c], R[I.e]);
                    R[I.a] = binarySlowPath(OpCode::ADD, R[I.b], prod);
                }
                break;
            case OpCode::MULSUB:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c]) && isArithScalar(R[I.e])) {
                    double prod = asScalar(R[I.c]) * asScalar(R[I.e]);
                    R[I.a].setScalarFast(asScalar(R[I.b]) - prod);
                } else {
                    Value prod = binarySlowPath(OpCode::MUL, R[I.c], R[I.e]);
                    R[I.a] = binarySlowPath(OpCode::SUB, R[I.b], prod);
                }
                break;
            case OpCode::EMULADD:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c]) && isArithScalar(R[I.e])) {
                    double prod = asScalar(R[I.c]) * asScalar(R[I.e]);
                    R[I.a].setScalarFast(asScalar(R[I.b]) + prod);
                } else {
                    Value prod = binarySlowPath(OpCode::EMUL, R[I.c], R[I.e]);
                    R[I.a] = binarySlowPath(OpCode::ADD, R[I.b], prod);
                }
                break;
            case OpCode::EMULSUB:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c]) && isArithScalar(R[I.e])) {
                    double prod = asScalar(R[I.c]) * asScalar(R[I.e]);
                    R[I.a].setScalarFast(asScalar(R[I.b]) - prod);
                } else {
                    Value prod = binarySlowPath(OpCode::EMUL, R[I.c], R[I.e]);
                    R[I.a] = binarySlowPath(OpCode::SUB, R[I.b], prod);
                }
                break;

            // ── Comparison ───────────────────────────────────────
            case OpCode::EQ:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setLogicalFast(asScalar(R[I.b]) == asScalar(R[I.c]));
                } else
                    { if (tryBinaryOpFrame(I.a, I.op, I.b, I.c, frame, ip)) goto enter_frame;
                          R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]); }
                break;
            case OpCode::NE:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setLogicalFast(asScalar(R[I.b]) != asScalar(R[I.c]));
                } else
                    { if (tryBinaryOpFrame(I.a, I.op, I.b, I.c, frame, ip)) goto enter_frame;
                          R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]); }
                break;
            case OpCode::LT:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setLogicalFast(asScalar(R[I.b]) < asScalar(R[I.c]));
                } else
                    { if (tryBinaryOpFrame(I.a, I.op, I.b, I.c, frame, ip)) goto enter_frame;
                          R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]); }
                break;
            case OpCode::GT:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setLogicalFast(asScalar(R[I.b]) > asScalar(R[I.c]));
                } else
                    { if (tryBinaryOpFrame(I.a, I.op, I.b, I.c, frame, ip)) goto enter_frame;
                          R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]); }
                break;
            case OpCode::LE:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setLogicalFast(asScalar(R[I.b]) <= asScalar(R[I.c]));
                } else
                    { if (tryBinaryOpFrame(I.a, I.op, I.b, I.c, frame, ip)) goto enter_frame;
                          R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]); }
                break;
            case OpCode::GE:
                if (isArithScalar(R[I.b]) && isArithScalar(R[I.c])) {
                    R[I.a].setLogicalFast(asScalar(R[I.b]) >= asScalar(R[I.c]));
                } else
                    { if (tryBinaryOpFrame(I.a, I.op, I.b, I.c, frame, ip)) goto enter_frame;
                          R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]); }
                break;
            case OpCode::AND:
                if (R[I.b].isDoubleScalar() && R[I.c].isDoubleScalar()) {
                    R[I.a].setLogicalFast(
                        R[I.b].scalarVal() != 0.0 && R[I.c].scalarVal() != 0.0);
                } else
                    { if (tryBinaryOpFrame(I.a, I.op, I.b, I.c, frame, ip)) goto enter_frame;
                          R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]); }
                break;
            case OpCode::OR:
                if (R[I.b].isDoubleScalar() && R[I.c].isDoubleScalar()) {
                    R[I.a].setLogicalFast(
                        R[I.b].scalarVal() != 0.0 || R[I.c].scalarVal() != 0.0);
                } else
                    { if (tryBinaryOpFrame(I.a, I.op, I.b, I.c, frame, ip)) goto enter_frame;
                          R[I.a] = binarySlowPath(I.op, R[I.b], R[I.c]); }
                break;

            // ── Unary ────────────────────────────────────────────
            case OpCode::NEG:
                if (R[I.b].isDoubleScalar()) {
                    R[I.a].setScalarFast(-R[I.b].scalarVal());
                } else
                    { if (tryUnaryOpFrame(I.a, I.op, I.b, frame, ip)) goto enter_frame;
                          R[I.a] = unarySlowPath(I.op, R[I.b]); }
                break;
            case OpCode::UPLUS:
                R[I.a] = R[I.b];
                break;
            case OpCode::NOT:
                if (R[I.b].isDoubleScalar()) {
                    R[I.a].setLogicalFast(R[I.b].scalarVal() == 0.0);
                } else
                    { if (tryUnaryOpFrame(I.a, I.op, I.b, frame, ip)) goto enter_frame;
                          R[I.a] = unarySlowPath(I.op, R[I.b]); }
                break;
            case OpCode::CTRANSPOSE:
            case OpCode::TRANSPOSE:
                if (R[I.b].isDoubleScalar()) {
                    R[I.a] = R[I.b];
                } else
                    { if (tryUnaryOpFrame(I.a, I.op, I.b, frame, ip)) goto enter_frame;
                          R[I.a] = unarySlowPath(I.op, R[I.b]); }
                break;

            // ── Fused element-wise idiom ─────────────────────────
            case OpCode::FUSE_EWISE: {
                // Gather the contiguous operand register block and run the
                // kernel; on success write dst and skip the `e` fallback
                // instructions, else fall through to them (the normally
                // compiled idiom). The runtime gate lets setFusion(false)
                // disable fusion even for already-compiled bytecode.
                if (engine_.fusionEnabled()) {
                    const FusionRule &rule =
                        engine_.fusionRules()[static_cast<size_t>(I.d)];
                    Value fout;
                    if (rule.execute(&R[I.b], I.c, fout, engine_.mr_)) {
                        engine_.noteFusionHit();
                        R[I.a] = std::move(fout);
                        ip += static_cast<int>(I.e) + 1;
                        continue;
                    }
                }
                break;  // disabled or declined → execute the fallback opcodes
            }

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
            // Type rule (matches MATLAB + tree_walker.cpp:colonOutputType):
            //   * all double → DOUBLE
            //   * one non-double type T (others double) → T
            //   * two different non-double types → throw
            case OpCode::COLON: {
                ValueType ta = R[I.b].type(), tb = R[I.c].type();
                ValueType t = (ta != ValueType::DOUBLE) ? ta
                            : (tb != ValueType::DOUBLE) ? tb
                            : ValueType::DOUBLE;
                if (ta != ValueType::DOUBLE && tb != ValueType::DOUBLE && ta != tb)
                    throw std::runtime_error(
                        "Colon operands must be all the same type, "
                        "or mixed with real scalar doubles");
                double start = R[I.b].toScalar(), stop = R[I.c].toScalar();
                R[I.a] = Value::colonRangeTyped(start, stop, t, engine_.mr_);
                break;
            }
            case OpCode::COLON3: {
                ValueType ta = R[I.b].type(), tb = R[I.c].type(), tc = R[I.e].type();
                ValueType t = ValueType::DOUBLE;
                ValueType nondefs[3] = {ta, tb, tc};
                for (int k = 0; k < 3; ++k) {
                    if (nondefs[k] == ValueType::DOUBLE) continue;
                    if (t == ValueType::DOUBLE) t = nondefs[k];
                    else if (t != nondefs[k])
                        throw std::runtime_error(
                            "Colon operands must be all the same type, "
                            "or mixed with real scalar doubles");
                }
                double start = R[I.b].toScalar(), step = R[I.c].toScalar(),
                       stop = R[I.e].toScalar();
                R[I.a] = Value::colonRangeTyped(start, step, stop, t, engine_.mr_);
                break;
            }

            // ── Array construction ───────────────────────────────
            case OpCode::HORZCAT:
                R[I.a] = Value::horzcat(&R[I.b], I.c, engine_.mr_);
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
                    dst.appendScalar(val.toScalar(), engine_.mr_);
                    break;
                }
                Value elems[2] = { dst, val };
                R[I.a] = Value::horzcat(elems, 2, engine_.mr_);
                break;
            }
            case OpCode::VERTCAT:
                R[I.a] = Value::vertcat(&R[I.b], I.c, engine_.mr_);
                break;
            case OpCode::HORZCAT_APPEND_CSL: {
                // a = dst (in/out), b = struct array source, d = nameIdx.
                // Expand `[dst, src(0).f, ..., src(N-1).f]` in one
                // horzcat to avoid quadratic growth.
                const std::string &fname = chunk.strings[I.d];
                Value &dst = R[I.a];
                const Value &src = R[I.b];
                std::vector<Value> elems;
                elems.push_back(dst);
                if (src.isStruct()) {
                    size_t n = src.numel();
                    elems.reserve(n + 1);
                    for (size_t i = 0; i < n; ++i) {
                        const auto &m = src.structArrayElem(i);
                        auto it = m.find(fname);
                        if (it == m.end())
                            throw std::runtime_error(
                                "Reference to non-existent field '" + fname + "'");
                        elems.push_back(it->second);
                    }
                } else if (src.isObject()) {
                    // OBJECT array CSL: [arr.prop] expands prop over each
                    // element via the class propGet hook.
                    const BuiltinClass *cls = engine_.findClass(src.objectClassName());
                    size_t n = src.objectCount();
                    elems.reserve(n + 1);
                    CallContext ctx{&engine_, currentCallEnv()};
                    for (size_t i = 0; i < n; ++i) {
                        Value elem = src.objectSubArray({i}, engine_.mr_);
                        Value out;
                        if (!cls || !cls->propGet || !cls->propGet(elem, fname, out, ctx))
                            throw std::runtime_error("No appropriate property '" + fname
                                                     + "' for class '"
                                                     + src.objectClassName() + "'");
                        elems.push_back(std::move(out));
                    }
                } else {
                    throw std::runtime_error(
                        "Comma-separated list expansion needs a struct or object");
                }
                dst = Value::horzcat(elems.data(), elems.size(), engine_.mr_);
                break;
            }
            case OpCode::HORZCAT_APPEND_FLATTEN: {
                // a=dst (in/out), b=elem. First-class concat builder: if R[elem] is a
                // comma-separated list, horzcat-append each item; else append the one
                // value. Driven by the runtime value, so [a, c{idx}, b] flattens for any
                // subscript. See dev-docs/memory/csl_first_class.md.
                Value             &dst  = R[I.a];
                const Value       &elem = R[I.b];
                std::vector<Value> elems;
                elems.push_back(dst);
                if (elem.isCsl())
                    for (size_t k = 0; k < elem.cslCount(); ++k)
                        elems.push_back(elem.cslAt(k));
                else
                    elems.push_back(elem);
                dst = Value::horzcat(elems.data(), elems.size(), engine_.mr_);
                break;
            }

            // ── Array indexing ───────────────────────────────────
            case OpCode::INDEX_GET: {
                const Value &mv = R[I.b];
                const Value &ix = R[I.c];
                // Hot path: heap double indexed by a scalar double -- the
                // dominant `x(n)` read in scalar loops. Skips the object/cell/
                // scalar branch chain and elemAt's internal type-switch; reads
                // straight into an inline scalar register. (Argument is read
                // before the write, so a dst==src register alias is safe.)
                if (mv.isHeapDouble() && ix.isDoubleScalar()) {
                    size_t i = checkedIndex(ix.scalarVal(), mv.numel());
                    R[I.a].setScalarFast(mv.doubleData()[i]);
                    break;
                }
                // OBJECT: obj(i) dispatches to the class subsref overload.
                if (mv.isObject()) {
                    Value idxArgs[1] = {ix};
                    // classdef subsref → same-stack VM frame (pausable, P4)
                    if (tryObjectSubsrefFrame(I.a, I.b, Span<const Value>(idxArgs, 1), frame, ip))
                        goto enter_frame;
                    Value out;
                    if (engine_.tryObjectSubsref(R[I.b], Span<const Value>(idxArgs, 1), 1,
                                                 out, currentCallEnv())) {
                        R[I.a] = std::move(out);
                        break;
                    }
                    // No custom subsref → builtin object-array indexing.
                    auto idxs = Value::resolveIndices(ix, mv.objectCount());
                    R[I.a] = mv.objectSubArray(idxs, engine_.mr_);
                    break;
                }
                if (mv.isCell()) {
                    // Cell () indexing always returns sub-cell
                    auto indices = Value::resolveIndices(ix, mv.numel());
                    R[I.a] = mv.indexGet(indices.data(), indices.size(),
                                         engine_.mr_);
                } else if (mv.isScalar() && ix.isDoubleScalar()) {
                    checkedIndex(ix.scalarVal(), 1); // validate bounds
                    R[I.a] = mv;
                } else if (ix.isDoubleScalar()) {
                    size_t i = checkedIndex(ix.scalarVal(), mv.numel());
                    R[I.a] = mv.elemAt(i, engine_.mr_);
                } else if (ix.isLogical()) {
                    R[I.a] = mv.logicalIndex(ix.logicalData(), ix.numel(),
                                             engine_.mr_);
                } else {
                    auto indices = Value::resolveIndices(ix, mv.numel());
                    R[I.a] = mv.indexGet(indices.data(), indices.size(), engine_.mr_);
                }
                break;
            }
            case OpCode::INDEX_GET_2D: {
                const Value &mv = R[I.b];
                // OBJECT: a custom subsref controls indexing; otherwise the
                // builtin 2-D path below (indexGet2D) handles object arrays.
                if (mv.isObject()) {
                    Value idxArgs[2] = {R[I.c], R[I.e]};
                    // classdef subsref → same-stack VM frame (pausable, P4)
                    if (tryObjectSubsrefFrame(I.a, I.b, Span<const Value>(idxArgs, 2), frame, ip))
                        goto enter_frame;
                    Value out;
                    if (engine_.tryObjectSubsref(R[I.b], Span<const Value>(idxArgs, 2), 1,
                                                 out, currentCallEnv())) {
                        R[I.a] = std::move(out);
                        break;
                    }
                }
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
                                        engine_.mr_);
                break;
            }
            case OpCode::INDEX_SET: {
                const Value &ix = R[I.b];
                // Hot path: A(i) = scalar into a uniquely-owned heap double,
                // scalar index in bounds -- the dominant `y(n) = …` write in
                // scalar loops. Skips the object / append / ensureSize / elemSet
                // chain and writeScalar's type-switch, writing the element
                // directly. Out-of-bounds / shared / non-scalar fall through.
                if (R[I.a].isHeapDouble() && R[I.a].heapRefCount() == 1
                    && ix.isDoubleScalar() && R[I.c].isDoubleScalar()) {
                    size_t i = Value::checkedScalarIndex(ix.scalarVal());
                    if (i < R[I.a].numel()) {
                        R[I.a].doubleDataMut()[i] = R[I.c].scalarVal();
                        break;
                    }
                }
                // OBJECT: obj(i) = v dispatches to the class subsasgn
                // overload (args = [index, value]); mutates R[I.a] in place.
                if (R[I.a].isObject()) {
                    Value sargs[2] = {ix, R[I.c]}; // [index, value]
                    // classdef subsasgn → same-stack VM frame (pausable, P4)
                    if (tryObjectSubsasgnFrame(I.a, Span<const Value>(sargs, 2), frame, ip))
                        goto enter_frame;
                    const BuiltinClass *cls = engine_.findClass(R[I.a].objectClassName());
                    if (cls && cls->subsasgn) {
                        Value args[2] = {ix, R[I.c]};
                        Value out[1];
                        CallContext ctx{&engine_, currentCallEnv()};
                        cls->subsasgn(R[I.a], Span<const Value>(args, 2), 0,
                                      Span<Value>(out, 1), ctx);
                        break;
                    }
                    // else: builtin object-array element store below.
                }
                // Builtin object-array indexed store: arr(i) = obj. Fires
                // when RHS is an object and the target is empty/unset (→ a
                // fresh object array) or an object array of the same class.
                if (R[I.c].isObject()) {
                    Value &dst = R[I.a];
                    const bool newable =
                        !dst.isObject() && (dst.isUnset() || dst.isEmpty());
                    const bool sameArr =
                        dst.isObject()
                        && dst.objectClassName() == R[I.c].objectClassName();
                    if (newable || sameArr) {
                        // Linear slice: scalar / range / vector / logical / `:`.
                        std::vector<size_t> pos =
                            resolveStoreSubscript(ix, dst.objectCount());
                        engine_.objectStoreSlice(dst, {pos}, R[I.c], currentCallEnv());
                        break;
                    }
                }
                if (R[I.a].isObject())
                    throw std::runtime_error("'()' assignment is not defined for class '"
                                             + R[I.a].objectClassName() + "'");
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
                        dst.appendScalar(rhs.toScalar(), engine_.mr_);
                        break;
                    }

                    if (dst.isEmpty() || dst.isScalar() || i >= dst.numel())
                        dst.ensureSize(i, engine_.mr_);
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
                        R[I.a].ensureSize(maxIdx, engine_.mr_);
                    R[I.a].indexSet(indices.data(), indices.size(), R[I.c]);
                }
                break;
            }
            case OpCode::INDEX_SET_2D: {
                const Value &ri = R[I.b];
                const Value &ci = R[I.c];
                const Value &val = R[I.e];

                // OBJECT: arr(i,j) = obj — builtin 2-D element store + grow,
                // when the target is empty/unset or a same-class object array.
                if (val.isObject()) {
                    Value &dst = R[I.a];
                    const bool newable =
                        !dst.isObject() && (dst.isUnset() || dst.isEmpty());
                    const bool sameArr =
                        dst.isObject()
                        && dst.objectClassName() == val.objectClassName();
                    if (newable || sameArr) {
                        std::vector<std::vector<size_t>> perDim = {
                            resolveStoreSubscript(ri, dst.isObject() ? dst.dims().rows() : 0),
                            resolveStoreSubscript(ci, dst.isObject() ? dst.dims().cols() : 0)};
                        engine_.objectStoreSlice(dst, perDim, val, currentCallEnv());
                        break;
                    }
                }

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
                    R[I.a].resize(newR, newC, engine_.mr_);
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
                    R[I.a].resize(maxR, maxC, engine_.mr_);

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
                // OBJECT: a custom subsref controls indexing; otherwise the
                // builtin N-D path below (indexGet3D/indexGetND) handles it.
                if (mv.isObject()) {
                    std::vector<Value> idx(ndims);
                    for (uint8_t i = 0; i < ndims; ++i)
                        idx[i] = R[base + i];
                    // classdef subsref → same-stack VM frame (pausable, P4)
                    if (tryObjectSubsrefFrame(I.a, I.b, Span<const Value>(idx.data(), ndims),
                                              frame, ip))
                        goto enter_frame;
                    Value out;
                    if (engine_.tryObjectSubsref(R[I.b], Span<const Value>(idx.data(), ndims),
                                                 1, out, currentCallEnv())) {
                        R[I.a] = std::move(out);
                        break;
                    }
                }
                if (ndims == 3) {
                    auto rowIds = Value::resolveIndices(R[base], mv.dims().rows());
                    auto colIds = Value::resolveIndices(R[base + 1], mv.dims().cols());
                    auto pageIds = Value::resolveIndices(R[base + 2], mv.dims().pages());
                    R[I.a] = mv.indexGet3D(rowIds.data(), rowIds.size(),
                                           colIds.data(), colIds.size(),
                                           pageIds.data(), pageIds.size(),
                                           engine_.mr_);
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
                                           engine_.mr_);
                }
                break;
            }
            case OpCode::INDEX_SET_ND: {
                // a=arr/cell, b=base, c=ndims, e=val
                uint8_t base = I.b, ndims = I.c;
                // OBJECT: arr(i,j,k,…) = obj — builtin N-D element store + grow.
                if (R[I.e].isObject()) {
                    Value &dst = R[I.a];
                    const Value &val = R[I.e];
                    const bool newable =
                        !dst.isObject() && (dst.isUnset() || dst.isEmpty());
                    const bool sameArr =
                        dst.isObject()
                        && dst.objectClassName() == val.objectClassName();
                    if (newable || sameArr) {
                        std::vector<std::vector<size_t>> perDim(ndims);
                        for (uint8_t i = 0; i < ndims; ++i) {
                            size_t curDim =
                                dst.isObject()
                                    ? (i < dst.dims().ndim() ? dst.dims().dim(i) : 1)
                                    : 0;
                            perDim[i] = resolveStoreSubscript(R[base + i], curDim);
                        }
                        engine_.objectStoreSlice(dst, perDim, val, currentCallEnv());
                        break;
                    }
                }
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
                        R[I.a].resize3d(maxR, maxC, maxP, engine_.mr_);

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
                        R[I.a].resizeND(need.data(), newNd, engine_.mr_);

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
                R[I.a].indexDelete(indices.data(), indices.size(), engine_.mr_);
                break;
            }
            case OpCode::INDEX_DELETE_2D: {
                // a=arr, b=row, c=col
                size_t Rows = R[I.a].dims().rows(), Cols = R[I.a].dims().cols();
                auto rowIdx = Value::resolveIndices(R[I.b], Rows);
                auto colIdx = Value::resolveIndices(R[I.c], Cols);
                R[I.a].indexDelete2D(rowIdx.data(), rowIdx.size(),
                                     colIdx.data(), colIdx.size(),
                                     engine_.mr_);
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
                                     ndims, engine_.mr_);
                break;
            }

            // ── Struct field access ──────────────────────────────
            case OpCode::FIELD_GET: {
                // a=dst, b=obj, d=nameIdx — strict: throws if field missing
                const std::string &fname = chunk.strings[I.d];
                // OBJECT: obj.Prop via class property hook (object model).
                if (R[I.b].isObject()) {
                    const std::string &cn = R[I.b].objectClassName();
                    const BuiltinClass *cls = engine_.findClass(cn);
                    if (cls) {
                        CallContext ctx{&engine_, currentCallEnv()};
                        // classdef `get.Prop` accessor → run its body as a
                        // same-stack VM frame (debuggable, no save/restore),
                        // enforcing the property's GetAccess first. P4,
                        // vm_callbacks_plan.md. Getters return exactly one value
                        // so destReg = I.a is always the right write-back.
                        if (const UserFunction *g = engine_.classGetter(cn, fname)) {
                            if (const BytecodeChunk *gc = engine_.ensureClassMethodChunk(*g)) {
                                engine_.enforcePropGetAccess(cn, fname);
                                Value selfBuf = R[I.b];
                                frame.ip = ip + 1;
                                pushCallFrame(*gc, &selfBuf, 1, I.a, 1, false, 0, 0, cn,
                                              /*isCtor=*/false);
                                goto enter_frame;
                            }
                        }
                        if (cls->propGet) {
                            Value out;
                            if (cls->propGet(R[I.b], fname, out, ctx)) {
                                R[I.a] = std::move(out);
                                break;
                            }
                        }
                        // Bare obj.method (no parens) → no-arg method call.
                        auto mit = cls->methods.find(fname);
                        if (mit != cls->methods.end()) {
                            Value self = R[I.b];
                            Value out[1];
                            mit->second(self, Span<const Value>(nullptr, 0), 1,
                                        Span<Value>(out, 1), ctx);
                            R[I.a] = std::move(out[0]);
                            break;
                        }
                    }
                    throw std::runtime_error("No appropriate property '" + fname
                                             + "' for class '" + R[I.b].objectClassName() + "'");
                }
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
                // OBJECT: read the property (propGet) so a compound lvalue
                // `obj.prop(i) = v` can modify the value and FIELD_SET it back
                // (read-modify-write — objects have no addressable slot).
                if (R[I.b].isObject()) {
                    const BuiltinClass *cls = engine_.findClass(R[I.b].objectClassName());
                    if (cls && cls->propGet) {
                        CallContext ctx{&engine_, currentCallEnv()};
                        Value out;
                        if (cls->propGet(R[I.b], fname, out, ctx)) {
                            R[I.a] = std::move(out);
                            break;
                        }
                    }
                    throw std::runtime_error("No appropriate property '" + fname
                                             + "' for class '" + R[I.b].objectClassName() + "'");
                }
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
                // OBJECT: obj.Prop = val via class property hook (object model).
                if (R[I.a].isObject()) {
                    const std::string &cn = R[I.a].objectClassName();
                    const BuiltinClass *cls = engine_.findClass(cn);
                    if (cls) {
                        CallContext ctx{&engine_, currentCallEnv()};
                        // classdef `set.Prop` accessor → run its body on the VM
                        // (P4). A value/handle setter that returns the object
                        // (`function obj = set.Prop(obj,val)`) runs as a
                        // same-stack frame (pausable, fast) with the modified
                        // object written back into the object register. A
                        // no-output handle setter (`function set.Prop(obj,val)`)
                        // mutates shared state in place; run it re-entrantly so
                        // the object register is not clobbered by an empty RET.
                        if (const UserFunction *s = engine_.classSetter(cn, fname)) {
                            if (const BytecodeChunk *sc = engine_.ensureClassMethodChunk(*s)) {
                                engine_.enforcePropSetAccess(cn, fname);
                                Value argbuf[2] = {R[I.a], R[I.b]};
                                if (!s->returns.empty()) {
                                    frame.ip = ip + 1;
                                    pushCallFrame(*sc, argbuf, 2, I.a, 1, false, 0, 0, cn,
                                                  /*isCtor=*/false);
                                    goto enter_frame;
                                }
                                callReentrant(*sc, Span<const Value>(argbuf, 2), 1, cn,
                                              /*isCtor=*/false);
                                break; // handle mutated via shared state
                            }
                        }
                        if (cls->propSet) {
                            if (cls->propSet(R[I.a], fname, R[I.b], ctx))
                                break;
                        }
                    }
                    throw std::runtime_error("Cannot set property '" + fname
                                             + "' on class '" + R[I.a].objectClassName() + "'");
                }
                if (R[I.a].isEmpty()) {
                    R[I.a] = Value::structure();
                }
                if (!R[I.a].isStruct())
                    throw std::runtime_error("Dot indexing requires a struct");
                // Broadcast: `s.f = val` for a multi-element struct
                // array sets f on every element (MATLAB semantics).
                if (R[I.a].isStructArray()) {
                    R[I.a].setFieldAll(fname, R[I.b]);  // BUG #15
                } else {
                    R[I.a].field(fname) = R[I.b];        // tracks via field()
                }
                break;
            }
            case OpCode::STRUCT_ELEM_FIELD_SET: {
                // a=obj (struct array, in/out), b=idxReg, c=valReg, d=nameIdx
                const std::string &fname = chunk.strings[I.d];
                size_t idx = static_cast<size_t>(R[I.b].toScalar()) - 1;
                Value &obj = R[I.a];
                obj.growStructArrayTo(idx, engine_.resource());
                obj.setField(idx, fname, R[I.c]);  // BUG #15
                break;
            }
            case OpCode::STRUCT_ELEM_GET_OR_CREATE: {
                // a=dst, b=obj (in/out), c=base, e=nargs — d(i…) as a 1×1
                // struct, auto-growing the array (compound-lvalue container).
                Value &obj = R[I.b];
                if (obj.isUnset() || obj.isEmpty())
                    obj = Value::structArray(0, 0, engine_.resource());
                if (!obj.isStruct())
                    throw std::runtime_error("Indexed assignment on a non-struct value");
                const int nargs = I.e;
                size_t coords[Dims::kMaxRank];
                for (int k = 0; k < nargs; ++k)
                    coords[k] = static_cast<size_t>(R[I.c + k].toScalar()) - 1;
                size_t linear;
                if (nargs == 1) {
                    linear = coords[0];
                    obj.growStructArrayTo(linear, engine_.resource());
                } else {
                    linear = obj.growStructArrayND(coords, nargs, engine_.resource());
                }
                R[I.a] = obj.elemAt(linear, engine_.mr_);  // 1×1 struct (empty if vacant)
                break;
            }
            case OpCode::STRUCT_ELEM_SET: {
                // a=obj (in/out), b=base, c=nargs, e=valReg — d(i…) = struct.
                Value &obj = R[I.a];
                if (obj.isUnset() || obj.isEmpty())
                    obj = Value::structArray(0, 0, engine_.resource());
                if (!obj.isStruct())
                    throw std::runtime_error("Indexed assignment on a non-struct value");
                const int nargs = I.c;
                size_t coords[Dims::kMaxRank];
                for (int k = 0; k < nargs; ++k)
                    coords[k] = static_cast<size_t>(R[I.b + k].toScalar()) - 1;
                size_t linear;
                if (nargs == 1) {
                    linear = coords[0];
                    obj.growStructArrayTo(linear, engine_.resource());
                } else {
                    linear = obj.growStructArrayND(coords, nargs, engine_.resource());
                }
                const Value &val = R[I.e];
                if (!val.isStruct())
                    throw std::runtime_error("Cannot assign a non-struct to a struct-array element");
                for (const auto &name : val.fieldNamesInOrder())
                    obj.setField(linear, name, val.field(name));  // BUG #15 order
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
            case OpCode::FIELD_GET_OR_CREATE_DYN: {
                // a=dst, b=obj (in/out), c=nameReg — lvalue dynamic field
                std::string fname = R[I.c].toString();
                if (R[I.b].isEmpty())
                    R[I.b] = Value::structure();
                if (!R[I.b].isStruct())
                    throw std::runtime_error("Dot indexing requires a struct");
                R[I.a] = R[I.b].field(fname);  // field() auto-creates if missing
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
            case OpCode::CELL_APPEND_FLATTEN: {
                // a=acc (in/out), b=elem. First-class cell-literal builder: if R[elem] is
                // a CSL, append all its items; else append it as one element. Driven by
                // the runtime value, so {a, c{idx}, b} flattens for any subscript.
                Value &acc = R[I.a];
                if (!acc.isCell())
                    acc = Value::cell(1, 0);
                size_t       n    = acc.numel();
                const Value &elem = R[I.b];
                if (elem.isCsl()) {
                    size_t m     = elem.cslCount();
                    auto   grown = Value::cell(1, n + m);
                    for (size_t i = 0; i < n; ++i)
                        grown.cellAt(i) = std::move(acc.cellAt(i));
                    for (size_t j = 0; j < m; ++j)
                        grown.cellAt(n + j) = elem.cslAt(j);
                    acc = std::move(grown);
                } else {
                    auto grown = Value::cell(1, n + 1);
                    for (size_t i = 0; i < n; ++i)
                        grown.cellAt(i) = std::move(acc.cellAt(i));
                    grown.cellAt(n) = elem;
                    acc = std::move(grown);
                }
                break;
            }
            case OpCode::CELL_GET: {
                if (!R[I.b].isCell())
                    throw std::runtime_error("Cell indexing requires a cell array");
                const Value &cell = R[I.b];
                const Value &sub  = R[I.c];
                // Hot path: a scalar numeric subscript c{i} -> the bare element, no CSL,
                // no resolveIndices allocation.
                if (sub.isDoubleScalar()) {
                    R[I.a] = cell.cellAt(Value::checkedScalarIndex(sub.scalarVal()));
                    break;
                }
                // Multi-select c{:} / c{vec} / c{range}: one selected index is still a
                // bare single value; N!=1 is a comma-separated list. In a single-value
                // context the compiler-inserted COLLAPSE collapses it (brick 5a); splice
                // consumers flatten it. See dev-docs/memory/csl_first_class.md.
                auto ids = Value::resolveIndices(sub, cell.numel());
                if (ids.size() == 1) {
                    R[I.a] = cell.cellAt(ids[0]);
                } else {
                    Value csl = Value::csl(ids.size());
                    for (size_t k = 0; k < ids.size(); ++k)
                        csl.cslAt(k) = cell.cellAt(ids[k]);
                    R[I.a] = std::move(csl);
                }
                break;
            }
            case OpCode::CELL_GET_OR_CREATE: {
                // a=dst, b=cell (in/out), c=base, e=nargs — c{i…} as a
                // compound-lvalue container: coerce to cell, auto-grow
                // (any rank), return the content slot.
                Value &cell = R[I.b];
                const int nargs = I.e;
                size_t coords[Dims::kMaxRank];
                for (int k = 0; k < nargs; ++k)
                    coords[k] = Value::checkedScalarIndex(R[I.c + k].toScalar());
                size_t linear = cell.growCellTo(coords, nargs, engine_.resource());
                R[I.a] = cell.cellAt(linear);
                break;
            }
            case OpCode::CELL_GET_2D: {
                if (!R[I.b].isCell())
                    throw std::runtime_error("Cell indexing requires a cell array");
                const Value &cell = R[I.b];
                const Value &rsub = R[I.c];
                const Value &csub = R[I.e];
                // Hot path: both scalar c{i,j} -> the bare element.
                if (rsub.isDoubleScalar() && csub.isDoubleScalar()) {
                    size_t r = Value::checkedScalarIndex(rsub.scalarVal());
                    size_t c = Value::checkedScalarIndex(csub.scalarVal());
                    R[I.a] = cell.cellAt(cell.dims().sub2ind(r, c));
                    break;
                }
                // Multi-select slice c{r,:} / c{:,j}: resolveIndices per dim, column-
                // major. 1 selected -> bare element; N!=1 -> a CSL (collapsed in a
                // single-value context, flattened by a splice consumer).
                auto rows = Value::resolveIndices(rsub, cell.dims().rows());
                auto cols = Value::resolveIndices(csub, cell.dims().cols());
                if (rows.size() * cols.size() == 1) {
                    R[I.a] = cell.cellAt(cell.dims().sub2ind(rows[0], cols[0]));
                } else {
                    Value  csl = Value::csl(rows.size() * cols.size());
                    size_t k   = 0;
                    for (size_t c : cols)
                        for (size_t r : rows)
                            csl.cslAt(k++) = cell.cellAt(cell.dims().sub2ind(r, c));
                    R[I.a] = std::move(csl);
                }
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
            case OpCode::CELL_GET_MULTI_2D: {
                // a=outBase, b=cell, c=rowSub, d=colSub, e=nout. resolveIndices per
                // dim + sub2ind, column-major (matches the TreeWalker order). Assigns
                // up to nout selected contents (parity with the 1-D path's truncation).
                if (!R[I.b].isCell())
                    throw std::runtime_error("Cell indexing requires a cell array");
                const Value &cell   = R[I.b];
                auto         rowIdx = Value::resolveIndices(R[I.c], cell.dims().rows());
                auto         colIdx = Value::resolveIndices(R[I.d], cell.dims().cols());
                uint8_t      outBase = I.a, nout = I.e;
                size_t       k = 0;
                for (size_t c : colIdx) {
                    for (size_t r : rowIdx) {
                        if (k >= nout) break;
                        R[outBase + k++] = cell.cellAt(cell.dims().sub2ind(r, c));
                    }
                    if (k >= nout) break;
                }
                break;
            }
            case OpCode::CELL_SET: {
                if (R[I.a].isEmpty())
                    R[I.a] = Value::cell(0, 0);
                if (!R[I.a].isCell())
                    throw std::runtime_error("Cell indexing requires a cell array");
                size_t i = Value::checkedScalarIndex(R[I.b].toScalar());
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
                size_t r = Value::checkedScalarIndex(R[I.b].toScalar()), c = Value::checkedScalarIndex(R[I.c].toScalar());
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
                    size_t r = Value::checkedScalarIndex(R[base].toScalar());
                    size_t c = Value::checkedScalarIndex(R[base + 1].toScalar());
                    size_t p = Value::checkedScalarIndex(R[base + 2].toScalar());
                    size_t idx = R[I.b].dims().sub2ind(r, c, p);
                    R[I.a] = R[I.b].cellAt(idx);
                } else {
                    // General ND (≥4): column-major linear index from
                    // per-axis subscripts using actual dim(i).
                    const auto &d = R[I.b].dims();
                    size_t idx = 0, stride = 1;
                    for (uint8_t i = 0; i < ndims; ++i) {
                        size_t si = Value::checkedScalarIndex(R[base + i].toScalar());
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
                    size_t r = Value::checkedScalarIndex(R[base].toScalar());
                    size_t c = Value::checkedScalarIndex(R[base + 1].toScalar());
                    size_t p = Value::checkedScalarIndex(R[base + 2].toScalar());
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
                        coords[i] = Value::checkedScalarIndex(R[base + i].toScalar());
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
                        R[I.a].resizeND(need.data(), newNd, engine_.mr_);
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

                // OBJECT: ClassName(args) constructs an instance when the
                // name is a registered class (object model, object_model.md).
                {
                    const std::string &ctorName = chunk.strings[funcIdx];
                    if (const BuiltinClass *cls = engine_.findClass(ctorName);
                        cls && cls->construct) {
                        engine_.enforceCtorAccess(ctorName); // private/protected ctor
                        // classdef with a user constructor → run the ctor body as
                        // a VM frame (debuggable), seeding the output variable with
                        // a default instance. The super-ctor / property assigns in
                        // the body then run on the VM too. P2, vm_callbacks_plan.md.
                        if (const UserFunction *cuf = engine_.classCtor(ctorName)) {
                            if (const BytecodeChunk *cc =
                                    engine_.ensureClassMethodChunk(*cuf)) {
                                Value seed = engine_.makeDefaultInstance(ctorName);
                                frame.ip = ip + 1;
                                pushCallFrame(*cc, &R[argBase], na, I.a, nargout_val,
                                              false, 0, 0, ctorName, /*isCtor=*/true,
                                              &seed);
                                goto enter_frame;
                            }
                        }
                        // No user ctor, or ctor body not VM-compilable → C++ path
                        // (default-fill, or the ctor body on the TreeWalker hook).
                        // Access already enforced above, so call construct directly.
                        Span<const Value> as(&R[argBase], na);
                        CallContext ctx{&engine_, currentCallEnv()};
                        R[I.a] = cls->construct(as, ctx);
                        break;
                    }
                }

                // OBJECT function-form: m(obj, ...) where obj's class has
                // method m beats a same-named global function.
                if (na >= 1 && R[argBase].isObject()) {
                    const std::string &mnm = chunk.strings[funcIdx];
                    const BuiltinClass *cls = engine_.findClass(R[argBase].objectClassName());
                    if (cls) {
                        // classdef method → run its body as a native VM frame
                        // (debuggable). Args are already contiguous at argBase
                        // with self == args[0]. The C++ hook (below) is kept
                        // only for native builtin-class methods.
                        auto fit = cls->methodFns.find(mnm);
                        if (fit != cls->methodFns.end()) {
                            const BytecodeChunk *mc = engine_.ensureClassMethodChunk(*fit->second);
                            if (mc) {
                                engine_.enforceMethodAccess(R[argBase].objectClassName(), mnm);
                                frame.ip = ip + 1;
                                pushCallFrame(*mc, &R[argBase], na, I.a, nargout_val, false, 0, 0,
                                              fit->second->ownerClass, false);
                                goto enter_frame;
                            }
                        }
                        if (cls->methods.count(mnm)) {
                            Value self = R[argBase];
                            Span<const Value> rest((na > 1) ? &R[argBase + 1] : nullptr, na - 1);
                            Value out[1];
                            CallContext ctx{&engine_, currentCallEnv()};
                            cls->methods.at(mnm)(self, rest, nargout_val, Span<Value>(out, 1),
                                                 ctx);
                            R[I.a] = std::move(out[0]);
                            break;
                        }
                    }
                }

                // Resolution order matches MATLAB: user-on-path beats
                // builtins. The compiled-cache check above already
                // handled functions adopted earlier in the session.
                // BUG #1 fix: m-file lookup MUST run before findExternal
                // so a user split.m on the path shadows the builtin
                // `split`. Previously the order was builtin → m-file
                // (CALL_MULTI/VM picked the builtin and broke
                // [a,b] = split(5) destructure).
                {
                    const std::string &funcName = chunk.strings[funcIdx];

                    // M-file lookup (Phase 9a; Phase 10 adds packages).
                    // Compiler::registerFunctionAs binds the chunk under
                    // the canonical name returned by lookupUserFunction —
                    // qualified for +pkg/foo.m, bare for plain foo.m.
                    if (auto *uf =
                            engine_.lookupUserFunction(funcName,
                                                        currentCallEnv())) {
                        if (const BytecodeChunk *found = findCompiledFunc(uf->name)) {
                            if (funcIdx >= (int16_t) resolvedFuncs.size())
                                resolvedFuncs.resize(funcIdx + 1, nullptr);
                            resolvedFuncs[funcIdx] = found;
                            frame.ip = ip + 1;
                            pushCallFrame(*found, &R[argBase], na, I.a, nargout_val);
                            goto enter_frame;
                        }
                    }

                    // Higher-order builtin (cellfun/arrayfun/…) whose function
                    // handle is user code: drive its callbacks as pausable VM
                    // frames via a continuation, instead of the synchronous
                    // builtin (whose callbacks would run on the non-pausable
                    // callReentrant path). tryStart returns nullptr — fall
                    // through to the synchronous builtin below — for a builtin
                    // handle or an arg form the state machine doesn't cover.
                    if (CallbackBuiltin *cb = engine_.callbackBuiltin(funcName)) {
                        Span<const Value> as(&R[argBase], na);
                        if (auto cont = cb->tryStart(as, nargout_val, &R[I.a], engine_)) {
                            frame.ip = ip + 1;
                            if (startContinuation(std::move(cont)))
                                goto enter_frame; // suspended on first callback frame
                            break;                // finished synchronously (empty input)
                        }
                    }

                    // External (builtin) function — call directly (no frame push).
                    const ExternalFunc *fnPtr = engine_.findExternal(
                        funcName, currentCallEnv());
                    if (fnPtr) {
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
                        CallContext ctx{&engine_, currentCallEnv()};
                        (*fnPtr)(as, nargout_val, os, ctx);
                        R[I.a] = std::move(ob[0]);
                        break;
                    }
                    throw std::runtime_error("VM: undefined function '" + funcName + "'");
                }
            }

            case OpCode::CALL_FLATTEN: {
                // First-class call with possible CSL args: flatten any comma-separated
                // list arg into a runtime arg vector, then run the SAME full target
                // dispatch as CALL (user fn / ctor / object method / m-file / callback /
                // external) over AB/NF. Emitted only when an arg could be a CSL, so the
                // hot no-CSL CALL is untouched. pushCallFrame copies args, so flatArgs may
                // die after the goto. See dev-docs/memory/csl_first_class.md.
                uint8_t argBase = I.b, na = I.c;
                uint8_t nargout_val = I.e;
                int16_t funcIdx = I.d;
                // Fast path: no arg is actually a CSL (the common f(c{i}) scalar case) ->
                // use the contiguous arg registers directly, no copy. Only build the
                // flattened vector when a CSL is present.
                bool anyCsl = false;
                for (uint8_t i = 0; i < na; ++i)
                    if (R[argBase + i].isCsl()) { anyCsl = true; break; }
                std::vector<Value> flatArgs;
                const Value       *AB;
                uint8_t            NF;
                if (!anyCsl) {
                    AB = na ? &R[argBase] : nullptr;
                    NF = na;
                } else {
                    flatArgs.reserve(na);
                    for (uint8_t i = 0; i < na; ++i) {
                        const Value &a = R[argBase + i];
                        if (a.isCsl())
                            for (size_t k = 0; k < a.cslCount(); ++k)
                                flatArgs.push_back(a.cslAt(k));
                        else
                            flatArgs.push_back(a);
                    }
                    AB = flatArgs.empty() ? nullptr : flatArgs.data();
                    NF = static_cast<uint8_t>(flatArgs.size());
                }

                const BytecodeChunk *targetChunk = nullptr;
                if (funcIdx < (int16_t) resolvedFuncs.size() && resolvedFuncs[funcIdx])
                    targetChunk = resolvedFuncs[funcIdx];
                if (!targetChunk) {
                    const std::string &funcName = chunk.strings[funcIdx];
                    if (const BytecodeChunk *found = findCompiledFunc(funcName)) {
                        if (funcIdx >= (int16_t) resolvedFuncs.size())
                            resolvedFuncs.resize(funcIdx + 1, nullptr);
                        resolvedFuncs[funcIdx] = found;
                        targetChunk = found;
                    }
                }
                if (targetChunk) {
                    frame.ip = ip + 1;
                    pushCallFrame(*targetChunk, AB, NF, I.a, nargout_val);
                    goto enter_frame;
                }

                {
                    const std::string &ctorName = chunk.strings[funcIdx];
                    if (const BuiltinClass *cls = engine_.findClass(ctorName);
                        cls && cls->construct) {
                        engine_.enforceCtorAccess(ctorName);
                        if (const UserFunction *cuf = engine_.classCtor(ctorName)) {
                            if (const BytecodeChunk *cc =
                                    engine_.ensureClassMethodChunk(*cuf)) {
                                Value seed = engine_.makeDefaultInstance(ctorName);
                                frame.ip = ip + 1;
                                pushCallFrame(*cc, AB, NF, I.a, nargout_val,
                                              false, 0, 0, ctorName, /*isCtor=*/true,
                                              &seed);
                                goto enter_frame;
                            }
                        }
                        Span<const Value> as(AB, NF);
                        CallContext ctx{&engine_, currentCallEnv()};
                        R[I.a] = cls->construct(as, ctx);
                        break;
                    }
                }

                if (NF >= 1 && AB[0].isObject()) {
                    const std::string &mnm = chunk.strings[funcIdx];
                    const BuiltinClass *cls = engine_.findClass(AB[0].objectClassName());
                    if (cls) {
                        auto fit = cls->methodFns.find(mnm);
                        if (fit != cls->methodFns.end()) {
                            const BytecodeChunk *mc = engine_.ensureClassMethodChunk(*fit->second);
                            if (mc) {
                                engine_.enforceMethodAccess(AB[0].objectClassName(), mnm);
                                frame.ip = ip + 1;
                                pushCallFrame(*mc, AB, NF, I.a, nargout_val, false, 0, 0,
                                              fit->second->ownerClass, false);
                                goto enter_frame;
                            }
                        }
                        if (cls->methods.count(mnm)) {
                            Value self = AB[0];
                            Span<const Value> rest((NF > 1) ? &AB[1] : nullptr, NF - 1);
                            Value out[1];
                            CallContext ctx{&engine_, currentCallEnv()};
                            cls->methods.at(mnm)(self, rest, nargout_val, Span<Value>(out, 1),
                                                 ctx);
                            R[I.a] = std::move(out[0]);
                            break;
                        }
                    }
                }

                {
                    const std::string &funcName = chunk.strings[funcIdx];
                    if (auto *uf =
                            engine_.lookupUserFunction(funcName, currentCallEnv())) {
                        if (const BytecodeChunk *found = findCompiledFunc(uf->name)) {
                            if (funcIdx >= (int16_t) resolvedFuncs.size())
                                resolvedFuncs.resize(funcIdx + 1, nullptr);
                            resolvedFuncs[funcIdx] = found;
                            frame.ip = ip + 1;
                            pushCallFrame(*found, AB, NF, I.a, nargout_val);
                            goto enter_frame;
                        }
                    }
                    if (CallbackBuiltin *cb = engine_.callbackBuiltin(funcName)) {
                        Span<const Value> as(AB, NF);
                        if (auto cont = cb->tryStart(as, nargout_val, &R[I.a], engine_)) {
                            frame.ip = ip + 1;
                            if (startContinuation(std::move(cont)))
                                goto enter_frame;
                            break;
                        }
                    }
                    const ExternalFunc *fnPtr = engine_.findExternal(
                        funcName, currentCallEnv());
                    if (fnPtr) {
                        Span<const Value> as(AB, NF);
                        Value             ob[1];
                        Span<Value>       os(ob, 1);
                        CallContext       ctx{&engine_, currentCallEnv()};
                        (*fnPtr)(as, nargout_val, os, ctx);
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

                // Try compiled user function first.
                if (const BytecodeChunk *found = findCompiledFunc(funcName)) {
                    frame.ip = ip + 1;
                    returnCount_ = 0;
                    pushCallFrame(*found, &R[argBase], na,
                                  0, nout, true, outBase, nout);
                    goto enter_frame;
                }

                // OBJECT function-form multi-output: [a,b] = m(obj, ...).
                // A class method on the dominant (first) object argument
                // beats a same-named path function.
                if (na >= 1 && R[argBase].isObject()) {
                    const BuiltinClass *cls =
                        engine_.findClass(R[argBase].objectClassName());
                    if (cls && cls->methods.count(funcName)) {
                        Value self = R[argBase];
                        Span<const Value> rest((na > 1) ? &R[argBase + 1] : nullptr, na - 1);
                        std::vector<Value> outBuf(nout);
                        CallContext ctx{&engine_, currentCallEnv()};
                        cls->methods.at(funcName)(self, rest, nout,
                                                  Span<Value>(outBuf.data(), nout), ctx);
                        for (uint8_t i = 0; i < nout; ++i)
                            if (outBuf[i].isUnset())
                                throw std::runtime_error("Too many output arguments.");
                        for (size_t i = 0; i < nout; ++i)
                            R[outBase + i] = std::move(outBuf[i]);
                        break;
                    }
                }
                // BUG #1 fix: m-file resolution must beat builtin
                // resolution to match MATLAB. Was builtin → m-file —
                // a user `split.m` couldn't shadow the builtin `split`,
                // so [a,b] = split(5) ended up calling the 1-output
                // builtin and failing the destructure.
                {
                    // M-file lookup (Phase 9a / 10). Same canonical-name
                    // dispatch as CALL above.
                    if (auto *uf =
                            engine_.lookupUserFunction(funcName,
                                                        currentCallEnv())) {
                        if (const BytecodeChunk *found = findCompiledFunc(uf->name)) {
                            frame.ip = ip + 1;
                            returnCount_ = 0;
                            pushCallFrame(*found, &R[argBase], na,
                                          0, nout, true, outBase, nout);
                            goto enter_frame;
                        }
                    }
                    // External function with nout — call directly.
                    const ExternalFunc *fnPtr = engine_.findExternal(
                        funcName, currentCallEnv());
                    if (fnPtr) {
                        std::vector<Value> outBuf(nout);
                        Span<const Value> as(&R[argBase], na);
                        Span<Value> os(outBuf.data(), nout);
                        CallContext ctx{&engine_, currentCallEnv()};
                        (*fnPtr)(as, nout, os, ctx);
                        // MATLAB: asking for more outputs than the builtin
                        // produces is "Too many output arguments" here, not
                        // a silently-unset target that later reads as a
                        // phantom undefined function (bug #44).
                        for (uint8_t i = 0; i < nout; ++i)
                            if (outBuf[i].isUnset())
                                throw std::runtime_error("Too many output arguments.");
                        for (size_t i = 0; i < nout; ++i)
                            R[outBase + i] = std::move(outBuf[i]);
                        break;
                    }
                    throw std::runtime_error("VM: undefined function '" + funcName + "'");
                }
            }

            case OpCode::CALL_FLATTEN_MULTI: {
                // First-class multi-output call with possible CSL args: flatten any CSL
                // arg into a runtime arg vector, then run the CALL_MULTI target dispatch
                // (compiled user fn / object method / m-file / external) over AB/NF.
                // a=outBase, b=argBase, c=nargs, d=funcIdx, e=nout.
                uint8_t outBase = I.a, argBase = I.b, na = I.c, nout = I.e;
                int16_t funcIdx = I.d;
                const std::string &funcName = chunk.strings[funcIdx];
                bool anyCsl = false;
                for (uint8_t i = 0; i < na; ++i)
                    if (R[argBase + i].isCsl()) { anyCsl = true; break; }
                std::vector<Value> flatArgs;
                const Value       *AB;
                uint8_t            NF;
                if (!anyCsl) {
                    AB = na ? &R[argBase] : nullptr;
                    NF = na;
                } else {
                    flatArgs.reserve(na);
                    for (uint8_t i = 0; i < na; ++i) {
                        const Value &a = R[argBase + i];
                        if (a.isCsl())
                            for (size_t k = 0; k < a.cslCount(); ++k)
                                flatArgs.push_back(a.cslAt(k));
                        else
                            flatArgs.push_back(a);
                    }
                    AB = flatArgs.empty() ? nullptr : flatArgs.data();
                    NF = static_cast<uint8_t>(flatArgs.size());
                }

                if (const BytecodeChunk *found = findCompiledFunc(funcName)) {
                    frame.ip = ip + 1;
                    returnCount_ = 0;
                    pushCallFrame(*found, AB, NF, 0, nout, true, outBase, nout);
                    goto enter_frame;
                }
                if (NF >= 1 && AB[0].isObject()) {
                    const BuiltinClass *cls = engine_.findClass(AB[0].objectClassName());
                    if (cls && cls->methods.count(funcName)) {
                        Value              self = AB[0];
                        Span<const Value>  rest((NF > 1) ? &AB[1] : nullptr, NF - 1);
                        std::vector<Value> outBuf(nout);
                        CallContext        ctx{&engine_, currentCallEnv()};
                        cls->methods.at(funcName)(self, rest, nout,
                                                  Span<Value>(outBuf.data(), nout), ctx);
                        for (uint8_t i = 0; i < nout; ++i)
                            if (outBuf[i].isUnset())
                                throw std::runtime_error("Too many output arguments.");
                        for (size_t i = 0; i < nout; ++i)
                            R[outBase + i] = std::move(outBuf[i]);
                        break;
                    }
                }
                {
                    if (auto *uf =
                            engine_.lookupUserFunction(funcName, currentCallEnv())) {
                        if (const BytecodeChunk *found = findCompiledFunc(uf->name)) {
                            frame.ip = ip + 1;
                            returnCount_ = 0;
                            pushCallFrame(*found, AB, NF, 0, nout, true, outBase, nout);
                            goto enter_frame;
                        }
                    }
                    const ExternalFunc *fnPtr = engine_.findExternal(
                        funcName, currentCallEnv());
                    if (fnPtr) {
                        std::vector<Value> outBuf(nout);
                        Span<const Value>  as(AB, NF);
                        Span<Value>        os(outBuf.data(), nout);
                        CallContext        ctx{&engine_, currentCallEnv()};
                        (*fnPtr)(as, nout, os, ctx);
                        for (uint8_t i = 0; i < nout; ++i)
                            if (outBuf[i].isUnset())
                                throw std::runtime_error("Too many output arguments.");
                        for (size_t i = 0; i < nout; ++i)
                            R[outBase + i] = std::move(outBuf[i]);
                        break;
                    }
                    throw std::runtime_error("VM: undefined function '" + funcName + "'");
                }
            }

            // ── Indirect function call (func handle) or array indexing ─
            case OpCode::CALL_METHOD: {
                // a=dst, b=objReg, c=argBase, d=nameIdx, e=nargs.
                // obj.name(args): dispatch a class method, else fall back
                // to field-value + CALL_INDIRECT (func handle / index).
                const std::string &mname = chunk.strings[I.d];
                const Value &obj = R[I.b];
                if (obj.isObject()) {
                    const BuiltinClass *cls = engine_.findClass(obj.objectClassName());
                    // classdef method → native VM frame (debuggable). Receiver
                    // and args aren't contiguous, so build [self, args…].
                    if (cls) {
                        auto fit = cls->methodFns.find(mname);
                        if (fit != cls->methodFns.end()) {
                            const BytecodeChunk *mc = engine_.ensureClassMethodChunk(*fit->second);
                            if (mc) {
                                engine_.enforceMethodAccess(obj.objectClassName(), mname);
                                std::vector<Value> frameArgs;
                                frameArgs.reserve(static_cast<size_t>(I.e) + 1);
                                frameArgs.push_back(obj);
                                for (uint8_t k = 0; k < I.e; ++k)
                                    frameArgs.push_back(R[I.c + k]);
                                frame.ip = ip + 1;
                                pushCallFrame(*mc, frameArgs.data(),
                                              static_cast<uint8_t>(frameArgs.size()), I.a, 1, false,
                                              0, 0, fit->second->ownerClass, false);
                                goto enter_frame;
                            }
                        }
                    }
                    if (cls && cls->methods.count(mname)) {
                        Value self = obj; // handle: shares state; value: own copy
                        Span<const Value> args((I.e ? &R[I.c] : nullptr), I.e);
                        Value out[1];
                        CallContext ctx{&engine_, currentCallEnv()};
                        cls->methods.at(mname)(self, args, 1, Span<Value>(out, 1), ctx);
                        R[I.a] = std::move(out[0]);
                        break;
                    }
                    // Not a method → property read, then index the result.
                    Value out;
                    CallContext ctx{&engine_, currentCallEnv()};
                    if (!cls || !cls->propGet || !cls->propGet(obj, mname, out, ctx))
                        throw std::runtime_error("No appropriate property '" + mname
                                                 + "' for class '" + obj.objectClassName() + "'");
                    if (execCallIndirectTarget(out, I.a, I.c, I.e, R, frame, ip))
                        goto enter_frame;
                } else {
                    // Struct field holding a func handle, or a value to index
                    // (`s.fh(args)`, `s.arr(idx)`).
                    if (!obj.isStruct())
                        throw std::runtime_error("Dot indexing requires a struct");
                    if (!obj.hasField(mname))
                        throw std::runtime_error("Reference to non-existent field '" + mname + "'");
                    Value fv = obj.field(mname);
                    if (execCallIndirectTarget(fv, I.a, I.c, I.e, R, frame, ip))
                        goto enter_frame;
                }
                break;
            }
            // ── Dotted multi-output method: [a,b] = obj.m(args) ──
            case OpCode::CALL_METHOD_MULTI: {
                // a=outBase, b=objReg, c=argBase, d=nameIdx,
                // e=(nargs<<4)|nout (each nibble ≤15).
                uint8_t outBase = I.a, objReg = I.b, argBase = I.c;
                uint8_t na = static_cast<uint8_t>((I.e >> 4) & 0x0F);
                uint8_t nout = static_cast<uint8_t>(I.e & 0x0F);
                const std::string &mname = chunk.strings[I.d];
                Value &obj = R[objReg];
                if (!obj.isObject())
                    throw std::runtime_error(
                        "Dot-indexing multi-output requires an object; '" + mname
                        + "' is not a method of a " + std::string(mtypeName(obj.type())));
                const BuiltinClass *cls = engine_.findClass(obj.objectClassName());
                // classdef method → native multi-output VM frame.
                if (cls) {
                    auto fit = cls->methodFns.find(mname);
                    if (fit != cls->methodFns.end()) {
                        const BytecodeChunk *mc = engine_.ensureClassMethodChunk(*fit->second);
                        if (mc) {
                            engine_.enforceMethodAccess(obj.objectClassName(), mname);
                            std::vector<Value> frameArgs;
                            frameArgs.reserve(static_cast<size_t>(na) + 1);
                            frameArgs.push_back(obj);
                            for (uint8_t k = 0; k < na; ++k)
                                frameArgs.push_back(R[argBase + k]);
                            frame.ip = ip + 1;
                            pushCallFrame(*mc, frameArgs.data(),
                                          static_cast<uint8_t>(frameArgs.size()), 0, nout, true,
                                          outBase, nout, fit->second->ownerClass, false);
                            goto enter_frame;
                        }
                    }
                }
                if (!cls || !cls->methods.count(mname))
                    throw std::runtime_error("No appropriate method '" + mname
                                             + "' for class '" + obj.objectClassName() + "'");
                Value self = obj; // handle: shares state; value: own copy
                Span<const Value> args((na ? &R[argBase] : nullptr), na);
                std::vector<Value> outBuf(nout);
                CallContext ctx{&engine_, currentCallEnv()};
                cls->methods.at(mname)(self, args, nout, Span<Value>(outBuf.data(), nout), ctx);
                for (uint8_t i = 0; i < nout; ++i)
                    if (outBuf[i].isUnset())
                        throw std::runtime_error("Too many output arguments.");
                for (size_t i = 0; i < nout; ++i)
                    R[outBase + i] = std::move(outBuf[i]);
                break;
            }
            // ── Superclass calls inside a classdef body ──
            case OpCode::CALL_SUPER_CTOR: {
                // obj = obj@Base(args): a=dst, b=objReg(seed), c=argBase,
                // d=baseNameIdx, e=nargs.
                const std::string &base = chunk.strings[I.d];
                Span<const Value> args((I.e ? &R[I.c] : nullptr), I.e);
                R[I.a] = engine_.superConstruct(base, R[I.b], args);
                break;
            }
            case OpCode::CALL_SUPER_METHOD: {
                // [outs] = method@Base(obj, args): a=outBase, b=argBase
                // (obj at [0]), c=nargs, d=idx of "Base>method", e=nout.
                const std::string &qual = chunk.strings[I.d];
                size_t gt = qual.find('>');
                std::string base = qual.substr(0, gt);
                std::string method = qual.substr(gt + 1);
                uint8_t outBase = I.a, argBase = I.b, na = I.c, nout = I.e;
                Span<const Value> args((na ? &R[argBase] : nullptr), na);
                auto results = engine_.superMethod(base, method, args,
                                                   std::max<size_t>(nout, 1));
                for (uint8_t i = 0; i < nout; ++i)
                    R[outBase + i] = (i < results.size()) ? std::move(results[i]) : Value();
                break;
            }
            case OpCode::CALL_INDIRECT:
                if (execCallIndirect(I, R, frame, ip))
                    goto enter_frame;
                break;

            case OpCode::CALL_INDIRECT_MULTI:
                if (execCallIndirectMulti(I, R, frame, ip))
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
            case OpCode::RET_VARARGOUT: {
                // a=fixedBase, b=numFixed, c=varargoutReg. Dynamic return
                // count = numFixed + numel(varargout cell).
                uint8_t fixedBase = I.a, numFixed = I.b, vaReg = I.c;
                uint8_t count = 0;
                for (uint8_t i = 0; i < numFixed && count < kMaxReturns; ++i)
                    returnBuf_[count++] = R[fixedBase + i];
                const Value &va = R[vaReg];
                if (va.isCell()) {
                    const size_t n = va.numel();
                    for (size_t i = 0; i < n && count < kMaxReturns; ++i)
                        returnBuf_[count++] = va.cellAt(i);
                }
                returnCount_ = count;
                popCallFrame(count > 0 ? Value(returnBuf_[0]) : Value());
                goto enter_frame;
            }
            case OpCode::RET_EMPTY:
                popCallFrame(Value());
                goto enter_frame;
            case OpCode::HALT:
                popCallFrame(Value());
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

                R[I.a] = Value::scalar(code, engine_.mr_);
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
        std::string id = mle.identifier().empty() ? "numkit:error" : mle.identifier();
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
        if (stopOnError_ && debugCtl()) {
            // dbstop if error: pause at the failing instruction with frames
            // intact; resumeExecution() rethrows the stored exception later.
            frame.ip = ip;
            errorPauseMsg_ = mle.what();
            pausedError_ = std::current_exception();
            atErrorPause_ = true;
            return ExecStatus::Paused;
        }
        throw;
    } catch (const std::exception &ex) {
        if (dispatchTryCatch(ex.what(), "numkit:error"))
            goto enter_frame;
        if (stopOnError_ && debugCtl()) {
            frame.ip = ip;
            errorPauseMsg_ = ex.what();
            pausedError_ = std::current_exception();
            atErrorPause_ = true;
            return ExecStatus::Paused;
        }
        enrichAndThrow(ex, ip, chunk);
    }

    // Fell off end of bytecode — implicit return empty
    popCallFrame(Value());
    goto enter_frame;
    } // scope for frame locals
}

// ============================================================
// Variable export from top-level frame
// ============================================================

// Rebuild `out` (lastVarMap_) as a chunk's assigned LOCAL variables, for syncing
// back to the workspace. Only names the chunk actually wrote to are exported
// (assignedVars) — reading `pi` or another reserved name must not shadow it in
// the base workspace. Globals are excluded: their value round-trips through
// globalsEnv_, and a bare `global X` is "assigned" with an unset register that
// would otherwise make syncVMToWorkspace remove() it (cascading into globalsEnv_
// and clobbering the live value). Global membership is O(1) via a set built once
// — and skipped entirely when the chunk declares no globals (the common case).
static void rebuildLocalVarMap(std::vector<std::pair<std::string, Value>> &out,
                               const BytecodeChunk &chunk, const Value *R, uint8_t nregs)
{
    out.clear();
    const auto &assigned = chunk.assignedVars;
    if (chunk.globalNames.empty()) {
        for (auto &[name, reg] : chunk.varMap)
            if (reg < nregs && assigned.count(name))
                out.push_back({name, R[reg]});
        return;
    }
    std::unordered_set<std::string_view> globals(chunk.globalNames.begin(),
                                                 chunk.globalNames.end());
    for (auto &[name, reg] : chunk.varMap)
        if (reg < nregs && assigned.count(name) && !globals.count(name))
            out.push_back({name, R[reg]});
}

void VM::exportTopLevelVariables()
{
    if (frames_.empty())
        return;

    CallFrame &topFrame = frames_[0];
    rebuildLocalVarMap(lastVarMap_, *topFrame.chunk, topFrame.R, topFrame.nregs);

    // Export top-level globals: write the value to globalsEnv_ only. No
    // local-storage mirror into workspaceEnv_ — that dual-storage is what
    // diverged who/whos between the engines. Base-workspace membership
    // (workspaceEnv_->globals_) is recorded by Engine::runOneChunk's
    // updateTopLevelGlobals; who/whos enumerate it.
    for (auto &gname : topFrame.chunk->globalNames) {
        for (auto &[vname, reg] : topFrame.chunk->varMap) {
            if (vname == gname && reg < topFrame.nregs) {
                if (!topFrame.R[reg].isUnset() && !topFrame.R[reg].isDeleted())
                    engine_.globalsEnv_->set(gname, topFrame.R[reg]);
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
                f.R[i] = Value();
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
    err.field("message") = Value::fromString(msg, engine_.mr_);
    err.field("identifier") = Value::fromString(identifier, engine_.mr_);
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
        const char *nm = builtinIdName(instr.d);
        return nm ? std::string("in call to '") + nm + "'" : "in builtin call";
    }
    case OpCode::CALL_INDIRECT:
    case OpCode::CALL_INDIRECT_MULTI:
        return "in function call";
    case OpCode::CALL_METHOD:
    case OpCode::CALL_METHOD_MULTI: {
        int16_t nameIdx = instr.d;
        if (nameIdx >= 0 && nameIdx < (int16_t)chunk.strings.size())
            return "in method call '." + chunk.strings[nameIdx] + "'";
        return "in method call";
    }

    // Cell indexing
    case OpCode::CELL_GET:
    case OpCode::CELL_GET_OR_CREATE:
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
    case OpCode::FIELD_GET_OR_CREATE_DYN:
    case OpCode::FIELD_SET_DYN:
        return "in dynamic field access";
    case OpCode::STRUCT_ELEM_FIELD_SET: {
        int16_t nameIdx = instr.d;
        if (nameIdx >= 0 && nameIdx < (int16_t)chunk.strings.size())
            return "in struct-array element write '." + chunk.strings[nameIdx] + "'";
        return "in struct-array element write";
    }
    case OpCode::STRUCT_ELEM_GET_OR_CREATE:
    case OpCode::STRUCT_ELEM_SET:
        return "in struct-array element access";

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

Environment *VM::currentCallEnv()
{
    // Frame 0 == top-level script: builtins see workspaceEnv directly,
    // matching script-level eval(). Frame >= 1 == user function call:
    // give it a private env so e.g. `import a.*` inside the function
    // doesn't leak after return. Lazy — only allocated on first builtin
    // that asks (most frames never need it).
    //
    // If `inheritedScope_` is set (re-entrant eval invoked with an
    // explicit scope by Engine::eval(src, scope)), the inner top-level
    // routes ctx.env there instead of the global workspace. Function
    // frames keep their own private env regardless.
    if (frames_.size() <= 1)
        return inheritedScope_ ? inheritedScope_ : &engine_.workspaceEnv();
    auto &frame = frames_.back();
    if (!frame.env) {
        frame.env = std::make_unique<Environment>(
            &engine_.workspaceEnv(),
            engine_.globalsEnv_.get());
    }
    return frame.env.get();
}

Environment *VM::callerEnvAtDepth(int n)
{
    // Frame index 0 is the top-level script frame; user-function frames
    // start at 1. The "caller" of the running builtin (n=0) is the last
    // user-function frame (frames_.back()) when we're inside one, or
    // the top-level script frame otherwise.
    //
    // n=0 → frames_.back() if it's a user-function frame, else workspace.
    // n=1 → one frame up. Etc. Walking off the top → workspaceEnv.
    if (n < 0) n = 0;
    int target = static_cast<int>(frames_.size()) - 1 - n;
    if (target < 1)
        return &engine_.workspaceEnv();
    auto &frame = frames_[target];
    if (!frame.env) {
        frame.env = std::make_unique<Environment>(
            &engine_.workspaceEnv(),
            engine_.globalsEnv_.get());
    }
    return frame.env.get();
}

void VM::assignInCallerFrame(int n, const std::string &name, const Value &val)
{
    if (n < 0) n = 0;
    int target = static_cast<int>(frames_.size()) - 1 - n;
    if (target < 1)
        return;  // top-level / out-of-range — env-side write done by Engine
    auto &frame = frames_[target];
    // varMap is vector<pair<name, regIdx>> (small), linear scan.
    for (const auto &entry : frame.chunk->varMap) {
        if (entry.first == name && entry.second < frame.nregs) {
            frame.R[entry.second] = val;
            return;
        }
    }
}

void VM::writeToFrameMatchingEnv(Environment *env, const std::string &name,
                                 const Value &val)
{
    if (!env) return;
    for (auto &f : frames_) {
        if (f.env.get() != env) continue;
        for (const auto &entry : f.chunk->varMap) {
            if (entry.first == name && entry.second < f.nregs) {
                f.R[entry.second] = val;
                return;
            }
        }
        return;  // matching frame found but no slot — no-op
    }
}

std::unordered_map<std::string, Value> VM::snapshotFrameVars(Environment *frameEnv)
{
    std::unordered_map<std::string, Value> out;
    if (!frameEnv) return out;
    // Find the frame that owns this env.
    const CallFrame *match = nullptr;
    for (const auto &f : frames_) {
        if (f.env.get() == frameEnv) { match = &f; break; }
    }
    if (match) {
        // Statically-allocated locals (live in registers).
        for (const auto &entry : match->chunk->varMap) {
            if (entry.second < match->nregs) {
                const Value &v = match->R[entry.second];
                if (!v.isUnset() && !v.isDeleted())
                    out.emplace(entry.first, v);
            }
        }
        // Existing dynVars overlay (assignin'd / debug-injected names).
        if (match->dynVars) {
            for (const auto &kv : *match->dynVars) {
                if (!kv.second.isUnset() && !kv.second.isDeleted())
                    out[kv.first] = kv.second;
            }
        }
    }
    return out;
}

void VM::pushCallFrame(const BytecodeChunk &funcChunk, const Value *args, uint8_t nargs,
                       uint8_t destReg, size_t nargout,
                       bool isMulti, uint8_t outBase, uint8_t nout,
                       const std::string &ownerClass, bool isCtor,
                       const Value *ctorSeed)
{
    if (callDepth() >= maxRecursion_)
        throw std::runtime_error("VM: maximum recursion depth exceeded");

    // varargin: the LAST declared parameter absorbs extras (1xN cell,
    // empty 0x0 when none) — same MATLAB semantics as TreeWalker.
    const bool va = !funcChunk.paramNames.empty()
                    && funcChunk.paramNames.back() == "varargin";
    if (!va && nargs > funcChunk.numParams)
        throw std::runtime_error("Too many input arguments for function '" + funcChunk.name + "'");
    if (va && nargs < funcChunk.numParams - 1)
        throw std::runtime_error("Not enough input arguments for function '" + funcChunk.name + "'");

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
        newR[i] = Value();

    for (uint8_t i = 0; i < pc; ++i)
        newR[i] = std::move(argsCopy[i]);

    // Pack the varargin cell into its declared parameter register. All
    // reads of `args` happen before any newR write below, so the aliasing
    // concern that motivated argsCopy does not apply here.
    if (va) {
        const uint8_t required = funcChunk.numParams - 1;
        Value vaCell = nargs > required
                           ? Value::cell(1, nargs - required, engine_.mr_)
                           : Value::cell(0, 0, engine_.mr_);
        for (uint8_t i = required; i < nargs; ++i)
            vaCell.cellAt(i - required) = args[i];
        for (auto &[vname, reg] : funcChunk.varMap)
            if (vname == "varargin" && reg < nregs) {
                newR[reg] = std::move(vaCell);
                break;
            }
    }

    // Inject nargin/nargout into function scope
    for (auto &[vname, reg] : funcChunk.varMap) {
        if (vname == "nargin" && reg < nregs)
            newR[reg] = Value::scalar(static_cast<double>(nargs), nullptr);
        else if (vname == "nargout" && reg < nregs)
            newR[reg] = Value::scalar(static_cast<double>(nargout), nullptr);
    }

    // Constructor seed: bind the output variable to the default instance before
    // the body runs (MATLAB seeds `obj` so the ctor only fills/overrides it).
    if (ctorSeed && !funcChunk.returnNames.empty()) {
        const std::string &rn = funcChunk.returnNames[0];
        for (auto &[vname, reg] : funcChunk.varMap)
            if (vname == rn && reg < nregs) {
                newR[reg] = *ctorSeed;
                break;
            }
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

    // Pre-compute caller's CALL-site arg names so the new frame can
    // expose them via inputname(k). caller's frame.ip was advanced past
    // the CALL instruction at the dispatch site, so CALL is at ip-1.
    std::vector<std::string> callerArgNames;
    if (!frames_.empty()) {
        const auto &callerFrame = frames_.back();
        if (callerFrame.chunk && callerFrame.ip
            && callerFrame.ip > callerFrame.chunk->code.data()) {
            size_t callIdx = static_cast<size_t>(
                (callerFrame.ip - 1) - callerFrame.chunk->code.data());
            auto it = callerFrame.chunk->callSiteArgNames.find(
                static_cast<uint32_t>(callIdx));
            if (it != callerFrame.chunk->callSiteArgNames.end())
                callerArgNames = it->second;
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
    cf.ownerClass = ownerClass;
    cf.isCtor = isCtor;
    cf.callerArgNames = std::move(callerArgNames);
    frames_.push_back(std::move(cf));

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

    // State-machine callback: detach the continuation so we can route this
    // frame's result into it (instead of a caller register) after the frame's
    // bookkeeping is unwound below.
    std::shared_ptr<VmContinuation> cont = std::move(frame.cont);

    // Update parent frame's registers BEFORE popFrame —
    // so onFunctionExit sees correct parent variables.
    if (auto *ctl = debugCtl()) {
        auto &stack = ctl->callStack();
        if (stack.size() >= 2 && frames_.size() >= 2)
            stack[stack.size() - 2].registers = frames_[frames_.size() - 2].R;
        ctl->popFrame();
    }

    bool isTopLevel = (frames_.size() == 1);

    // Export globals back to globalsEnv_ (the single global value store) — no
    // local copy in workspaceEnv_. A function-scope `global X` writes only
    // globalsEnv_ and never touches the base workspace; mirroring it there was
    // the leak. Base-workspace membership is recorded separately by
    // runOneChunk's updateTopLevelGlobals, and reads of a base-undeclared
    // global are blocked by Engine::getVariable's membership gate.
    for (auto &gname : frame.chunk->globalNames) {
        for (auto &[vname, reg] : frame.chunk->varMap) {
            if (vname == gname && reg < frame.nregs) {
                if (isTopLevel) {
                    // Bare `global X` (unset/deleted reg) must not clobber an
                    // existing global value.
                    if (!frame.R[reg].isUnset() && !frame.R[reg].isDeleted())
                        engine_.globalsEnv_->set(gname, frame.R[reg]);
                } else {
                    engine_.globalsEnv_->set(gname, frame.R[reg]);
                }
                break;
            }
        }
    }

    if (isTopLevel)
        rebuildLocalVarMap(lastVarMap_, *frame.chunk, frame.R, frame.nregs);

    // Cleanup: release heap objects in frame
    for (uint8_t i = 0; i < frame.nregs; ++i) {
        if (!frame.R[i].isDoubleScalar() && !frame.R[i].isEmpty())
            frame.R[i] = Value();
    }

    // Restore stack pointers
    regStackTop_ = frame.regBase;
    forStack_.resize(std::min(forStack_.size(), frame.forStackBase));
    tryStack_.resize(std::min(tryStack_.size(), frame.tryStackBase));

    frames_.pop_back();

    // State-machine callback return: feed the result into the continuation,
    // which either pushes the next callback frame (re-attaching itself) or
    // writes the final result to its captured destination. No caller-register
    // write — the "caller" of a callback frame is the C++ state machine.
    if (cont) {
        R_ = frames_.empty() ? nullptr : frames_.back().R;
        cont->step(*this, &retVal, cont);
        return;
    }

    if (!frames_.empty()) {
        CallFrame &caller = frames_.back();
        R_ = caller.R;

        if (frame.isMultiReturn) {
            // MATLAB: a `[...] = f()` destructure that requests more
            // outputs than the callee produces is "Too many output
            // arguments" (bug #44 — parity with the TreeWalker and the
            // external-builtin path above).
            size_t produced = (returnCount_ > 0)
                                   ? returnCount_
                                   : (retVal.isUnset() ? 0u : 1u);
            if (produced < frame.nout) {
                // Restore caller register window before unwinding so the
                // exception propagates from a consistent VM state.
                throw std::runtime_error("Too many output arguments.");
            }
            // Multi-return: distribute returnBuf_ into caller's registers
            if (returnCount_ > 0) {
                for (size_t i = 0; i < frame.nout && i < returnCount_; ++i)
                    caller.R[frame.outBase + i] = std::move(returnBuf_[i]);
                for (uint8_t i = 0; i < returnCount_; ++i)
                    returnBuf_[i] = Value();
                returnCount_ = 0;
            } else {
                // Single return via RET — store in first output slot
                caller.R[frame.outBase] = std::move(retVal);
                for (size_t i = 1; i < frame.nout; ++i)
                    caller.R[frame.outBase + i] = Value();
            }
        } else {
            caller.R[frame.destReg] = std::move(retVal);
        }
    } else {
        R_ = nullptr;
        lastResult_ = std::move(retVal);
    }
}

bool VM::startContinuation(std::shared_ptr<VmContinuation> cont)
{
    // First step: no prior callback result yet.
    return cont->step(*this, nullptr, cont);
}

bool VM::pushCallbackFrame(const Value &handle, Span<const Value> args, size_t nargout,
                           std::shared_ptr<VmContinuation> cont)
{
    // Unwrap a closure cell {handle, captures…}: captures append to the args,
    // matching the [user_params…, captures…] chunk parameter layout (same as
    // callFunctionHandleMulti).
    const Value *bare = &handle;
    std::vector<Value> withCaptures;
    if (handle.isCell() && handle.numel() >= 1 && handle.cellAt(0).isFuncHandle()) {
        bare = &handle.cellAt(0);
        withCaptures.assign(args.begin(), args.end());
        for (size_t i = 1; i < handle.numel(); ++i)
            withCaptures.push_back(handle.cellAt(i));
        args = Span<const Value>(withCaptures.data(), withCaptures.size());
    }
    if (!bare->isFuncHandle())
        return false;
    const UserFunction *uf = engine_.lookupUserFunctionLocal(bare->funcHandleName());
    if (!uf)
        return false; // not user code — caller must use the synchronous path
    const BytecodeChunk *cc = engine_.ensureClassMethodChunk(*uf);
    if (!cc)
        return false;
    pushCallFrame(*cc, args.data(), static_cast<uint8_t>(args.size()), /*destReg=*/0, nargout,
                  /*isMulti=*/false, 0, 0, std::string(), /*isCtor=*/false, /*ctorSeed=*/nullptr);
    frames_.back().cont = std::move(cont);
    return true;
}

bool LoopContinuation::step(VM &vm, Value *prevResult,
                            const std::shared_ptr<VmContinuation> &self)
{
    if (prevResult) {
        results.push_back(std::move(*prevResult));
        ++i;
    }
    if (i >= n) {
        *dest = pack ? pack(results) : Value();
        return false; // finished — output written
    }
    std::vector<Value> a = makeArgs(i);
    return vm.pushCallbackFrame(handle, Span<const Value>(a.data(), a.size()), 1, self);
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
            varReg = Value::fromString(std::string(1, src[fs.index]), engine_.mr_);
        } else {
            auto col = Value::matrix(fs.rows, 1, ValueType::CHAR, engine_.mr_);
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
            auto col = Value::matrix(fs.rows, 1, ValueType::LOGICAL, engine_.mr_);
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
    auto col = Value::matrix(rows, 1, ValueType::DOUBLE, engine_.mr_);
    double *dst = col.doubleDataMut();
    const double *src = fs.data + fs.index * rows;
    for (size_t r = 0; r < rows; ++r)
        dst[r] = src[r];
    varReg = std::move(col);
}

// ============================================================
// Extracted dispatch helpers
// ============================================================

// Opcode → MATLAB operator token (the form Engine's operator tables key on).
// Shared by the slow paths and the object-operator frame-push helpers so the
// mapping has a single source of truth.
static const char *binaryOpString(OpCode op)
{
    switch (op) {
    case OpCode::ADD:   return "+";
    case OpCode::SUB:   return "-";
    case OpCode::MUL:   return "*";
    case OpCode::RDIV:  return "/";
    case OpCode::LDIV:  return "\\";
    case OpCode::POW:   return "^";
    case OpCode::EMUL:  return ".*";
    case OpCode::ERDIV: return "./";
    case OpCode::ELDIV: return ".\\";
    case OpCode::EPOW:  return ".^";
    case OpCode::EQ:    return "==";
    case OpCode::NE:    return "~=";
    case OpCode::LT:    return "<";
    case OpCode::GT:    return ">";
    case OpCode::LE:    return "<=";
    case OpCode::GE:    return ">=";
    case OpCode::AND:   return "&";
    case OpCode::OR:    return "|";
    default:            return nullptr;
    }
}

static const char *unaryOpString(OpCode op)
{
    switch (op) {
    case OpCode::NEG:        return "-";
    case OpCode::NOT:        return "~";
    case OpCode::CTRANSPOSE: return "'";
    case OpCode::TRANSPOSE:  return ".'";
    default:                 return nullptr;
    }
}

Value VM::binarySlowPath(OpCode op, const Value &lhs, const Value &rhs)
{
    const char *opStr = binaryOpString(op);
    // OBJECT operator overloading: dispatch to the dominant object's class
    // `ops` before the numeric builtin path (throws if no overload).
    if (opStr && (lhs.isObject() || rhs.isObject())) {
        Value out;
        if (engine_.tryObjectBinaryOp(opStr, lhs, rhs, currentCallEnv(), out))
            return out;
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
    const char *opStr = unaryOpString(op);
    // OBJECT unary operator overloading — before the numeric builtin path.
    if (opStr && operand.isObject()) {
        Value out;
        if (engine_.tryObjectUnaryOp(opStr, operand, currentCallEnv(), out))
            return out;
    }
    if (opStr) {
        auto it = engine_.unaryOps_.find(opStr);
        if (it != engine_.unaryOps_.end())
            return it->second(operand);
    }
    throw std::runtime_error("VM: unsupported unary op");
}

bool VM::tryBinaryOpFrame(uint8_t dst, OpCode op, uint8_t lhsReg, uint8_t rhsReg,
                          CallFrame &frame, const Instruction *ip)
{
    Value *R = frame.R;
    if (!R[lhsReg].isObject() && !R[rhsReg].isObject())
        return false; // numeric fast path — no object operand
    const char *opStr = binaryOpString(op);
    if (!opStr)
        return false;
    std::string ownerClass;
    const BytecodeChunk *cc =
        engine_.resolveBinaryOpChunk(opStr, R[lhsReg], R[rhsReg], ownerClass);
    if (!cc)
        return false; // no user overload / uncompilable → caller's slow path
    Value argbuf[2] = {R[lhsReg], R[rhsReg]}; // operator params ARE the operands
    frame.ip = ip + 1;
    pushCallFrame(*cc, argbuf, 2, dst, 1, false, 0, 0, ownerClass, /*isCtor=*/false);
    return true;
}

bool VM::tryUnaryOpFrame(uint8_t dst, OpCode op, uint8_t operandReg,
                         CallFrame &frame, const Instruction *ip)
{
    Value *R = frame.R;
    if (!R[operandReg].isObject())
        return false;
    const char *opStr = unaryOpString(op);
    if (!opStr)
        return false;
    std::string ownerClass;
    const BytecodeChunk *cc = engine_.resolveUnaryOpChunk(opStr, R[operandReg], ownerClass);
    if (!cc)
        return false;
    Value argbuf[1] = {R[operandReg]};
    frame.ip = ip + 1;
    pushCallFrame(*cc, argbuf, 1, dst, 1, false, 0, 0, ownerClass, /*isCtor=*/false);
    return true;
}

bool VM::tryObjectSubsrefFrame(uint8_t dst, uint8_t selfReg, Span<const Value> idx,
                               CallFrame &frame, const Instruction *ip)
{
    Value *R = frame.R;
    if (!R[selfReg].isObject())
        return false;
    return tryObjectSubsrefFrameObj(R[selfReg], dst, idx, frame, ip);
}

bool VM::tryObjectSubsrefFrameObj(const Value &self, uint8_t dst, Span<const Value> idx,
                                  CallFrame &frame, const Instruction *ip)
{
    if (!self.isObject())
        return false;
    std::string ownerClass;
    std::vector<Value> args;
    const BytecodeChunk *cc = engine_.resolveSubsrefChunk(self, idx, ownerClass, args);
    if (!cc)
        return false;
    frame.ip = ip + 1;
    pushCallFrame(*cc, args.data(), static_cast<uint8_t>(args.size()), dst, 1, false, 0, 0,
                  ownerClass, /*isCtor=*/false);
    return true;
}

bool VM::tryObjectSubsasgnFrame(uint8_t objReg, Span<const Value> idxAndVal,
                                CallFrame &frame, const Instruction *ip)
{
    Value *R = frame.R;
    if (!R[objReg].isObject())
        return false;
    std::string ownerClass;
    std::vector<Value> args;
    const BytecodeChunk *cc = engine_.resolveSubsasgnChunk(R[objReg], idxAndVal, ownerClass, args);
    if (!cc)
        return false;
    // subsasgn returns the modified object → write back into the object register
    // (value class); a handle mutates shared state and returns itself.
    frame.ip = ip + 1;
    pushCallFrame(*cc, args.data(), static_cast<uint8_t>(args.size()), objReg, 1, false, 0, 0,
                  ownerClass, /*isCtor=*/false);
    return true;
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
        // sqrt / log / log2 / log10 of a negative scalar promote to a
        // complex result in MATLAB. The scalar fast path can only hold a
        // real double, so for v < 0 we bail (handled = false) and let the
        // full builtin (Value wrapper) produce the complex value.
        case 5:  if (v < 0.0) { handled = false; break; } result = std::sqrt(v);  break;
        case 6:  result = std::exp(v); break;
        case 7:  if (v < 0.0) { handled = false; break; } result = std::log(v);   break;
        case 8:  if (v < 0.0) { handled = false; break; } result = std::log2(v);  break;
        case 9:  if (v < 0.0) { handled = false; break; } result = std::log10(v); break;
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
            // MATLAB: mod(a, 0) == a (std::fmod(a, 0) would be NaN).
            if (b == 0.0) { result = a; break; }
            result = std::fmod(a, b);
            if (result != 0.0 && ((result > 0) != (b > 0)))
                result += b;
            break;
        case 21: result = std::fmod(a, b); break;
        // max/min ignore NaN (MATLAB): fmax/fmin return the non-NaN
        // operand, NaN only if both are NaN. The old (a>=b)?a:b returned
        // NaN for max(5,NaN) and disagreed with the TreeWalker (fmax).
        case 22: result = std::fmax(a, b); break;
        case 23: result = std::fmin(a, b); break;
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
    const char *fname = builtinIdName(bid);
    if (fname) {
        const ExternalFunc *fnPtr = engine_.findExternal(
            fname, currentCallEnv());
        if (fnPtr) {
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
            CallContext ctx{&engine_, currentCallEnv()};
            (*fnPtr)(as, 1, os, ctx);
            R[I.a] = std::move(ob[0]);
            return;
        }
    }
    throw std::runtime_error("VM: unsupported builtin");
}

bool VM::execCallIndirect(const Instruction &I, Value *R,
                           CallFrame &frame, const Instruction *ip)
{
    return execCallIndirectTarget(R[I.b], I.a, I.c, I.e, R, frame, ip);
}

bool VM::execCallIndirectTarget(const Value &target, uint8_t dstReg, uint8_t argBase, uint8_t na,
                                Value *R, CallFrame &frame, const Instruction *ip)
{
    // Resolve function handle (plain or closure cell)
    Value funcHandleVal;
    size_t numCaptures = 0;

    if (target.isCell() && target.numel() >= 1
        && target.cellAt(0).isFuncHandle()) {
        funcHandleVal = target.cellAt(0);
        numCaptures = target.numel() - 1;
    } else if (target.isFuncHandle()) {
        funcHandleVal = target;
    } else if (target.isObject()) {
        // OBJECT: obj(i…) dispatches to the class subsref overload.
        // (A known variable `obj(i)` compiles to CALL_INDIRECT, so the
        // object subsref hook is needed here too, not just in INDEX_GET.)
        {
            std::vector<Value> idx(na);
            for (uint8_t i = 0; i < na; ++i)
                idx[i] = R[argBase + i];
            // classdef subsref → same-stack VM frame (pausable, P4e). Returns
            // true on push; the caller then `goto enter_frame`. Without this
            // the body runs through the engine call below — outside any VM
            // frame, so breakpoints inside subsref never fire.
            if (tryObjectSubsrefFrameObj(target, dstReg, Span<const Value>(idx.data(), na), frame, ip))
                return true;
            Value out;
            if (engine_.tryObjectSubsref(const_cast<Value&>(target), Span<const Value>(idx.data(), na), 1,
                                         out, currentCallEnv())) {
                R[dstReg] = std::move(out);
                return false;
            }
        }
        // No custom subsref → builtin object-array indexing (1-D / 2-D /
        // N-D), routed through the OBJECT-aware Value index methods.
        const Value &mv = target;
        if (na == 1) {
            auto idxs = Value::resolveIndices(R[argBase], mv.objectCount());
            R[dstReg] = mv.objectSubArray(idxs, engine_.mr_);
        } else if (na == 2) {
            auto rids = Value::resolveIndices(R[argBase], mv.dims().rows());
            auto cids = Value::resolveIndices(R[argBase + 1], mv.dims().cols());
            R[dstReg] = mv.indexGet2D(rids.data(), rids.size(), cids.data(), cids.size(),
                                   engine_.mr_);
        } else {
            const int nd = static_cast<int>(na);
            std::vector<std::vector<size_t>> lists(nd);
            std::vector<const size_t *> ptrs(nd);
            std::vector<size_t> counts(nd);
            for (int i = 0; i < nd; ++i) {
                const size_t lim = (i < mv.dims().ndim()) ? mv.dims().dim(i) : 1;
                lists[i] = Value::resolveIndices(R[argBase + i], lim);
                ptrs[i] = lists[i].data();
                counts[i] = lists[i].size();
            }
            R[dstReg] = mv.indexGetND(ptrs.data(), counts.data(), nd, engine_.mr_);
        }
        return false;
    } else {
        // Array indexing fallback
        execIndirectIndexTarget(target, dstReg, argBase, na, R);
        return false;
    }

    const std::string &funcName = funcHandleVal.funcHandleName();

    // Build args: user args + captured values
    size_t totalArgsN = static_cast<size_t>(na) + numCaptures;
    std::vector<Value> argsBuf(totalArgsN);
    for (uint8_t i = 0; i < na; ++i)
        argsBuf[i] = R[argBase + i];
    for (size_t i = 0; i < numCaptures; ++i)
        argsBuf[na + i] = target.cellAt(1 + i);
    uint8_t totalArgs = static_cast<uint8_t>(std::min(totalArgsN, size_t(255)));

    // Try compiled user function
    if (const BytecodeChunk *found = findCompiledFunc(funcName)) {
        frame.ip = ip + 1;
        pushCallFrame(*found, argsBuf.data(), totalArgs, dstReg, 1);
        return true; // caller must re-enter dispatch loop
    }

    // External function
    auto extIt = engine_.externalFuncs_.find(funcName);
    if (extIt != engine_.externalFuncs_.end()) {
        Span<const Value> as(argsBuf.data(), na);
        Value ob[1];
        Span<Value> os(ob, 1);
        CallContext ctx{&engine_, currentCallEnv()};
        extIt->second(as, 1, os, ctx);
        R[dstReg] = std::move(ob[0]);
        return false;
    }
    throw std::runtime_error("VM: undefined function in handle '@" + funcName + "'");
}

bool VM::execCallIndirectMulti(const Instruction &I, Value *R,
                               CallFrame &frame, const Instruction *ip)
{
    // a=outBase, b=fhReg, c=argBase, d=nargs, e=nout
    uint8_t outBase = I.a, fhReg = I.b, argBase = I.c;
    uint8_t na = static_cast<uint8_t>(I.d);
    uint8_t nout = I.e;

    // Resolve the function handle (plain or closure cell) + its captures.
    Value funcHandleVal;
    size_t numCaptures = 0;
    if (R[fhReg].isCell() && R[fhReg].numel() >= 1
        && R[fhReg].cellAt(0).isFuncHandle()) {
        funcHandleVal = R[fhReg].cellAt(0);
        numCaptures = R[fhReg].numel() - 1;
    } else if (R[fhReg].isFuncHandle()) {
        funcHandleVal = R[fhReg];
    } else {
        throw std::runtime_error(
            "VM: multi-output call '[...] = f(...)' of a value that is not a "
            "function handle");
    }

    const std::string &funcName = funcHandleVal.funcHandleName();

    // Build args: user args followed by captured values.
    size_t totalArgsN = static_cast<size_t>(na) + numCaptures;
    std::vector<Value> argsBuf(totalArgsN);
    for (uint8_t i = 0; i < na; ++i)
        argsBuf[i] = R[argBase + i];
    for (size_t i = 0; i < numCaptures; ++i)
        argsBuf[na + i] = R[fhReg].cellAt(1 + i);
    uint8_t totalArgs = static_cast<uint8_t>(std::min(totalArgsN, size_t(255)));

    // Compiled user / anonymous function → multi-return frame push (the body
    // runs with nargout = nout and RET_MULTI distributes back to outBase).
    if (const BytecodeChunk *found = findCompiledFunc(funcName)) {
        frame.ip = ip + 1;
        returnCount_ = 0;
        pushCallFrame(*found, argsBuf.data(), totalArgs,
                      0, nout, true, outBase, nout);
        return true; // caller re-enters the dispatch loop
    }

    // External (builtin) function called for several outputs.
    const ExternalFunc *fnPtr = engine_.findExternal(funcName, currentCallEnv());
    if (fnPtr) {
        std::vector<Value> outBuf(nout);
        Span<const Value> as(argsBuf.data(), totalArgs);
        Span<Value> os(outBuf.data(), nout);
        CallContext ctx{&engine_, currentCallEnv()};
        (*fnPtr)(as, nout, os, ctx);
        for (uint8_t i = 0; i < nout; ++i)
            if (outBuf[i].isUnset())
                throw std::runtime_error("Too many output arguments.");
        for (size_t i = 0; i < nout; ++i)
            R[outBase + i] = std::move(outBuf[i]);
        return false;
    }
    throw std::runtime_error("VM: undefined function in handle '@" + funcName + "'");
}

void VM::execIndirectIndex(const Instruction &I, Value *R)
{
    execIndirectIndexTarget(R[I.b], I.a, I.c, I.e, R);
}

void VM::execIndirectIndexTarget(const Value &mv, uint8_t dstReg, uint8_t argBase, uint8_t na, Value *R)
{
    if (na == 1) {
        const Value &ix = R[argBase];
        if (mv.isCell()) {
            auto indices = Value::resolveIndices(ix, mv.numel());
            R[dstReg] = mv.indexGet(indices.data(), indices.size(), engine_.mr_);
        } else if (ix.isChar() && ix.numel() == 1 && ix.charData()[0] == ':') {
            // BUG #14: scalar(:) on a tag-stored value (e.g. `true(:)`,
            // `false(:)`) used to segfault here — the previous fast path
            // memcpy'd `n*es` bytes from `mv.rawData()`, but tag scalars
            // have no heap buffer, so rawData() returned nullptr/garbage.
            // Identity short-circuit covers every scalar type (DOUBLE /
            // LOGICAL tag / INT* / STRUCT / etc.) without touching the
            // raw byte representation.
            if (mv.isScalar()) {
                R[dstReg] = mv;
            } else {
                size_t n = mv.numel();
                ValueType t = mv.type();
                auto res = Value::matrix(n, 1, t, engine_.mr_);
                if (n > 0) {
                    size_t es = elementSize(t);
                    std::memcpy(res.rawDataMut(), mv.rawData(), n * es);
                }
                R[dstReg] = numkit::narrowComplex(std::move(res), engine_.mr_);  // z(:) all-real -> real
            }
        } else if (mv.isScalar() && ix.isDoubleScalar()) {
            checkedIndex(ix.scalarVal(), 1);
            R[dstReg] = mv;
        } else if (ix.isDoubleScalar()) {
            size_t i = checkedIndex(ix.scalarVal(), mv.numel());
            R[dstReg] = mv.elemAt(i, engine_.mr_);
        } else if (ix.isLogical()) {
            R[dstReg] = mv.logicalIndex(ix.logicalData(), ix.numel(), engine_.mr_);
        } else {
            // General index vector — resolveIndices handles all numeric/char
            // index types (int*, single, char codes) and validates
            // positivity/integrality/bounds, matching the INDEX_GET opcode.
            // (Raw doubleData() threw "Not a double array" on e.g. A(int32(2))
            // and silently truncated fractional indices.)
            auto indices = Value::resolveIndices(ix, mv.numel());
            R[dstReg] = mv.indexGet(indices.data(), indices.size(), engine_.mr_);
        }
    } else if (na == 2) {
        const Value &ri = R[argBase];
        const Value &ci = R[argBase + 1];
        auto rowIds = Value::resolveIndices(ri, mv.dims().rows());
        auto colIds = Value::resolveIndices(ci, mv.dims().cols());
        R[dstReg] = mv.indexGet2D(rowIds.data(), rowIds.size(),
                               colIds.data(), colIds.size(),
                               engine_.mr_);
    } else if (na == 3) {
        if (mv.isCell()) {
            size_t r = (size_t) R[argBase].toScalar() - 1;
            size_t c = (size_t) R[argBase + 1].toScalar() - 1;
            size_t p = (size_t) R[argBase + 2].toScalar() - 1;
            R[dstReg] = mv.cellAt(mv.dims().sub2indChecked(r, c, p));
        } else {
            auto rowIds = Value::resolveIndices(R[argBase], mv.dims().rows());
            auto colIds = Value::resolveIndices(R[argBase + 1], mv.dims().cols());
            auto pageIds = Value::resolveIndices(R[argBase + 2], mv.dims().pages());
            R[dstReg] = mv.indexGet3D(rowIds.data(), rowIds.size(),
                                   colIds.data(), colIds.size(),
                                   pageIds.data(), pageIds.size(),
                                   engine_.mr_);
        }
    } else {
        // ND indexing fallback for na >= 4. CELL handled by indexGetND.
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
        R[dstReg] = mv.indexGetND(idxPtrs.data(), idxCounts.data(), nd, engine_.mr_);
    }
}

void VM::execDisplay(const Instruction &I, Value *R, const BytecodeChunk &chunk)
{
    if (R[I.a].isUnset())
        return;
    const std::string &name = chunk.strings[I.d];
    if (R[I.a].isObject()) {
        engine_.displayObject(name, R[I.a]); // honours class disp/display override
        return;
    }
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
                    std::string bytesStr = std::to_string(val.deepBytes());
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