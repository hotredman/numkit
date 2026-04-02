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
// Public API
// ============================================================

MValue VM::execute(const BytecodeChunk &chunk, const MValue *args, uint8_t nargs)
{
    chunkCallCache_.clear();
    regStackTop_ = 0;
    R_ = regStack_.data();

    for (uint8_t i = 0; i < chunk.numRegisters; ++i)
        R_[i] = MValue::empty();

    regStackTop_ = chunk.numRegisters;

    if (args) {
        uint8_t pc = std::min(nargs, chunk.numParams);
        for (uint8_t i = 0; i < pc; ++i)
            R_[i] = args[i];
    }

    MValue result = executeInternal(chunk);
    regStackTop_ = 0;
    R_ = nullptr;
    return result;
}

// ============================================================
// Internal dispatch — MValue directly, scalar fast paths
// ============================================================

MValue VM::executeInternal(const BytecodeChunk &chunk)
{
    const Instruction *ip = chunk.code.data();
    const Instruction *end = ip + chunk.code.size();
    auto *R = R_;
    auto &resolvedFuncs = chunkCallCache_[&chunk];

    while (ip < end) {
        const Instruction &I = *ip;

        switch (I.op) {
        // ── Data movement ────────────────────────────────────
        case OpCode::LOAD_CONST:
            R[I.a] = chunk.constants[I.d];
            break;
        case OpCode::LOAD_EMPTY:
            R[I.a] = MValue::empty();
            break;
        case OpCode::LOAD_STRING:
            R[I.a] = MValue::fromString(chunk.strings[I.d], &engine_.allocator_);
            break;
        case OpCode::MOVE:
            R[I.a] = R[I.b];
            break;
        case OpCode::COLON_ALL:
            R[I.a] = MValue::fromString(":", &engine_.allocator_);
            break;

        // ── Scalar arithmetic ────────────────────────────────
        case OpCode::ADD:
            if (R[I.b].isDoubleScalar() && R[I.c].isDoubleScalar()) {
                R[I.a].setScalarFast(R[I.b].scalarVal() + R[I.c].scalarVal());
            } else
                goto binary_slow;
            break;
        case OpCode::SUB:
            if (R[I.b].isDoubleScalar() && R[I.c].isDoubleScalar()) {
                R[I.a].setScalarFast(R[I.b].scalarVal() - R[I.c].scalarVal());
            } else
                goto binary_slow;
            break;
        case OpCode::MUL:
        case OpCode::EMUL:
            if (R[I.b].isDoubleScalar() && R[I.c].isDoubleScalar()) {
                R[I.a].setScalarFast(R[I.b].scalarVal() * R[I.c].scalarVal());
            } else
                goto binary_slow;
            break;
        case OpCode::RDIV:
        case OpCode::ERDIV:
            if (R[I.b].isDoubleScalar() && R[I.c].isDoubleScalar()) {
                R[I.a].setScalarFast(R[I.b].scalarVal() / R[I.c].scalarVal());
            } else
                goto binary_slow;
            break;
        case OpCode::LDIV:
        case OpCode::ELDIV:
            if (R[I.b].isDoubleScalar() && R[I.c].isDoubleScalar()) {
                R[I.a].setScalarFast(R[I.c].scalarVal() / R[I.b].scalarVal());
            } else
                goto binary_slow;
            break;
        case OpCode::POW:
        case OpCode::EPOW:
            if (R[I.b].isDoubleScalar() && R[I.c].isDoubleScalar()) {
                R[I.a].setScalarFast(std::pow(R[I.b].scalarVal(), R[I.c].scalarVal()));
            } else
                goto binary_slow;
            break;

        // ── Comparison ───────────────────────────────────────
        case OpCode::EQ:
            if (R[I.b].isDoubleScalar() && R[I.c].isDoubleScalar()) {
                R[I.a].setScalarFast(R[I.b].scalarVal() == R[I.c].scalarVal() ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;
        case OpCode::NE:
            if (R[I.b].isDoubleScalar() && R[I.c].isDoubleScalar()) {
                R[I.a].setScalarFast(R[I.b].scalarVal() != R[I.c].scalarVal() ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;
        case OpCode::LT:
            if (R[I.b].isDoubleScalar() && R[I.c].isDoubleScalar()) {
                R[I.a].setScalarFast(R[I.b].scalarVal() < R[I.c].scalarVal() ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;
        case OpCode::GT:
            if (R[I.b].isDoubleScalar() && R[I.c].isDoubleScalar()) {
                R[I.a].setScalarFast(R[I.b].scalarVal() > R[I.c].scalarVal() ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;
        case OpCode::LE:
            if (R[I.b].isDoubleScalar() && R[I.c].isDoubleScalar()) {
                R[I.a].setScalarFast(R[I.b].scalarVal() <= R[I.c].scalarVal() ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;
        case OpCode::GE:
            if (R[I.b].isDoubleScalar() && R[I.c].isDoubleScalar()) {
                R[I.a].setScalarFast(R[I.b].scalarVal() >= R[I.c].scalarVal() ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;
        case OpCode::AND:
            if (R[I.b].isDoubleScalar() && R[I.c].isDoubleScalar()) {
                R[I.a].setScalarFast(
                    (R[I.b].scalarVal() != 0.0 && R[I.c].scalarVal() != 0.0) ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;
        case OpCode::OR:
            if (R[I.b].isDoubleScalar() && R[I.c].isDoubleScalar()) {
                R[I.a].setScalarFast(
                    (R[I.b].scalarVal() != 0.0 || R[I.c].scalarVal() != 0.0) ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;

        // ── Unary ────────────────────────────────────────────
        case OpCode::NEG:
            if (R[I.b].isDoubleScalar()) {
                R[I.a].setScalarFast(-R[I.b].scalarVal());
            } else
                goto unary_slow;
            break;
        case OpCode::UPLUS:
            R[I.a] = R[I.b];
            break;
        case OpCode::NOT:
            if (R[I.b].isDoubleScalar()) {
                R[I.a].setScalarFast(R[I.b].scalarVal() == 0.0 ? 1.0 : 0.0);
            } else
                goto unary_slow;
            break;
        case OpCode::CTRANSPOSE:
        case OpCode::TRANSPOSE:
            if (R[I.b].isDoubleScalar()) {
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
            fs.index = 0;
            if (R[I.b].isDoubleScalar()) {
                fs.range = R[I.b];
                fs.count = 1;
                fs.rows = 0;
                fs.data = nullptr;
            } else if (R[I.b].isEmpty()) {
                fs.count = 0;
            } else {
                fs.range = R[I.b];
                auto &dims = fs.range.dims();
                fs.count = dims.cols();
                fs.rows = dims.rows();
                fs.data = fs.range.doubleData();
            }
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
            double start = R[I.b].toScalar(), stop = R[I.c].toScalar();
            size_t cnt = colonCount(start, 1.0, stop);
            auto mat = MValue::matrix(1, cnt, MType::DOUBLE, &engine_.allocator_);
            if (cnt > 0) {
                double *d = mat.doubleDataMut();
                for (size_t i = 0; i < cnt; ++i)
                    d[i] = start + (double) i;
            }
            R[I.a] = std::move(mat);
            break;
        }
        case OpCode::COLON3: {
            double start = R[I.b].toScalar(), step = R[I.c].toScalar(), stop = R[I.e].toScalar();
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
            R[I.a] = std::move(mat);
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
            if (R[I.b].isDoubleScalar() && R[I.c].isDoubleScalar()) {
                R[I.a] = R[I.b]; // scalar(scalar) = scalar
            } else if (R[I.c].isDoubleScalar()) {
                size_t i = (size_t) R[I.c].scalarVal() - 1;
                R[I.a] = MValue::scalar(R[I.b].doubleData()[i], &engine_.allocator_);
            } else {
                const MValue &mv = R[I.b];
                const MValue &ix = R[I.c];
                size_t n = ix.numel();
                auto res = MValue::matrix(1, n, MType::DOUBLE, &engine_.allocator_);
                double *dst = res.doubleDataMut();
                const double *src = mv.doubleData(), *id = ix.doubleData();
                for (size_t k = 0; k < n; ++k)
                    dst[k] = src[(size_t) id[k] - 1];
                R[I.a] = std::move(res);
            }
            break;
        }
        case OpCode::INDEX_GET_2D: {
            size_t r = (size_t) R[I.c].toScalar() - 1, c = (size_t) R[I.e].toScalar() - 1;
            const MValue &mv = R[I.b];
            R[I.a] = MValue::scalar(mv.doubleData()[mv.dims().sub2ind(r, c)], &engine_.allocator_);
            break;
        }
        case OpCode::INDEX_SET: {
            double val = R[I.c].toScalar();
            size_t i = (size_t) R[I.b].toScalar() - 1;
            if (R[I.a].isEmpty() || R[I.a].isDoubleScalar()) {
                R[I.a].ensureSize(i, &engine_.allocator_);
            } else if (i >= R[I.a].numel()) {
                R[I.a].ensureSize(i, &engine_.allocator_);
            }
            R[I.a].doubleDataMut()[i] = val;
            break;
        }
        case OpCode::INDEX_SET_2D: {
            size_t r = (size_t) R[I.b].toScalar() - 1, c = (size_t) R[I.c].toScalar() - 1;
            R[I.a].doubleDataMut()[R[I.a].dims().sub2ind(r, c)] = R[I.e].toScalar();
            break;
        }

        // ── Inline scalar builtins ───────────────────────────
        case OpCode::CALL_BUILTIN: {
            uint8_t argBase = I.b, na = I.c;
            int16_t bid = I.d;
            if (na == 1 && R[argBase].isDoubleScalar()) {
                double v = R[argBase].scalarVal(), result;
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
            if (na == 2 && R[argBase].isDoubleScalar() && R[argBase + 1].isDoubleScalar()) {
                double a = R[argBase].scalarVal(), b = R[argBase + 1].scalarVal(), result;
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
                    Span<const MValue> as(&R[argBase], na);
                    MValue ob[1];
                    Span<MValue> os(ob, 1);
                    extIt->second(as, 1, os);
                    R[I.a] = std::move(ob[0]);
                    break;
                }
            }
            throw std::runtime_error("VM: unsupported builtin");
        }
        }

        // ── General function calls ───────────────────────────
        case OpCode::CALL: {
            uint8_t argBase = I.b, na = I.c;
            int16_t funcIdx = I.d;
            if (funcIdx < (int16_t) resolvedFuncs.size() && resolvedFuncs[funcIdx]) {
                R[I.a] = callUserFunc(*resolvedFuncs[funcIdx], &R[argBase], na);
                break;
            }
            const std::string &funcName = chunk.strings[funcIdx];
            if (compiledFuncs_) {
                auto cfIt = compiledFuncs_->find(funcName);
                if (cfIt != compiledFuncs_->end()) {
                    if (funcIdx >= (int16_t) resolvedFuncs.size())
                        resolvedFuncs.resize(funcIdx + 1, nullptr);
                    resolvedFuncs[funcIdx] = &cfIt->second;
                    R[I.a] = callUserFunc(cfIt->second, &R[argBase], na);
                    break;
                }
            }
            // External func — args already in register file, pass directly
            auto extIt = engine_.externalFuncs_.find(funcName);
            if (extIt != engine_.externalFuncs_.end()) {
                Span<const MValue> as(&R[argBase], na);
                MValue ob[1];
                Span<MValue> os(ob, 1);
                extIt->second(as, 1, os);
                R[I.a] = std::move(ob[0]);
                break;
            }
            throw std::runtime_error("VM: undefined function '" + funcName + "'");
        }

        // ── Display ──────────────────────────────────────────
        case OpCode::DISPLAY: {
            const std::string &name = chunk.strings[I.d];
            std::ostringstream os;
            if (R[I.a].isDoubleScalar())
                os << name << " = " << R[I.a].scalarVal() << "\n\n";
            else if (R[I.a].isEmpty())
                os << name << " = []\n\n";
            else if (R[I.a].isChar())
                os << name << " = '" << R[I.a].toString() << "'\n\n";
            else
                os << name << " = " << R[I.a].debugString() << "\n\n";
            engine_.outputText(os.str());
            break;
        }

        // ── Return ───────────────────────────────────────────
        case OpCode::RET:
            return R[I.a];
        case OpCode::RET_EMPTY:
            return MValue::empty();
        case OpCode::HALT:
            return MValue::empty();
        case OpCode::NOP:
            break;

        default:
            throw std::runtime_error("VM: unimplemented opcode " + std::to_string((int) I.op));

        // ── Slow paths ───────────────────────────────────────
        binary_slow: {
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
                    R[I.a] = it->second(R[I.b], R[I.c]);
                    break;
                }
            }
            throw std::runtime_error("VM: unsupported binary op");
        }
        unary_slow: {
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
                    R[I.a] = it->second(R[I.b]);
                    break;
                }
            }
            throw std::runtime_error("VM: unsupported unary op");
        }

        } // switch
        ++ip;
    } // while

    return MValue::empty();
}

// ============================================================
// User function call — no VMValue conversion needed
// ============================================================

MValue VM::callUserFunc(const BytecodeChunk &funcChunk, const MValue *args, uint8_t nargs)
{
    if (++recursionDepth_ > kMaxRecursion) {
        --recursionDepth_;
        throw std::runtime_error("VM: maximum recursion depth exceeded");
    }

    uint8_t pc = std::min(nargs, funcChunk.numParams);
    MValue argsCopy[16];
    for (uint8_t i = 0; i < pc; ++i)
        argsCopy[i] = args[i];

    size_t savedTop = regStackTop_;
    MValue *savedR = R_;
    size_t savedForSize = forStack_.size();

    uint8_t nregs = funcChunk.numRegisters;
    if (regStackTop_ + nregs > kRegStackSize) {
        --recursionDepth_;
        throw std::runtime_error("VM: register stack overflow");
    }

    R_ = regStack_.data() + regStackTop_;
    regStackTop_ += nregs;

    for (uint8_t i = 0; i < pc; ++i)
        R_[i] = std::move(argsCopy[i]);

    MValue result = executeInternal(funcChunk);

    // Cleanup: release heap objects in callee frame
    for (uint8_t i = 0; i < nregs; ++i) {
        if (!R_[i].isDoubleScalar() && !R_[i].isEmpty())
            R_[i] = MValue::empty();
    }

    regStackTop_ = savedTop;
    R_ = savedR;
    forStack_.resize(savedForSize);
    --recursionDepth_;
    return result;
}

// ============================================================
// Array helpers
// ============================================================

void VM::executeHorzcat(MValue &dst, const MValue *regs, uint8_t count)
{
    size_t total = 0;
    for (uint8_t i = 0; i < count; ++i) {
        if (regs[i].isEmpty())
            continue;
        if (regs[i].isDoubleScalar()) {
            total++;
            continue;
        }
        total += regs[i].numel();
    }
    auto result = MValue::matrix(1, total, MType::DOUBLE, &engine_.allocator_);
    double *d = result.doubleDataMut();
    size_t pos = 0;
    for (uint8_t i = 0; i < count; ++i) {
        if (regs[i].isEmpty())
            continue;
        if (regs[i].isDoubleScalar()) {
            d[pos++] = regs[i].scalarVal();
            continue;
        }
        const double *src = regs[i].doubleData();
        size_t n = regs[i].numel();
        std::copy(src, src + n, d + pos);
        pos += n;
    }
    dst = std::move(result);
}

void VM::executeVertcat(MValue &dst, const MValue *regs, uint8_t count)
{
    size_t totalRows = 0, cols = 0;
    for (uint8_t i = 0; i < count; ++i) {
        if (regs[i].isEmpty())
            continue;
        if (regs[i].isDoubleScalar()) {
            totalRows++;
            if (!cols)
                cols = 1;
            continue;
        }
        auto &dims = regs[i].dims();
        totalRows += dims.rows();
        if (!cols)
            cols = dims.cols();
    }
    if (!cols) {
        dst = MValue::empty();
        return;
    }
    auto result = MValue::matrix(totalRows, cols, MType::DOUBLE, &engine_.allocator_);
    double *d = result.doubleDataMut();
    size_t rowOff = 0;
    for (uint8_t i = 0; i < count; ++i) {
        if (regs[i].isEmpty())
            continue;
        if (regs[i].isDoubleScalar()) {
            d[rowOff++] = regs[i].scalarVal();
            continue;
        }
        auto &dims = regs[i].dims();
        const double *src = regs[i].doubleData();
        for (size_t c = 0; c < cols; ++c)
            for (size_t r = 0; r < dims.rows(); ++r)
                d[(rowOff + r) + c * totalRows] = src[r + c * dims.rows()];
        rowOff += dims.rows();
    }
    dst = std::move(result);
}

void VM::forSetVar(MValue &varReg, const ForState &fs)
{
    if (fs.rows == 0) {
        varReg.setScalarVal(fs.range.scalarVal());
        return;
    }
    if (fs.rows == 1) {
        varReg.setScalarVal(fs.data[fs.index]);
        return;
    }
    size_t rows = fs.rows;
    auto col = MValue::matrix(rows, 1, MType::DOUBLE, &engine_.allocator_);
    double *dst = col.doubleDataMut();
    const double *src = fs.data + fs.index * rows;
    for (size_t r = 0; r < rows; ++r)
        dst[r] = src[r];
    varReg = std::move(col);
}

} // namespace mlab