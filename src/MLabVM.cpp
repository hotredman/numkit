// src/MLabVM.cpp
#include "MLabVM.hpp"
#include "MLabEngine.hpp"
#include "MLabSpan.hpp"

#include <cmath>
#include <sstream>
#include <stdexcept>

namespace mlab {

VM::VM(Engine &engine)
    : engine_(engine)
{
    regStack_.resize(kRegStackSize);
}

static inline size_t colonCount(double start, double step, double stop)
{
    if (step == 0.0)
        return 0;
    double n = (stop - start) / step;
    if (n < 0)
        return 0;
    return static_cast<size_t>(std::floor(n + 1e-10)) + 1;
}

// ============================================================
// Public API: MValue boundary
// ============================================================

MValue VM::execute(const BytecodeChunk &chunk, const MValue *args, uint8_t nargs)
{
    // Push frame onto register stack
    regStackTop_ = 0;
    R_ = regStack_.data();

    // Clear registers for this frame
    for (uint8_t i = 0; i < chunk.numRegisters; ++i)
        R_[i] = VMValue();

    regStackTop_ = chunk.numRegisters;

    // Load arguments
    if (args) {
        uint8_t pc = std::min(nargs, chunk.numParams);
        for (uint8_t i = 0; i < pc; ++i)
            R_[i] = VMValue::fromMValue(args[i]);
    }

    VMValue result = executeInternal(chunk);
    regStackTop_ = 0;
    R_ = nullptr;
    return result.toMValue(&engine_.allocator_);
}

// ============================================================
// Internal execute — VMValue throughout, no MValue in hot path
// ============================================================

VMValue VM::executeInternal(const BytecodeChunk &chunk)
{
    const Instruction *ip = chunk.code.data();
    const Instruction *end = ip + chunk.code.size();
    auto *R = R_; // local copy of frame pointer for speed

    while (ip < end) {
        const Instruction &I = *ip;

        switch (I.op) {
        // ── Data movement ────────────────────────────────────
        case OpCode::LOAD_CONST: {
            const MValue &cv = chunk.constants[I.d];
            if (cv.isScalar() && cv.type() == MType::DOUBLE)
                R[I.a].setScalarFast(cv.toScalar());
            else
                R[I.a] = VMValue::fromMValue(cv);
            break;
        }

        case OpCode::LOAD_EMPTY:
            R[I.a] = VMValue();
            break;

        case OpCode::LOAD_STRING:
            R[I.a].setMValue(MValue::fromString(chunk.strings[I.d], &engine_.allocator_));
            break;

        case OpCode::MOVE:
            R[I.a] = R[I.b];
            break;

        case OpCode::COLON_ALL:
            R[I.a].setMValue(MValue::fromString(":", &engine_.allocator_));
            break;

        // ── Scalar arithmetic ────────────────────────────────
        case OpCode::ADD:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalarFast(R[I.b].scalar() + R[I.c].scalar());
            } else
                goto binary_slow;
            break;
        case OpCode::SUB:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalarFast(R[I.b].scalar() - R[I.c].scalar());
            } else
                goto binary_slow;
            break;
        case OpCode::MUL:
        case OpCode::EMUL:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalarFast(R[I.b].scalar() * R[I.c].scalar());
            } else
                goto binary_slow;
            break;
        case OpCode::RDIV:
        case OpCode::ERDIV:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalarFast(R[I.b].scalar() / R[I.c].scalar());
            } else
                goto binary_slow;
            break;
        case OpCode::LDIV:
        case OpCode::ELDIV:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalarFast(R[I.c].scalar() / R[I.b].scalar());
            } else
                goto binary_slow;
            break;
        case OpCode::POW:
        case OpCode::EPOW:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalarFast(std::pow(R[I.b].scalar(), R[I.c].scalar()));
            } else
                goto binary_slow;
            break;

        // ── Comparison ───────────────────────────────────────
        case OpCode::EQ:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalarFast(R[I.b].scalar() == R[I.c].scalar() ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;
        case OpCode::NE:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalarFast(R[I.b].scalar() != R[I.c].scalar() ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;
        case OpCode::LT:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalarFast(R[I.b].scalar() < R[I.c].scalar() ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;
        case OpCode::GT:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalarFast(R[I.b].scalar() > R[I.c].scalar() ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;
        case OpCode::LE:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalarFast(R[I.b].scalar() <= R[I.c].scalar() ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;
        case OpCode::GE:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalarFast(R[I.b].scalar() >= R[I.c].scalar() ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;
        case OpCode::AND:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalarFast((R[I.b].scalar() != 0.0 && R[I.c].scalar() != 0.0) ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;
        case OpCode::OR:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalarFast((R[I.b].scalar() != 0.0 || R[I.c].scalar() != 0.0) ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;

        // ── Unary ────────────────────────────────────────────
        case OpCode::NEG:
            if (R[I.b].isScalar()) {
                R[I.a].setScalarFast(-R[I.b].scalar());
            } else
                goto unary_slow;
            break;
        case OpCode::UPLUS:
            R[I.a] = R[I.b];
            break;
        case OpCode::NOT:
            if (R[I.b].isScalar()) {
                R[I.a].setScalarFast(R[I.b].scalar() == 0.0 ? 1.0 : 0.0);
            } else
                goto unary_slow;
            break;
        case OpCode::CTRANSPOSE:
        case OpCode::TRANSPOSE:
            if (R[I.b].isScalar()) {
                R[I.a] = R[I.b];
            } else
                goto unary_slow;
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
            if (R[I.b].isScalar()) {
                fs.range = MValue::scalar(R[I.b].scalar(), &engine_.allocator_);
                fs.count = 1;
            } else if (R[I.b].isEmpty()) {
                fs.count = 0;
            } else {
                fs.range = R[I.b].mvalue();
                fs.count = fs.range.dims().cols();
            }
            fs.index = 0;
            if (fs.count == 0) {
                ip += I.d;
                continue;
            }
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
            double start = R[I.b].toDouble(), stop = R[I.c].toDouble();
            size_t cnt = colonCount(start, 1.0, stop);
            auto mat = MValue::matrix(1, cnt, MType::DOUBLE, &engine_.allocator_);
            if (cnt > 0) {
                double *d = mat.doubleDataMut();
                for (size_t i = 0; i < cnt; ++i)
                    d[i] = start + (double) i;
            }
            R[I.a].setMValue(std::move(mat));
            break;
        }
        case OpCode::COLON3: {
            double start = R[I.b].toDouble(), step = R[I.c].toDouble(), stop = R[I.e].toDouble();
            size_t cnt = colonCount(start, step, stop);
            auto mat = MValue::matrix(1, cnt, MType::DOUBLE, &engine_.allocator_);
            if (cnt > 0) {
                double *d = mat.doubleDataMut();
                for (size_t i = 0; i < cnt; ++i)
                    d[i] = start + (double) i * step;
                if (cnt >= 2) {
                    double last = start + (double) (cnt - 1) * step;
                    if ((step > 0 && last > stop) || (step < 0 && last < stop))
                        d[cnt - 1] = stop;
                }
            }
            R[I.a].setMValue(std::move(mat));
            break;
        }

        // ── Array construction ───────────────────────────────
        case OpCode::HORZCAT:
            executeHorzcat(R[I.a], &R[I.b], I.c);
            break;
        case OpCode::VERTCAT:
            executeVertcat(R[I.a], &R[I.b], I.c);
            break;

        // ── Array indexing ───────────────────────────────────
        case OpCode::INDEX_GET: {
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalarFast(R[I.b].scalar());
            } else if (R[I.c].isScalar() && R[I.b].isMValue()) {
                size_t i = (size_t) R[I.c].scalar() - 1;
                R[I.a].setScalarFast(R[I.b].mvalue().doubleData()[i]);
            } else if (R[I.c].isMValue() && R[I.b].isMValue()) {
                const MValue &mv = R[I.b].mvalue();
                const MValue &ix = R[I.c].mvalue();
                size_t n = ix.numel();
                auto res = MValue::matrix(1, n, MType::DOUBLE, &engine_.allocator_);
                double *dst = res.doubleDataMut();
                const double *src = mv.doubleData(), *id = ix.doubleData();
                for (size_t k = 0; k < n; ++k)
                    dst[k] = src[(size_t) id[k] - 1];
                R[I.a].setMValue(std::move(res));
            } else {
                throw std::runtime_error("VM: unsupported INDEX_GET operands");
            }
            break;
        }
        case OpCode::INDEX_GET_2D: {
            if (R[I.b].isMValue() && R[I.c].isScalar() && R[I.e].isScalar()) {
                size_t r = (size_t) R[I.c].scalar() - 1, c = (size_t) R[I.e].scalar() - 1;
                const MValue &mv = R[I.b].mvalue();
                R[I.a].setScalarFast(mv.doubleData()[mv.dims().sub2ind(r, c)]);
            } else {
                throw std::runtime_error("VM: non-scalar 2D indexing not supported");
            }
            break;
        }
        case OpCode::INDEX_SET: {
            double val = R[I.c].toDouble();
            size_t i = (size_t) R[I.b].toDouble() - 1;
            if (R[I.a].isMValue()) {
                MValue &mv = R[I.a].mvalue();
                if (i >= mv.numel()) {
                    size_t ns = i + 1;
                    auto g = MValue::matrix(1, ns, MType::DOUBLE, &engine_.allocator_);
                    double *dst = g.doubleDataMut();
                    std::fill(dst, dst + ns, 0.0);
                    if (!mv.isEmpty()) {
                        const double *s = mv.doubleData();
                        std::copy(s, s + mv.numel(), dst);
                    }
                    mv = std::move(g);
                }
                mv.doubleDataMut()[i] = val;
            } else {
                size_t ns = i + 1;
                auto mat = MValue::matrix(1, ns, MType::DOUBLE, &engine_.allocator_);
                double *dst = mat.doubleDataMut();
                std::fill(dst, dst + ns, 0.0);
                if (R[I.a].isScalar() && ns >= 1)
                    dst[0] = R[I.a].scalar();
                dst[i] = val;
                R[I.a].setMValue(std::move(mat));
            }
            break;
        }
        case OpCode::INDEX_SET_2D: {
            if (R[I.a].isMValue()) {
                size_t r = (size_t) R[I.b].toDouble() - 1, c = (size_t) R[I.c].toDouble() - 1;
                MValue &mv = R[I.a].mvalue();
                mv.doubleDataMut()[mv.dims().sub2ind(r, c)] = R[I.e].toDouble();
            }
            break;
        }

        // ── Inline scalar builtins (CALL_BUILTIN) ────────────
        case OpCode::CALL_BUILTIN: {
            uint8_t argBase = I.b, na = I.c;
            int16_t bid = I.d;

            if (na == 1 && R[argBase].isScalar()) {
                double v = R[argBase].scalar(), result;
                switch (bid) {
                case 0:
                    result = std::fabs(v);
                    break;
                case 1:
                    result = std::floor(v);
                    break;
                case 2:
                    result = std::ceil(v);
                    break;
                case 3:
                    result = std::round(v);
                    break;
                case 4:
                    result = std::trunc(v);
                    break;
                case 5:
                    result = std::sqrt(v);
                    break;
                case 6:
                    result = std::exp(v);
                    break;
                case 7:
                    result = std::log(v);
                    break;
                case 8:
                    result = std::log2(v);
                    break;
                case 9:
                    result = std::log10(v);
                    break;
                case 10:
                    result = std::sin(v);
                    break;
                case 11:
                    result = std::cos(v);
                    break;
                case 12:
                    result = std::tan(v);
                    break;
                case 13:
                    result = (v > 0) ? 1.0 : (v < 0) ? -1.0 : 0.0;
                    break;
                case 14:
                    result = std::isnan(v) ? 1.0 : 0.0;
                    break;
                case 15:
                    result = std::isinf(v) ? 1.0 : 0.0;
                    break;
                default:
                    goto builtin_fallback;
                }
                R[I.a].setScalarFast(result);
                break;
            }
            if (na == 2 && R[argBase].isScalar() && R[argBase + 1].isScalar()) {
                double a = R[argBase].scalar(), b = R[argBase + 1].scalar(), result;
                switch (bid) {
                case 20:
                    result = std::fmod(a, b);
                    if (result != 0.0 && ((result > 0) != (b > 0)))
                        result += b;
                    break;
                case 21:
                    result = std::fmod(a, b);
                    break;
                case 22:
                    result = (a >= b) ? a : b;
                    break;
                case 23:
                    result = (a <= b) ? a : b;
                    break;
                case 24:
                    result = std::pow(a, b);
                    break;
                case 25:
                    result = std::atan2(a, b);
                    break;
                default:
                    goto builtin_fallback;
                }
                R[I.a].setScalarFast(result);
                break;
            }

        builtin_fallback: {
            static const char *bn[] = {"abs",   "floor", "ceil",  "round", "fix",   "sqrt",  "exp",
                                       "log",   "log2",  "log10", "sin",   "cos",   "tan",   "sign",
                                       "isnan", "isinf", nullptr, nullptr, nullptr, nullptr, "mod",
                                       "rem",   "max",   "min",   "pow",   "atan2"};
            const char *fname = (bid >= 0 && bid < 26) ? bn[bid] : nullptr;
            if (fname) {
                auto extIt = engine_.externalFuncs_.find(fname);
                if (extIt != engine_.externalFuncs_.end()) {
                    std::vector<MValue> margs(na);
                    for (uint8_t i = 0; i < na; ++i)
                        margs[i] = R[argBase + i].toMValue(&engine_.allocator_);
                    Span<const MValue> as(margs.data(), na);
                    MValue ob[1];
                    Span<MValue> os(ob, 1);
                    extIt->second(as, 1, os);
                    R[I.a] = VMValue::fromMValue(std::move(ob[0]));
                    break;
                }
            }
            throw std::runtime_error("VM: unsupported builtin");
        }
        }

        // ── General function calls ───────────────────────────
        case OpCode::CALL: {
            const std::string &funcName = chunk.strings[I.d];
            uint8_t argBase = I.b, na = I.c;

            // 1. Compiled user function — VMValue native, no MValue conversion
            if (compiledFuncs_) {
                auto cfIt = compiledFuncs_->find(funcName);
                if (cfIt != compiledFuncs_->end()) {
                    R[I.a] = callUserFunc(cfIt->second, &R[argBase], na);
                    break;
                }
            }

            // 2. External/builtin — needs MValue conversion
            auto extIt = engine_.externalFuncs_.find(funcName);
            if (extIt != engine_.externalFuncs_.end()) {
                std::vector<MValue> margs(na);
                for (uint8_t i = 0; i < na; ++i)
                    margs[i] = R[argBase + i].toMValue(&engine_.allocator_);
                Span<const MValue> as(margs.data(), na);
                MValue ob[1];
                Span<MValue> os(ob, 1);
                extIt->second(as, 1, os);
                R[I.a] = VMValue::fromMValue(std::move(ob[0]));
                break;
            }

            throw std::runtime_error("VM: undefined function '" + funcName + "'");
        }

        // ── Display ──────────────────────────────────────────
        case OpCode::DISPLAY: {
            const std::string &name = chunk.strings[I.d];
            std::ostringstream os;
            if (R[I.a].isScalar())
                os << name << " = " << R[I.a].scalar() << "\n\n";
            else if (R[I.a].isEmpty())
                os << name << " = []\n\n";
            else {
                MValue mv = R[I.a].toMValue(&engine_.allocator_);
                if (mv.isChar())
                    os << name << " = '" << mv.toString() << "'\n\n";
                else
                    os << name << " = " << mv.debugString() << "\n\n";
            }
            engine_.outputText(os.str());
            break;
        }

        // ── Return ───────────────────────────────────────────
        case OpCode::RET:
            return R[I.a]; // return VMValue directly
        case OpCode::RET_EMPTY:
            return VMValue();
        case OpCode::HALT:
            return VMValue();
        case OpCode::NOP:
            break;

        default:
            throw std::runtime_error("VM: unimplemented opcode " + std::to_string((int) I.op));

        // ── Slow paths ───────────────────────────────────────
        binary_slow: {
            MValue a = R[I.b].toMValue(&engine_.allocator_);
            MValue b = R[I.c].toMValue(&engine_.allocator_);
            const char *opStr = nullptr;
            switch (I.op) {
            case OpCode::ADD:
                opStr = "+";
                break;
            case OpCode::SUB:
                opStr = "-";
                break;
            case OpCode::MUL:
                opStr = "*";
                break;
            case OpCode::RDIV:
                opStr = "/";
                break;
            case OpCode::LDIV:
                opStr = "\\";
                break;
            case OpCode::POW:
                opStr = "^";
                break;
            case OpCode::EMUL:
                opStr = ".*";
                break;
            case OpCode::ERDIV:
                opStr = "./";
                break;
            case OpCode::ELDIV:
                opStr = ".\\";
                break;
            case OpCode::EPOW:
                opStr = ".^";
                break;
            case OpCode::EQ:
                opStr = "==";
                break;
            case OpCode::NE:
                opStr = "~=";
                break;
            case OpCode::LT:
                opStr = "<";
                break;
            case OpCode::GT:
                opStr = ">";
                break;
            case OpCode::LE:
                opStr = "<=";
                break;
            case OpCode::GE:
                opStr = ">=";
                break;
            case OpCode::AND:
                opStr = "&";
                break;
            case OpCode::OR:
                opStr = "|";
                break;
            default:
                break;
            }
            if (opStr) {
                auto it = engine_.binaryOps_.find(opStr);
                if (it != engine_.binaryOps_.end()) {
                    R[I.a] = VMValue::fromMValue(it->second(a, b));
                    break;
                }
            }
            throw std::runtime_error("VM: unsupported binary op");
        }
        unary_slow: {
            MValue src = R[I.b].toMValue(&engine_.allocator_);
            const char *opStr = nullptr;
            switch (I.op) {
            case OpCode::NEG:
                opStr = "-";
                break;
            case OpCode::NOT:
                opStr = "~";
                break;
            case OpCode::CTRANSPOSE:
                opStr = "'";
                break;
            case OpCode::TRANSPOSE:
                opStr = ".'";
                break;
            default:
                break;
            }
            if (opStr) {
                auto it = engine_.unaryOps_.find(opStr);
                if (it != engine_.unaryOps_.end()) {
                    R[I.a] = VMValue::fromMValue(it->second(src));
                    break;
                }
            }
            throw std::runtime_error("VM: unsupported unary op");
        }

        } // switch
        ++ip;
    }

    return VMValue();
}

// ============================================================
// User function call — VMValue native
// ============================================================

VMValue VM::callUserFunc(const BytecodeChunk &funcChunk, const VMValue *args, uint8_t nargs)
{
    if (++recursionDepth_ > kMaxRecursion) {
        --recursionDepth_;
        throw std::runtime_error("VM: maximum recursion depth exceeded");
    }

    // Copy args before changing R_ (args points into current frame)
    uint8_t pc = std::min(nargs, funcChunk.numParams);
    VMValue argsCopy[16];
    for (uint8_t i = 0; i < pc; ++i)
        argsCopy[i] = args[i];

    // Push new frame onto register stack
    size_t savedTop = regStackTop_;
    VMValue *savedR = R_;
    size_t savedForSize = forStack_.size();

    uint8_t nregs = funcChunk.numRegisters;
    if (regStackTop_ + nregs > kRegStackSize) {
        --recursionDepth_;
        throw std::runtime_error("VM: register stack overflow");
    }

    R_ = regStack_.data() + regStackTop_;
    regStackTop_ += nregs;

    // Load arguments into first registers
    // Other registers: left as-is from regStack (pre-init empty or stale scalar — safe).
    // LOAD_EMPTY in bytecode handles return var init explicitly.
    for (uint8_t i = 0; i < pc; ++i)
        R_[i] = std::move(argsCopy[i]);

    // Execute
    VMValue result = executeInternal(funcChunk);

    // Cleanup: only release MValue pointers (scalar/empty are free)
    for (uint8_t i = 0; i < nregs; ++i) {
        if (R_[i].isMValue())
            R_[i] = VMValue();
    }

    // Pop frame
    regStackTop_ = savedTop;
    R_ = savedR;
    forStack_.resize(savedForSize);
    --recursionDepth_;
    return result;
}

// ============================================================
// Array helpers
// ============================================================

void VM::executeHorzcat(VMValue &dst, const VMValue *regs, uint8_t count)
{
    size_t total = 0;
    for (uint8_t i = 0; i < count; ++i) {
        if (regs[i].isEmpty())
            continue;
        if (regs[i].isScalar()) {
            total++;
            continue;
        }
        total += regs[i].mvalue().numel();
    }
    auto result = MValue::matrix(1, total, MType::DOUBLE, &engine_.allocator_);
    double *d = result.doubleDataMut();
    size_t pos = 0;
    for (uint8_t i = 0; i < count; ++i) {
        if (regs[i].isEmpty())
            continue;
        if (regs[i].isScalar()) {
            d[pos++] = regs[i].scalar();
            continue;
        }
        const double *src = regs[i].mvalue().doubleData();
        size_t n = regs[i].mvalue().numel();
        std::copy(src, src + n, d + pos);
        pos += n;
    }
    dst.setMValue(std::move(result));
}

void VM::executeVertcat(VMValue &dst, const VMValue *regs, uint8_t count)
{
    size_t totalRows = 0, cols = 0;
    for (uint8_t i = 0; i < count; ++i) {
        if (regs[i].isEmpty())
            continue;
        if (regs[i].isScalar()) {
            totalRows++;
            if (!cols)
                cols = 1;
            continue;
        }
        auto dims = regs[i].mvalue().dims();
        totalRows += dims.rows();
        if (!cols)
            cols = dims.cols();
    }
    if (!cols) {
        dst = VMValue();
        return;
    }
    auto result = MValue::matrix(totalRows, cols, MType::DOUBLE, &engine_.allocator_);
    double *d = result.doubleDataMut();
    size_t rowOff = 0;
    for (uint8_t i = 0; i < count; ++i) {
        if (regs[i].isEmpty())
            continue;
        if (regs[i].isScalar()) {
            d[rowOff++] = regs[i].scalar();
            continue;
        }
        auto dims = regs[i].mvalue().dims();
        const double *src = regs[i].mvalue().doubleData();
        for (size_t c = 0; c < cols; ++c)
            for (size_t r = 0; r < dims.rows(); ++r)
                d[(rowOff + r) + c * totalRows] = src[r + c * dims.rows()];
        rowOff += dims.rows();
    }
    dst.setMValue(std::move(result));
}

void VM::forSetVar(VMValue &varReg, const ForState &fs)
{
    const MValue &range = fs.range;
    if (range.isScalar()) {
        varReg.setScalar(range.toScalar());
        return;
    }
    if (range.type() == MType::DOUBLE) {
        auto dims = range.dims();
        if (dims.rows() == 1) {
            varReg.setScalar(range.doubleData()[fs.index]);
            return;
        }
        size_t rows = dims.rows();
        auto col = MValue::matrix(rows, 1, MType::DOUBLE, &engine_.allocator_);
        double *dst = col.doubleDataMut();
        const double *src = range.doubleData();
        for (size_t r = 0; r < rows; ++r)
            dst[r] = src[r + fs.index * rows];
        varReg.setMValue(std::move(col));
        return;
    }
    throw std::runtime_error("VM: unsupported type in for-loop range");
}

} // namespace mlab