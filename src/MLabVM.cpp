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
{}

// Helper: get scalar from register (inline, no allocation)
static inline double regScalar(const VMValue &r)
{
    return r.scalar(); // asserts Tag::SCALAR
}

// Helper: colon count
static inline size_t colonCount(double start, double step, double stop)
{
    if (step == 0.0)
        return 0;
    double n = (stop - start) / step;
    if (n < 0)
        return 0;
    return static_cast<size_t>(std::floor(n + 1e-10)) + 1;
}

MValue VM::execute(const BytecodeChunk &chunk, const MValue *args, uint8_t nargs)
{
    registers_.resize(chunk.numRegisters);
    for (auto &r : registers_)
        r = VMValue();

    // Load function arguments
    if (args) {
        uint8_t pc = std::min(nargs, chunk.numParams);
        for (uint8_t i = 0; i < pc; ++i)
            registers_[i] = VMValue::fromMValue(args[i]);
    }

    const Instruction *ip = chunk.code.data();
    const Instruction *end = ip + chunk.code.size();
    auto &R = registers_; // alias for brevity

    while (ip < end) {
        const Instruction &I = *ip;

        switch (I.op) {
        // ── Data movement ────────────────────────────────────
        case OpCode::LOAD_CONST: {
            const MValue &cv = chunk.constants[I.d];
            if (cv.isScalar() && cv.type() == MType::DOUBLE)
                R[I.a].setScalar(cv.toScalar());
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

        // ── Scalar arithmetic (fast path — no MValue) ────────
        case OpCode::ADD:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalar(R[I.b].scalar() + R[I.c].scalar());
            } else
                goto binary_slow;
            break;
        case OpCode::SUB:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalar(R[I.b].scalar() - R[I.c].scalar());
            } else
                goto binary_slow;
            break;
        case OpCode::MUL:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalar(R[I.b].scalar() * R[I.c].scalar());
            } else
                goto binary_slow;
            break;
        case OpCode::RDIV:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalar(R[I.b].scalar() / R[I.c].scalar());
            } else
                goto binary_slow;
            break;
        case OpCode::POW:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalar(std::pow(R[I.b].scalar(), R[I.c].scalar()));
            } else
                goto binary_slow;
            break;
        case OpCode::LDIV:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalar(R[I.c].scalar() / R[I.b].scalar());
            } else
                goto binary_slow;
            break;

        // Element-wise (same as regular for scalars)
        case OpCode::EMUL:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalar(R[I.b].scalar() * R[I.c].scalar());
            } else
                goto binary_slow;
            break;
        case OpCode::ERDIV:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalar(R[I.b].scalar() / R[I.c].scalar());
            } else
                goto binary_slow;
            break;
        case OpCode::ELDIV:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalar(R[I.c].scalar() / R[I.b].scalar());
            } else
                goto binary_slow;
            break;
        case OpCode::EPOW:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalar(std::pow(R[I.b].scalar(), R[I.c].scalar()));
            } else
                goto binary_slow;
            break;

        // ── Comparison (fast path) ───────────────────────────
        case OpCode::EQ:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalar(R[I.b].scalar() == R[I.c].scalar() ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;
        case OpCode::NE:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalar(R[I.b].scalar() != R[I.c].scalar() ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;
        case OpCode::LT:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalar(R[I.b].scalar() < R[I.c].scalar() ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;
        case OpCode::GT:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalar(R[I.b].scalar() > R[I.c].scalar() ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;
        case OpCode::LE:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalar(R[I.b].scalar() <= R[I.c].scalar() ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;
        case OpCode::GE:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalar(R[I.b].scalar() >= R[I.c].scalar() ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;

        // ── Logical (fast path) ──────────────────────────────
        case OpCode::AND:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalar((R[I.b].scalar() != 0.0 && R[I.c].scalar() != 0.0) ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;
        case OpCode::OR:
            if (R[I.b].isScalar() && R[I.c].isScalar()) {
                R[I.a].setScalar((R[I.b].scalar() != 0.0 || R[I.c].scalar() != 0.0) ? 1.0 : 0.0);
            } else
                goto binary_slow;
            break;

        // ── Unary (fast path) ────────────────────────────────
        case OpCode::NEG:
            if (R[I.b].isScalar()) {
                R[I.a].setScalar(-R[I.b].scalar());
            } else
                goto unary_slow;
            break;
        case OpCode::UPLUS:
            R[I.a] = R[I.b];
            break;
        case OpCode::NOT:
            if (R[I.b].isScalar()) {
                R[I.a].setScalar(R[I.b].scalar() == 0.0 ? 1.0 : 0.0);
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
            double start = R[I.b].toDouble();
            double stop = R[I.c].toDouble();
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
            double start = R[I.b].toDouble();
            double step = R[I.c].toDouble();
            double stop = R[I.e].toDouble();
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
            const VMValue &arr = R[I.b];
            const VMValue &idx = R[I.c];

            if (arr.isScalar() && idx.isScalar()) {
                R[I.a].setScalar(arr.scalar());
            } else if (idx.isScalar() && arr.isMValue()) {
                size_t i = (size_t) idx.scalar() - 1;
                const MValue &mv = arr.mvalue();
                if (mv.type() == MType::DOUBLE) {
                    R[I.a].setScalar(mv.doubleData()[i]);
                } else {
                    throw std::runtime_error("VM: unsupported type for indexing");
                }
            } else if (idx.isMValue() && arr.isMValue()) {
                const MValue &mv = arr.mvalue();
                const MValue &ix = idx.mvalue();
                size_t n = ix.numel();
                auto res = MValue::matrix(1, n, MType::DOUBLE, &engine_.allocator_);
                double *dst = res.doubleDataMut();
                const double *src = mv.doubleData();
                const double *id = ix.doubleData();
                for (size_t k = 0; k < n; ++k)
                    dst[k] = src[(size_t) id[k] - 1];
                R[I.a].setMValue(std::move(res));
            } else {
                throw std::runtime_error("VM: unsupported INDEX_GET operands");
            }
            break;
        }

        case OpCode::INDEX_GET_2D: {
            const VMValue &arr = R[I.b];
            if (arr.isMValue() && R[I.c].isScalar() && R[I.e].isScalar()) {
                size_t r = (size_t) R[I.c].scalar() - 1;
                size_t c = (size_t) R[I.e].scalar() - 1;
                const MValue &mv = arr.mvalue();
                R[I.a].setScalar(mv.doubleData()[mv.dims().sub2ind(r, c)]);
            } else {
                throw std::runtime_error("VM: non-scalar 2D indexing not supported");
            }
            break;
        }

        case OpCode::INDEX_SET: {
            VMValue &arr = R[I.a];
            double val = R[I.c].toDouble();
            size_t i = (size_t) R[I.b].toDouble() - 1;

            if (arr.isMValue()) {
                MValue &mv = arr.mvalue();
                if (i >= mv.numel()) {
                    size_t ns = i + 1;
                    auto grown = MValue::matrix(1, ns, MType::DOUBLE, &engine_.allocator_);
                    double *dst = grown.doubleDataMut();
                    std::fill(dst, dst + ns, 0.0);
                    if (!mv.isEmpty()) {
                        const double *src = mv.doubleData();
                        std::copy(src, src + mv.numel(), dst);
                    }
                    mv = std::move(grown);
                }
                mv.doubleDataMut()[i] = val;
            } else if (arr.isEmpty() || arr.isScalar()) {
                size_t ns = i + 1;
                auto mat = MValue::matrix(1, ns, MType::DOUBLE, &engine_.allocator_);
                double *dst = mat.doubleDataMut();
                std::fill(dst, dst + ns, 0.0);
                if (arr.isScalar() && ns >= 1)
                    dst[0] = arr.scalar();
                dst[i] = val;
                arr.setMValue(std::move(mat));
            }
            break;
        }

        case OpCode::INDEX_SET_2D: {
            VMValue &arr = R[I.a];
            if (arr.isMValue()) {
                size_t r = (size_t) R[I.b].toDouble() - 1;
                size_t c = (size_t) R[I.c].toDouble() - 1;
                double v = R[I.e].toDouble();
                MValue &mv = arr.mvalue();
                mv.doubleDataMut()[mv.dims().sub2ind(r, c)] = v;
            }
            break;
        }

        // ── Function calls ───────────────────────────────────
        case OpCode::CALL: {
            const std::string &funcName = chunk.strings[I.d];
            uint8_t argBase = I.b;
            uint8_t na = I.c;

            // 0. Inline scalar builtins — zero allocation
            if (na == 1 && R[argBase].isScalar()) {
                double v = R[argBase].scalar();
                bool handled = true;
                double result;
                if (funcName == "abs")
                    result = std::fabs(v);
                else if (funcName == "floor")
                    result = std::floor(v);
                else if (funcName == "ceil")
                    result = std::ceil(v);
                else if (funcName == "round")
                    result = std::round(v);
                else if (funcName == "fix")
                    result = std::trunc(v);
                else if (funcName == "sqrt")
                    result = std::sqrt(v);
                else if (funcName == "exp")
                    result = std::exp(v);
                else if (funcName == "log")
                    result = std::log(v);
                else if (funcName == "log2")
                    result = std::log2(v);
                else if (funcName == "log10")
                    result = std::log10(v);
                else if (funcName == "sin")
                    result = std::sin(v);
                else if (funcName == "cos")
                    result = std::cos(v);
                else if (funcName == "tan")
                    result = std::tan(v);
                else if (funcName == "sign")
                    result = (v > 0.0) ? 1.0 : (v < 0.0) ? -1.0 : 0.0;
                else if (funcName == "isnan")
                    result = std::isnan(v) ? 1.0 : 0.0;
                else if (funcName == "isinf")
                    result = std::isinf(v) ? 1.0 : 0.0;
                else
                    handled = false;
                if (handled) {
                    R[I.a].setScalar(result);
                    break;
                }
            }
            if (na == 2 && R[argBase].isScalar() && R[argBase + 1].isScalar()) {
                double a = R[argBase].scalar();
                double b = R[argBase + 1].scalar();
                bool handled = true;
                double result;
                if (funcName == "mod") {
                    result = std::fmod(a, b);
                    if (result != 0.0 && ((result > 0) != (b > 0)))
                        result += b;
                } else if (funcName == "rem")
                    result = std::fmod(a, b);
                else if (funcName == "max")
                    result = (a >= b) ? a : b;
                else if (funcName == "min")
                    result = (a <= b) ? a : b;
                else if (funcName == "pow")
                    result = std::pow(a, b);
                else if (funcName == "atan2")
                    result = std::atan2(a, b);
                else
                    handled = false;
                if (handled) {
                    R[I.a].setScalar(result);
                    break;
                }
            }

            // 1. Compiled user function
            if (compiledFuncs_) {
                auto cfIt = compiledFuncs_->find(funcName);
                if (cfIt != compiledFuncs_->end()) {
                    std::vector<MValue> margs(na);
                    for (uint8_t i = 0; i < na; ++i)
                        margs[i] = R[argBase + i].toMValue(&engine_.allocator_);

                    MValue result = executeCall(cfIt->second, margs.data(), na);
                    R[I.a] = VMValue::fromMValue(std::move(result));
                    break;
                }
            }

            // 2. External/builtin function
            auto extIt = engine_.externalFuncs_.find(funcName);
            if (extIt != engine_.externalFuncs_.end()) {
                std::vector<MValue> margs(na);
                for (uint8_t i = 0; i < na; ++i)
                    margs[i] = R[argBase + i].toMValue(&engine_.allocator_);

                Span<const MValue> argsSpan(margs.data(), na);
                MValue outBuf[1];
                Span<MValue> outsSpan(outBuf, 1);
                extIt->second(argsSpan, 1, outsSpan);
                R[I.a] = VMValue::fromMValue(std::move(outBuf[0]));
                break;
            }

            throw std::runtime_error("VM: undefined function '" + funcName + "'");
        }

        // ── Display ──────────────────────────────────────────
        case OpCode::DISPLAY: {
            const std::string &name = chunk.strings[I.d];
            std::ostringstream os;
            if (R[I.a].isScalar()) {
                os << name << " = " << R[I.a].scalar() << "\n\n";
            } else if (R[I.a].isEmpty()) {
                os << name << " = []\n\n";
            } else {
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
            return R[I.a].toMValue(&engine_.allocator_);

        case OpCode::RET_EMPTY:
            return MValue::empty();

        case OpCode::HALT:
            return MValue::empty();

        case OpCode::NOP:
            break;

        default:
            throw std::runtime_error("VM: unimplemented opcode "
                                     + std::to_string(static_cast<int>(I.op)));

        // ── Slow paths (array fallback) ──────────────────────
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
            throw std::runtime_error("VM: unsupported binary op on arrays");
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

    return MValue::empty();
}

// ============================================================
// Array construction
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
        } else {
            const MValue &mv = regs[i].mvalue();
            const double *src = mv.doubleData();
            size_t n = mv.numel();
            std::copy(src, src + n, d + pos);
            pos += n;
        }
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
            d[rowOff] = regs[i].scalar();
            rowOff++;
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

// ============================================================
// For-loop helper
// ============================================================

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
        } else {
            size_t rows = dims.rows();
            auto col = MValue::matrix(rows, 1, MType::DOUBLE, &engine_.allocator_);
            double *dst = col.doubleDataMut();
            const double *src = range.doubleData();
            for (size_t r = 0; r < rows; ++r)
                dst[r] = src[r + fs.index * rows];
            varReg.setMValue(std::move(col));
        }
        return;
    }
    throw std::runtime_error("VM: unsupported type in for-loop range");
}

// ============================================================
// Function call
// ============================================================

MValue VM::executeCall(const BytecodeChunk &funcChunk, const MValue *args, uint8_t nargs)
{
    if (++recursionDepth_ > kMaxRecursion) {
        --recursionDepth_;
        throw std::runtime_error("VM: maximum recursion depth exceeded");
    }

    auto savedRegs = std::move(registers_);
    auto savedForStack = std::move(forStack_);

    MValue result = execute(funcChunk, args, nargs);

    registers_ = std::move(savedRegs);
    forStack_ = std::move(savedForStack);
    --recursionDepth_;
    return result;
}

} // namespace mlab