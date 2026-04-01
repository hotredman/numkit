// src/MLabVM.cpp
#include "MLabVM.hpp"
#include "MLabEngine.hpp"

#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace mlab {

VM::VM(Engine &engine)
    : engine_(engine)
{}

MValue VM::execute(const BytecodeChunk &chunk)
{
    registers_.resize(chunk.numRegisters);
    for (auto &r : registers_)
        r = MValue::empty();

    const Instruction *ip = chunk.code.data();
    const Instruction *end = ip + chunk.code.size();

    while (ip < end) {
        const Instruction &instr = *ip;

        switch (instr.op) {
        // ── Data movement ────────────────────────────────────
        case OpCode::LOAD_CONST:
            registers_[instr.a] = chunk.constants[instr.d];
            break;

        case OpCode::LOAD_EMPTY:
            registers_[instr.a] = MValue::empty();
            break;

        case OpCode::LOAD_STRING:
            registers_[instr.a] = MValue::fromString(chunk.strings[instr.d], &engine_.allocator_);
            break;

        case OpCode::MOVE:
            registers_[instr.a] = registers_[instr.b];
            break;

        case OpCode::COLON_ALL:
            // Marker value for ':'  — use a special string marker
            registers_[instr.a] = MValue::fromString(":", &engine_.allocator_);
            break;

        // ── Arithmetic ───────────────────────────────────────
        case OpCode::ADD:
        case OpCode::SUB:
        case OpCode::MUL:
        case OpCode::RDIV:
        case OpCode::LDIV:
        case OpCode::POW:
        case OpCode::EMUL:
        case OpCode::ERDIV:
        case OpCode::ELDIV:
        case OpCode::EPOW:
            executeBinaryOp(instr.op, registers_[instr.a], registers_[instr.b], registers_[instr.c]);
            break;

        // ── Comparison ───────────────────────────────────────
        case OpCode::EQ:
        case OpCode::NE:
        case OpCode::LT:
        case OpCode::GT:
        case OpCode::LE:
        case OpCode::GE:
            executeBinaryOp(instr.op, registers_[instr.a], registers_[instr.b], registers_[instr.c]);
            break;

        // ── Logical ──────────────────────────────────────────
        case OpCode::AND:
        case OpCode::OR:
            executeBinaryOp(instr.op, registers_[instr.a], registers_[instr.b], registers_[instr.c]);
            break;

        // ── Unary ────────────────────────────────────────────
        case OpCode::NEG:
        case OpCode::UPLUS:
        case OpCode::NOT:
        case OpCode::CTRANSPOSE:
        case OpCode::TRANSPOSE:
            executeUnaryOp(instr.op, registers_[instr.a], registers_[instr.b]);
            break;

        // ── Control flow ─────────────────────────────────────
        case OpCode::JMP:
            ip += instr.d;
            continue; // skip ip++ at bottom

        case OpCode::JMP_TRUE:
            if (registers_[instr.a].toBool()) {
                ip += instr.d;
                continue;
            }
            break;

        case OpCode::JMP_FALSE:
            if (!registers_[instr.a].toBool()) {
                ip += instr.d;
                continue;
            }
            break;

        // ── For-loop ─────────────────────────────────────────
        case OpCode::FOR_INIT: {
            // a = varReg, b = rangeReg, d = endOffset (jump if empty)
            const MValue &range = registers_[instr.b];

            ForState fs;
            fs.range = range;
            fs.index = 0;

            if (range.isEmpty()) {
                fs.count = 0;
            } else if (range.isScalar()) {
                fs.count = 1;
            } else {
                fs.count = range.dims().cols();
            }

            if (fs.count == 0) {
                // Empty range — skip loop
                ip += instr.d;
                continue;
            }

            // Set variable to first element
            forStack_.push_back(std::move(fs));
            forSetVar(registers_[instr.a], forStack_.back());
            break;
        }

        case OpCode::FOR_NEXT: {
            // a = varReg, d = backOffset (jump to body start)
            auto &fs = forStack_.back();
            fs.index++;

            if (fs.index < fs.count) {
                // More iterations: set var, jump back to body
                forSetVar(registers_[instr.a], fs);
                ip += instr.d;
                continue;
            }

            // Done: pop state, fall through
            forStack_.pop_back();
            break;
        }

        // ── Colon expressions ────────────────────────────────
        case OpCode::COLON:
            // dst=a, start=R[b], stop=R[c]
            executeColon(registers_[instr.a],
                         registers_[instr.b].toScalar(),
                         registers_[instr.c].toScalar());
            break;

        case OpCode::COLON3:
            // dst=a, start=R[b], step=R[c], stop=R[e]
            executeColon3(registers_[instr.a],
                          registers_[instr.b].toScalar(),
                          registers_[instr.c].toScalar(),
                          registers_[instr.e].toScalar());
            break;

        // ── Array construction ───────────────────────────────
        case OpCode::HORZCAT:
            // dst=a, base=b, count=c
            executeHorzcat(registers_[instr.a], &registers_[instr.b], instr.c);
            break;

        case OpCode::VERTCAT:
            // dst=a, base=b, count=c
            executeVertcat(registers_[instr.a], &registers_[instr.b], instr.c);
            break;

        // ── Array indexing ───────────────────────────────────
        case OpCode::INDEX_GET: {
            // dst=a, arr=R[b], idx=R[c]
            const MValue &arr = registers_[instr.b];
            const MValue &idx = registers_[instr.c];

            if (arr.isScalar() && idx.isScalar()) {
                // Scalar indexing into scalar — just return the value
                size_t i = static_cast<size_t>(idx.toScalar()) - 1; // 1-based
                if (i == 0) {
                    registers_[instr.a] = arr;
                } else {
                    throw std::runtime_error("VM: index out of bounds");
                }
            } else if (idx.isScalar()) {
                size_t i = static_cast<size_t>(idx.toScalar()) - 1;
                if (arr.type() == MType::DOUBLE) {
                    registers_[instr.a] = MValue::scalar(arr.doubleData()[i], &engine_.allocator_);
                } else {
                    throw std::runtime_error("VM: unsupported type for indexing");
                }
            } else {
                // Vector indexing — return sub-array
                size_t n = idx.numel();
                auto result = MValue::matrix(1, n, MType::DOUBLE, &engine_.allocator_);
                double *dst = result.doubleDataMut();
                const double *src = arr.doubleData();
                const double *idxData = idx.doubleData();
                for (size_t k = 0; k < n; ++k) {
                    dst[k] = src[static_cast<size_t>(idxData[k]) - 1];
                }
                registers_[instr.a] = std::move(result);
            }
            break;
        }

        case OpCode::INDEX_GET_2D: {
            // dst=a, arr=R[b], row=R[c], col=R[e]
            const MValue &arr = registers_[instr.b];
            const MValue &row = registers_[instr.c];
            const MValue &col = registers_[instr.e];

            if (row.isScalar() && col.isScalar()) {
                size_t r = static_cast<size_t>(row.toScalar()) - 1;
                size_t c = static_cast<size_t>(col.toScalar()) - 1;
                registers_[instr.a] = MValue::scalar(arr.doubleData()[arr.dims().sub2ind(r, c)],
                                                     &engine_.allocator_);
            } else {
                throw std::runtime_error("VM: non-scalar 2D indexing not yet supported");
            }
            break;
        }

        case OpCode::INDEX_SET: {
            // arr=R[a], idx=R[b], val=R[c]
            MValue &arr = registers_[instr.a];
            const MValue &idx = registers_[instr.b];
            const MValue &val = registers_[instr.c];

            if (idx.isScalar()) {
                size_t i = static_cast<size_t>(idx.toScalar()) - 1;
                if (arr.type() == MType::DOUBLE) {
                    // Auto-grow if needed
                    if (i >= arr.numel()) {
                        size_t newSize = i + 1;
                        auto grown = MValue::matrix(1, newSize, MType::DOUBLE, &engine_.allocator_);
                        double *dst = grown.doubleDataMut();
                        std::fill(dst, dst + newSize, 0.0);
                        if (!arr.isEmpty()) {
                            const double *src = arr.doubleData();
                            std::copy(src, src + arr.numel(), dst);
                        }
                        arr = std::move(grown);
                    }
                    arr.doubleDataMut()[i] = val.toScalar();
                } else if (arr.isEmpty()) {
                    // First assignment to empty — create array
                    size_t newSize = i + 1;
                    arr = MValue::matrix(1, newSize, MType::DOUBLE, &engine_.allocator_);
                    double *dst = arr.doubleDataMut();
                    std::fill(dst, dst + newSize, 0.0);
                    dst[i] = val.toScalar();
                } else {
                    throw std::runtime_error("VM: unsupported type for index set");
                }
            } else {
                throw std::runtime_error("VM: non-scalar index set not yet supported");
            }
            break;
        }

        case OpCode::INDEX_SET_2D: {
            // arr=R[a], row=R[b], col=R[c], val=R[e]
            MValue &arr = registers_[instr.a];
            size_t r = static_cast<size_t>(registers_[instr.b].toScalar()) - 1;
            size_t c = static_cast<size_t>(registers_[instr.c].toScalar()) - 1;
            double v = registers_[instr.e].toScalar();

            if (arr.type() == MType::DOUBLE) {
                arr.doubleDataMut()[arr.dims().sub2ind(r, c)] = v;
            } else {
                throw std::runtime_error("VM: unsupported type for 2D index set");
            }
            break;
        }

        // ── Display ──────────────────────────────────────────
        case OpCode::DISPLAY: {
            const std::string &name = chunk.strings[instr.d];
            const MValue &val = registers_[instr.a];
            std::ostringstream os;
            if (val.isScalar() && val.type() == MType::DOUBLE) {
                os << name << " = " << val.toScalar() << "\n\n";
            } else if (val.isChar()) {
                os << name << " = '" << val.toString() << "'\n\n";
            } else if (val.isEmpty()) {
                os << name << " = []\n\n";
            } else {
                os << name << " = " << val.debugString() << "\n\n";
            }
            engine_.outputText(os.str());
            break;
        }

        // ── Return ───────────────────────────────────────────
        case OpCode::RET:
            return registers_[instr.a];

        case OpCode::RET_EMPTY:
            return MValue::empty();

        // ── End ──────────────────────────────────────────────
        case OpCode::HALT:
            return MValue::empty();

        case OpCode::NOP:
            break;

        default:
            throw std::runtime_error("VM: unimplemented opcode "
                                     + std::to_string(static_cast<int>(instr.op)));
        }

        ++ip;
    }

    return MValue::empty();
}

