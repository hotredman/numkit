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

    // Export script-level variables to lastVarMap_ for environment sync
    lastVarMap_.clear();
    for (auto &[name, reg] : chunk.varMap) {
        if (reg < chunk.numRegisters)
            lastVarMap_.push_back({name, R_[reg]});
    }

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

dispatch_loop:
    try {
        while (ip < end) {
            const Instruction &I = *ip;

            switch (I.op) {
            // ── Data movement ────────────────────────────────────
            case OpCode::LOAD_CONST: {
                const MValue &cv = chunk.constants[I.d];
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
                R[I.a] = MValue::empty();
                break;
            case OpCode::LOAD_STRING:
                R[I.a] = MValue::fromString(chunk.strings[I.d], &engine_.allocator_);
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
                double start = R[I.b].toScalar(), step = R[I.c].toScalar(),
                       stop = R[I.e].toScalar();
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
                    size_t i = checkedIndex(R[I.c].scalarVal(), R[I.b].numel());
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
                const MValue &mv = R[I.b];
                size_t r = (size_t) R[I.c].toScalar() - 1, c = (size_t) R[I.e].toScalar() - 1;
                R[I.a] = MValue::scalar(mv.doubleData()[mv.dims().sub2indChecked(r, c)],
                                        &engine_.allocator_);
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
                // Auto-expand if needed
                if (r >= R[I.a].dims().rows() || c >= R[I.a].dims().cols()) {
                    size_t newR = std::max(R[I.a].dims().rows(), r + 1);
                    size_t newC = std::max(R[I.a].dims().cols(), c + 1);
                    R[I.a].resize(newR, newC, &engine_.allocator_);
                }
                R[I.a].doubleDataMut()[R[I.a].dims().sub2ind(r, c)] = R[I.e].toScalar();
                break;
            }

            // ── ND array/cell indexing (3D+) ─────────────────────
            case OpCode::INDEX_GET_ND: {
                // a=dst, b=arr/cell, c=base, e=ndims
                uint8_t base = I.c, ndims = I.e;
                if (R[I.b].isCell()) {
                    // Cell ND indexing → linear index via sub2ind
                    size_t idx;
                    if (ndims == 3) {
                        size_t r = (size_t) R[base].toScalar() - 1;
                        size_t c = (size_t) R[base + 1].toScalar() - 1;
                        size_t p = (size_t) R[base + 2].toScalar() - 1;
                        idx = R[I.b].dims().sub2ind(r, c, p);
                    } else {
                        idx = (size_t) R[base].toScalar() - 1; // fallback linear
                    }
                    R[I.a] = R[I.b].cellAt(idx);
                } else {
                    // Double array ND indexing
                    const double *data = R[I.b].doubleData();
                    size_t idx;
                    if (ndims == 3) {
                        size_t r = (size_t) R[base].toScalar() - 1;
                        size_t c = (size_t) R[base + 1].toScalar() - 1;
                        size_t p = (size_t) R[base + 2].toScalar() - 1;
                        idx = R[I.b].dims().sub2ind(r, c, p);
                    } else {
                        idx = (size_t) R[base].toScalar() - 1;
                    }
                    R[I.a] = MValue::scalar(data[idx], &engine_.allocator_);
                }
                break;
            }
            case OpCode::INDEX_SET_ND: {
                // a=arr/cell, b=base, c=ndims, e=val
                uint8_t base = I.b, ndims = I.c;
                if (R[I.a].isCell()) {
                    size_t idx;
                    if (ndims == 3) {
                        size_t r = (size_t) R[base].toScalar() - 1;
                        size_t c = (size_t) R[base + 1].toScalar() - 1;
                        size_t p = (size_t) R[base + 2].toScalar() - 1;
                        idx = R[I.a].dims().sub2ind(r, c, p);
                    } else {
                        idx = (size_t) R[base].toScalar() - 1;
                    }
                    R[I.a].cellAt(idx) = R[I.e];
                } else {
                    size_t idx;
                    if (ndims == 3) {
                        size_t r = (size_t) R[base].toScalar() - 1;
                        size_t c = (size_t) R[base + 1].toScalar() - 1;
                        size_t p = (size_t) R[base + 2].toScalar() - 1;
                        idx = R[I.a].dims().sub2ind(r, c, p);
                    } else {
                        idx = (size_t) R[base].toScalar() - 1;
                    }
                    R[I.a].doubleDataMut()[idx] = R[I.e].toScalar();
                }
                break;
            }

            // ── Struct field access ──────────────────────────────
            case OpCode::FIELD_GET: {
                // a=dst, b=obj, d=nameIdx
                const std::string &fname = chunk.strings[I.d];
                if (!R[I.b].isStruct())
                    throw std::runtime_error("Dot indexing requires a struct");
                if (!R[I.b].hasField(fname))
                    throw std::runtime_error("Reference to non-existent field '" + fname + "'");
                R[I.a] = R[I.b].field(fname);
                break;
            }
            case OpCode::FIELD_SET: {
                // a=obj, b=val, d=nameIdx
                const std::string &fname = chunk.strings[I.d];
                // Auto-create struct if empty
                if (R[I.a].isEmpty()) {
                    R[I.a] = MValue::structure();
                }
                if (!R[I.a].isStruct())
                    throw std::runtime_error("Dot indexing requires a struct");
                R[I.a].field(fname) = R[I.b];
                break;
            }

            // ── Cell operations ──────────────────────────────────
            case OpCode::CELL_LITERAL: {
                uint8_t base = I.b, count = I.c;
                auto cell = MValue::cell(1, count);
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
            case OpCode::CELL_SET: {
                if (R[I.a].isEmpty())
                    R[I.a] = MValue::cell(0, 0);
                if (!R[I.a].isCell())
                    throw std::runtime_error("Cell indexing requires a cell array");
                size_t i = (size_t) R[I.b].toScalar() - 1;
                if (i >= R[I.a].numel()) {
                    size_t ns = i + 1;
                    auto nc = MValue::cell(1, ns);
                    for (size_t k = 0; k < R[I.a].numel(); ++k)
                        nc.cellAt(k) = R[I.a].cellAt(k);
                    R[I.a] = std::move(nc);
                }
                R[I.a].cellAt(i) = R[I.c];
                break;
            }
            case OpCode::CELL_SET_2D: {
                if (!R[I.a].isCell())
                    throw std::runtime_error("Cell indexing requires a cell array");
                size_t r = (size_t) R[I.b].toScalar() - 1, c = (size_t) R[I.c].toScalar() - 1;
                R[I.a].cellAt(R[I.a].dims().sub2ind(r, c)) = R[I.e];
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
                static const char *bn[] = {"abs",   "floor", "ceil",  "round", "fix",   "sqrt",
                                           "exp",   "log",   "log2",  "log10", "sin",   "cos",
                                           "tan",   "sign",  "isnan", "isinf", nullptr, nullptr,
                                           nullptr, nullptr, "mod",   "rem",   "max",   "min",
                                           "pow",   "atan2"};
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

            // ── Multi-return function call ──────────────────────
            case OpCode::CALL_MULTI: {
                // a=outBase, b=argBase, c=nargs, d=funcIdx, e=nout
                uint8_t outBase = I.a, argBase = I.b, na = I.c, nout = I.e;
                int16_t funcIdx = I.d;
                const std::string &funcName = chunk.strings[funcIdx];

                // Try compiled user function
                if (compiledFuncs_) {
                    auto cfIt = compiledFuncs_->find(funcName);
                    if (cfIt != compiledFuncs_->end()) {
                        auto results = callUserFuncMulti(cfIt->second, &R[argBase], na, nout);
                        for (size_t i = 0; i < results.size() && i < nout; ++i)
                            R[outBase + i] = std::move(results[i]);
                        break;
                    }
                }
                // External function with nout
                auto extIt = engine_.externalFuncs_.find(funcName);
                if (extIt != engine_.externalFuncs_.end()) {
                    std::vector<MValue> outBuf(nout);
                    Span<const MValue> as(&R[argBase], na);
                    Span<MValue> os(outBuf.data(), nout);
                    extIt->second(as, nout, os);
                    for (size_t i = 0; i < nout; ++i)
                        R[outBase + i] = std::move(outBuf[i]);
                    break;
                }
                throw std::runtime_error("VM: undefined function '" + funcName + "'");
            }

            // ── Indirect function call (func handle) or array indexing ─
            case OpCode::CALL_INDIRECT: {
                // a=dst, b=fhReg, c=argBase, e=nargs
                uint8_t fhReg = I.b, argBase = I.c, na = I.e;

                // Check if it's a closure cell: {funcHandle, cap1, cap2, ...}
                MValue funcHandleVal;
                const MValue *capturedVals = nullptr;
                size_t numCaptures = 0;

                if (R[fhReg].isCell() && R[fhReg].numel() >= 1
                    && R[fhReg].cellAt(0).isFuncHandle()) {
                    // Closure cell — unpack
                    funcHandleVal = R[fhReg].cellAt(0);
                    numCaptures = R[fhReg].numel() - 1;
                } else if (R[fhReg].isFuncHandle()) {
                    funcHandleVal = R[fhReg];
                } else {
                    // Array indexing fallback
                    goto call_indirect_index;
                }

                {
                    const std::string &funcName = funcHandleVal.funcHandleName();

                    // Build args: user args + captured values
                    // Copy user args + captures into a temp buffer
                    MValue argsBuffer[32];
                    for (uint8_t i = 0; i < na; ++i)
                        argsBuffer[i] = R[argBase + i];
                    for (size_t i = 0; i < numCaptures; ++i)
                        argsBuffer[na + i] = R[fhReg].cellAt(1 + i);
                    uint8_t totalArgs = na + static_cast<uint8_t>(numCaptures);

                    if (compiledFuncs_) {
                        auto cfIt = compiledFuncs_->find(funcName);
                        if (cfIt != compiledFuncs_->end()) {
                            R[I.a] = callUserFunc(cfIt->second, argsBuffer, totalArgs);
                            break;
                        }
                    }
                    auto extIt = engine_.externalFuncs_.find(funcName);
                    if (extIt != engine_.externalFuncs_.end()) {
                        Span<const MValue> as(argsBuffer, na); // only user args for external
                        MValue ob[1];
                        Span<MValue> os(ob, 1);
                        extIt->second(as, 1, os);
                        R[I.a] = std::move(ob[0]);
                        break;
                    }
                    throw std::runtime_error("VM: undefined function in handle '@" + funcName + "'");
                }

            call_indirect_index:
                // Array indexing fallback
                if (na == 1) {
                    if (R[fhReg].isDoubleScalar() && R[argBase].isDoubleScalar()) {
                        R[I.a] = R[fhReg];
                    } else if (R[argBase].isDoubleScalar()) {
                        size_t i = checkedIndex(R[argBase].scalarVal(), R[fhReg].numel());
                        R[I.a] = MValue::scalar(R[fhReg].doubleData()[i], &engine_.allocator_);
                    } else {
                        const MValue &mv = R[fhReg];
                        const MValue &ix = R[argBase];
                        size_t n = ix.numel();
                        auto res = MValue::matrix(1, n, MType::DOUBLE, &engine_.allocator_);
                        double *dst = res.doubleDataMut();
                        const double *src = mv.doubleData(), *id = ix.doubleData();
                        for (size_t k = 0; k < n; ++k)
                            dst[k] = src[checkedIndex(id[k], mv.numel())];
                        R[I.a] = std::move(res);
                    }
                } else if (na == 2) {
                    const MValue &mv = R[fhReg];
                    size_t r = (size_t) R[argBase].toScalar() - 1,
                           c = (size_t) R[argBase + 1].toScalar() - 1;
                    R[I.a] = MValue::scalar(mv.doubleData()[mv.dims().sub2indChecked(r, c)],
                                            &engine_.allocator_);
                } else if (na == 3) {
                    size_t r = (size_t) R[argBase].toScalar() - 1;
                    size_t c = (size_t) R[argBase + 1].toScalar() - 1;
                    size_t p = (size_t) R[argBase + 2].toScalar() - 1;
                    const MValue &mv = R[fhReg];
                    if (mv.isCell()) {
                        R[I.a] = mv.cellAt(mv.dims().sub2indChecked(r, c, p));
                    } else {
                        R[I.a] = MValue::scalar(mv.doubleData()[mv.dims().sub2indChecked(r, c, p)],
                                                &engine_.allocator_);
                    }
                } else {
                    throw std::runtime_error("VM: unsupported CALL_INDIRECT with "
                                             + std::to_string(na) + " args");
                }
                break;
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
            case OpCode::RET_MULTI: {
                // a=base, b=count — return values are in R[base..base+count-1]
                // For multi-return, we store results in a cell so callUserFuncMulti can extract
                uint8_t base = I.a, count = I.b;
                auto cell = MValue::cell(1, count);
                for (uint8_t i = 0; i < count; ++i)
                    cell.cellAt(i) = R[base + i];
                return cell;
            }
            case OpCode::RET_EMPTY:
                return MValue::empty();
            case OpCode::HALT:
                return MValue::empty();
            case OpCode::NOP:
                if (I.a == 1 && !forStack_.empty())
                    forStack_.pop_back(); // break from for-loop
                break;

            // ── Try/catch ────────────────────────────────────────
            case OpCode::TRY_BEGIN: {
                // I.d = offset to catch block, I.a = exception register
                TryHandler th;
                th.catchIp = ip + I.d;
                th.exReg = I.a;
                tryStack_.push_back(th);
                break;
            }
            case OpCode::TRY_END:
                if (!tryStack_.empty())
                    tryStack_.pop_back();
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
    } catch (const std::exception &ex) {
        if (!tryStack_.empty()) {
            TryHandler th = tryStack_.back();
            tryStack_.pop_back();
            MValue err = MValue::structure();
            err.field("message") = MValue::fromString(ex.what(), &engine_.allocator_);
            err.field("identifier") = MValue::fromString("MLAB:error", &engine_.allocator_);
            R[th.exReg] = std::move(err);
            ip = th.catchIp;
            goto dispatch_loop;
        }
        throw;
    }

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

// ============================================================
// Multi-return user function call
// ============================================================

std::vector<MValue> VM::callUserFuncMulti(const BytecodeChunk &funcChunk,
                                          const MValue *args,
                                          uint8_t nargs,
                                          size_t nout)
{
    // Call the function — it returns a cell for multi-return (via RET_MULTI)
    // or a single value (via RET)
    MValue result = callUserFunc(funcChunk, args, nargs);

    if (result.isCell() && result.numel() >= nout) {
        // RET_MULTI packed results in a cell
        std::vector<MValue> results;
        results.reserve(nout);
        for (size_t i = 0; i < nout; ++i)
            results.push_back(result.cellAt(i));
        return results;
    }

    // Single return — wrap in vector
    std::vector<MValue> results;
    results.push_back(std::move(result));
    while (results.size() < nout)
        results.push_back(MValue::empty());
    return results;
}

// ============================================================
// Array helpers
// ============================================================

void VM::executeHorzcat(MValue &dst, const MValue *regs, uint8_t count)
{
    // Determine output dimensions
    size_t totalCols = 0;
    size_t rows = 0;
    bool allScalar = true;
    for (uint8_t i = 0; i < count; ++i) {
        if (regs[i].isEmpty())
            continue;
        if (regs[i].isDoubleScalar()) {
            totalCols++;
            if (!rows)
                rows = 1;
            allScalar = true;
            continue;
        }
        auto &dims = regs[i].dims();
        totalCols += dims.cols();
        if (!rows)
            rows = dims.rows();
        else if (rows != dims.rows() && dims.rows() != 1 && rows != 1)
            throw std::runtime_error("Dimensions of arrays being concatenated are not consistent");
        if (dims.rows() > 1)
            rows = dims.rows();
        allScalar = false;
    }
    if (!rows)
        rows = 1;

    if (rows == 1) {
        // Simple 1D case — row vector
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
        return;
    }

    // 2D case — concatenate columns
    auto result = MValue::matrix(rows, totalCols, MType::DOUBLE, &engine_.allocator_);
    double *d = result.doubleDataMut();
    size_t colOff = 0;
    for (uint8_t i = 0; i < count; ++i) {
        if (regs[i].isEmpty())
            continue;
        if (regs[i].isDoubleScalar()) {
            d[colOff * rows] = regs[i].scalarVal();
            colOff++;
            continue;
        }
        auto &dims = regs[i].dims();
        const double *src = regs[i].doubleData();
        for (size_t c = 0; c < dims.cols(); ++c)
            for (size_t r = 0; r < dims.rows(); ++r)
                d[r + (colOff + c) * rows] = src[r + c * dims.rows()];
        colOff += dims.cols();
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
        // Scalar range — only one iteration, use safe path
        if (varReg.isDoubleScalar())
            varReg.setScalarFast(fs.range.scalarVal());
        else
            varReg.setScalarVal(fs.range.scalarVal());
        return;
    }
    if (fs.rows == 1) {
        // Row vector — most common. After first iteration, varReg is always scalar.
        if (varReg.isDoubleScalar())
            varReg.setScalarFast(fs.data[fs.index]);
        else
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