// ============================================================
// Binary operations
// ============================================================

void VM::executeBinaryOp(OpCode op, MValue &dst, const MValue &a, const MValue &b)
{
    // Scalar fast path
    if (a.isScalar() && a.type() == MType::DOUBLE && b.isScalar() && b.type() == MType::DOUBLE) {
        double av = a.toScalar();
        double bv = b.toScalar();
        double result;

        switch (op) {
        case OpCode::ADD:
            result = av + bv;
            break;
        case OpCode::SUB:
            result = av - bv;
            break;
        case OpCode::MUL:
            result = av * bv;
            break;
        case OpCode::RDIV:
            result = av / bv;
            break;
        case OpCode::LDIV:
            result = bv / av;
            break;
        case OpCode::POW:
            result = std::pow(av, bv);
            break;
        case OpCode::EMUL:
            result = av * bv;
            break;
        case OpCode::ERDIV:
            result = av / bv;
            break;
        case OpCode::ELDIV:
            result = bv / av;
            break;
        case OpCode::EPOW:
            result = std::pow(av, bv);
            break;
        case OpCode::EQ:
            result = (av == bv) ? 1.0 : 0.0;
            break;
        case OpCode::NE:
            result = (av != bv) ? 1.0 : 0.0;
            break;
        case OpCode::LT:
            result = (av < bv) ? 1.0 : 0.0;
            break;
        case OpCode::GT:
            result = (av > bv) ? 1.0 : 0.0;
            break;
        case OpCode::LE:
            result = (av <= bv) ? 1.0 : 0.0;
            break;
        case OpCode::GE:
            result = (av >= bv) ? 1.0 : 0.0;
            break;
        case OpCode::AND:
            result = (av != 0.0 && bv != 0.0) ? 1.0 : 0.0;
            break;
        case OpCode::OR:
            result = (av != 0.0 || bv != 0.0) ? 1.0 : 0.0;
            break;
        default:
            throw std::runtime_error("VM: unknown binary opcode");
        }

        dst = MValue::scalar(result, &engine_.allocator_);
        return;
    }

    // Array fallback — delegate to Engine's registered binary ops
    const char *opStr = nullptr;
    switch (op) {
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
            dst = it->second(a, b);
            return;
        }
    }

    throw std::runtime_error("VM: unsupported binary operation on arrays");
}

// ============================================================
// Unary operations
// ============================================================

void VM::executeUnaryOp(OpCode op, MValue &dst, const MValue &src)
{
    // Scalar fast path
    if (src.isScalar() && src.type() == MType::DOUBLE) {
        double v = src.toScalar();
        switch (op) {
        case OpCode::NEG:
            dst = MValue::scalar(-v, &engine_.allocator_);
            return;
        case OpCode::UPLUS:
            dst = src;
            return;
        case OpCode::NOT:
            dst = MValue::scalar(v == 0.0 ? 1.0 : 0.0, &engine_.allocator_);
            return;
        default:
            break;
        }
    }

    // Array fallback
    const char *opStr = nullptr;
    switch (op) {
    case OpCode::NEG:
        opStr = "-";
        break;
    case OpCode::UPLUS:
        opStr = "+";
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
            dst = it->second(src);
            return;
        }
    }

    throw std::runtime_error("VM: unsupported unary operation");
}

// ============================================================
// Colon expressions
// ============================================================

static size_t colonCount(double start, double step, double stop)
{
    if (step == 0.0)
        return 0;
    double n = (stop - start) / step;
    if (n < 0)
        return 0;
    return static_cast<size_t>(std::floor(n + 1e-10)) + 1;
}

void VM::executeColon(MValue &dst, double start, double stop)
{
    size_t count = colonCount(start, 1.0, stop);
    auto result = MValue::matrix(1, count, MType::DOUBLE, &engine_.allocator_);
    if (count > 0) {
        double *d = result.doubleDataMut();
        for (size_t i = 0; i < count; ++i)
            d[i] = start + static_cast<double>(i);
    }
    dst = std::move(result);
}

void VM::executeColon3(MValue &dst, double start, double step, double stop)
{
    size_t count = colonCount(start, step, stop);
    auto result = MValue::matrix(1, count, MType::DOUBLE, &engine_.allocator_);
    if (count > 0) {
        double *d = result.doubleDataMut();
        for (size_t i = 0; i < count; ++i)
            d[i] = start + static_cast<double>(i) * step;
        // Snap last element to stop to avoid floating-point drift
        if (count >= 2) {
            double last = start + static_cast<double>(count - 1) * step;
            if ((step > 0 && last > stop) || (step < 0 && last < stop))
                d[count - 1] = stop;
        }
    }
    dst = std::move(result);
}

// ============================================================
// Array construction
// ============================================================

void VM::executeHorzcat(MValue &dst, const MValue *regs, uint8_t count)
{
    // Count total elements
    size_t total = 0;
    for (uint8_t i = 0; i < count; ++i) {
        if (regs[i].isEmpty())
            continue;
        total += regs[i].numel();
    }

    auto result = MValue::matrix(1, total, MType::DOUBLE, &engine_.allocator_);
    double *d = result.doubleDataMut();
    size_t pos = 0;
    for (uint8_t i = 0; i < count; ++i) {
        if (regs[i].isEmpty())
            continue;
        if (regs[i].isScalar() && regs[i].type() == MType::DOUBLE) {
            d[pos++] = regs[i].toScalar();
        } else if (regs[i].type() == MType::DOUBLE) {
            const double *src = regs[i].doubleData();
            size_t n = regs[i].numel();
            std::copy(src, src + n, d + pos);
            pos += n;
        }
    }
    dst = std::move(result);
}

void VM::executeVertcat(MValue &dst, const MValue *regs, uint8_t count)
{
    // Determine output dimensions
    size_t totalRows = 0;
    size_t cols = 0;
    for (uint8_t i = 0; i < count; ++i) {
        if (regs[i].isEmpty())
            continue;
        auto dims = regs[i].dims();
        totalRows += dims.rows();
        if (cols == 0)
            cols = dims.cols();
    }

    if (cols == 0) {
        dst = MValue::empty();
        return;
    }

    auto result = MValue::matrix(totalRows, cols, MType::DOUBLE, &engine_.allocator_);
    double *d = result.doubleDataMut();
    size_t rowOffset = 0;
    for (uint8_t i = 0; i < count; ++i) {
        if (regs[i].isEmpty())
            continue;
        auto dims = regs[i].dims();
        const double *src = regs[i].doubleData();
        // Copy column-major: for each col, copy rows
        for (size_t c = 0; c < cols; ++c) {
            for (size_t r = 0; r < dims.rows(); ++r) {
                d[(rowOffset + r) + c * totalRows] = src[r + c * dims.rows()];
            }
        }
        rowOffset += dims.rows();
    }
    dst = std::move(result);
}

// ============================================================
// For-loop helper
// ============================================================

void VM::forSetVar(MValue &varReg, const ForState &fs)
{
    const MValue &range = fs.range;

    if (range.isScalar()) {
        varReg = range;
        return;
    }

    if (range.type() == MType::DOUBLE) {
        auto dims = range.dims();
        if (dims.rows() == 1) {
            // Row vector: iterate elements
            varReg = MValue::scalar(range.doubleData()[fs.index], &engine_.allocator_);
        } else {
            // Matrix: iterate columns
            size_t rows = dims.rows();
            auto col = MValue::matrix(rows, 1, MType::DOUBLE, &engine_.allocator_);
            double *dst = col.doubleDataMut();
            const double *src = range.doubleData();
            for (size_t r = 0; r < rows; ++r)
                dst[r] = src[r + fs.index * rows];
            varReg = std::move(col);
        }
        return;
    }

    throw std::runtime_error("VM: unsupported type in for-loop range");
}

} // namespace mlab