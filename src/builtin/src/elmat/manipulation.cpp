// src/builtin/src/elmat/manipulation.cpp
//
// Array manipulation, reshaping, slicing, and transformations for numkit::builtin.

#include <numkit/builtin/elmat.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/error.hpp>
#include <numkit/lang/arrays/matrix.hpp>
#include <numkit/lang/arrays/manip.hpp>
#include <numkit/lang/arrays/nd_manip.hpp>
#include <numkit/lang/operators/binary_ops.hpp>

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace numkit::builtin {

Value repmat(const Value &x, Span<const size_t> reps, std::pmr::memory_resource *mr)
{
    return numkit::lang::repmatND(x, reps, mr);
}

Value repmat(const Value &x, size_t r, size_t c, std::pmr::memory_resource *mr)
{
    return numkit::lang::repmat(x, r, c, 1, mr);
}

Value repelem(const Value &x, Span<const size_t> reps, std::pmr::memory_resource *mr)
{
    if (reps.size() == 1) {
        return numkit::lang::repelem(x, reps[0], mr);
    } else if (reps.size() == 2) {
        return numkit::lang::repelem(x, reps[0], reps[1], mr);
    }
    throw std::runtime_error("repelem: >2D reps not supported");
}

Value reshape(const Value &x, Span<const size_t> newDims, std::pmr::memory_resource *mr)
{
    return numkit::lang::reshapeND(x, newDims, mr);
}

Value diag(const Value &x, int k, std::pmr::memory_resource *mr)
{
    return numkit::lang::diag(x, k, mr);
}

Value blkdiag(Span<const Value> matrices, std::pmr::memory_resource *mr)
{
    return numkit::lang::blkdiag(matrices, mr);
}

Value cat(int dim, Span<const Value> arrays, std::pmr::memory_resource *mr)
{
    return numkit::lang::cat(dim, arrays, mr);
}

Value horzcat(Span<const Value> arrays, std::pmr::memory_resource *mr)
{
    return numkit::lang::horzcat(arrays, mr);
}

Value vertcat(Span<const Value> arrays, std::pmr::memory_resource *mr)
{
    return numkit::lang::vertcat(arrays, mr);
}

Value rot90(const Value &x, int k, std::pmr::memory_resource *mr)
{
    return numkit::lang::rot90(x, k, mr);
}

Value fliplr(const Value &x, std::pmr::memory_resource *mr)
{
    return numkit::lang::fliplr(x, mr);
}

Value flipud(const Value &x, std::pmr::memory_resource *mr)
{
    return numkit::lang::flipud(x, mr);
}

Value flip(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    return numkit::lang::flip(x, dim, mr);
}

Value circshift(const Value &x, Span<const int> shifts, std::pmr::memory_resource *mr)
{
    if (shifts.size() == 1) {
        return numkit::lang::circshift(x, shifts[0], mr);
    } else if (shifts.size() == 2) {
        return numkit::lang::circshift(x, shifts[0], shifts[1], mr);
    } else {
        std::vector<int64_t> s(shifts.begin(), shifts.end());
        return numkit::lang::circshiftND(x, Span<const int64_t>(s.data(), s.size()), mr);
    }
}

Value permute(const Value &x, Span<const size_t> order, std::pmr::memory_resource *mr)
{
    std::vector<int> p(order.size());
    for (size_t i = 0; i < order.size(); ++i) p[i] = static_cast<int>(order[i]);
    return numkit::lang::permute(x, Span<const int>(p.data(), p.size()), mr);
}

Value ipermute(const Value &x, Span<const size_t> order, std::pmr::memory_resource *mr)
{
    std::vector<int> p(order.size());
    for (size_t i = 0; i < order.size(); ++i) p[i] = static_cast<int>(order[i]);
    return numkit::lang::ipermute(x, Span<const int>(p.data(), p.size()), mr);
}

Value shiftdim(const Value &x, int n, std::pmr::memory_resource *mr)
{
    return numkit::lang::shiftdim(x, n, mr);
}

Value squeeze(const Value &x, std::pmr::memory_resource *mr)
{
    return numkit::lang::squeeze(x, mr);
}

Value head(const Value &x, size_t k, std::pmr::memory_resource *mr)
{
    if (x.isEmpty()) return x;
    if (x.dims().ndim() > 2)
        throw std::runtime_error("head: ND inputs (>2) not supported");
    const size_t R = x.dims().rows(), C = x.dims().cols();
    const size_t n = std::min(R, k);
    auto out = Value::matrix(n, C, x.type(), mr);
    const size_t es = elementSize(x.type());
    const char *src = static_cast<const char *>(x.rawData());
    char *dst = static_cast<char *>(out.rawDataMut());
    for (size_t c = 0; c < C; ++c) {
        std::memcpy(dst + (c * n) * es,
                    src + (c * R) * es,
                    n * es);
    }
    return out;
}

Value tail(const Value &x, size_t k, std::pmr::memory_resource *mr)
{
    if (x.isEmpty()) return x;
    if (x.dims().ndim() > 2)
        throw std::runtime_error("tail: ND inputs (>2) not supported");
    const size_t R = x.dims().rows(), C = x.dims().cols();
    const size_t n = std::min(R, k);
    auto out = Value::matrix(n, C, x.type(), mr);
    const size_t es = elementSize(x.type());
    const char *src = static_cast<const char *>(x.rawData());
    char *dst = static_cast<char *>(out.rawDataMut());
    const size_t rowOff = R - n;
    for (size_t c = 0; c < C; ++c) {
        std::memcpy(dst + (c * n) * es,
                    src + (c * R + rowOff) * es,
                    n * es);
    }
    return out;
}

Value paddata(const Value &x, size_t len, int dim, const std::string &side, std::pmr::memory_resource *mr)
{
    if (x.isEmpty()) return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    size_t rows = x.dims().rows();
    size_t cols = x.dims().cols();
    bool isRowVec = (rows == 1 && cols > 1);
    int targetDim = (dim == 0) ? (isRowVec ? 2 : 1) : dim;

    if (targetDim == 1) {
        if (rows >= len) return x;
        auto out = Value::matrix(len, cols, ValueType::DOUBLE, mr);
        size_t padRows = len - rows;
        size_t startRow = (side == "left") ? padRows : 0;
        for (size_t c = 0; c < cols; ++c) {
            for (size_t r = 0; r < rows; ++r) {
                out.doubleDataMut()[c * len + startRow + r] = x.elemAsDouble(c * rows + r);
            }
        }
        return out;
    } else {
        if (cols >= len) return x;
        auto out = Value::matrix(rows, len, ValueType::DOUBLE, mr);
        size_t padCols = len - cols;
        size_t startCol = (side == "left") ? padCols : 0;
        for (size_t c = 0; c < cols; ++c) {
            for (size_t r = 0; r < rows; ++r) {
                out.doubleDataMut()[(startCol + c) * rows + r] = x.elemAsDouble(c * rows + r);
            }
        }
        return out;
    }
}

Value trimdata(const Value &x, size_t len, int dim, const std::string &side, std::pmr::memory_resource *mr)
{
    if (x.isEmpty()) return x;
    size_t rows = x.dims().rows();
    size_t cols = x.dims().cols();
    bool isRowVec = (rows == 1 && cols > 1);
    int targetDim = (dim == 0) ? (isRowVec ? 2 : 1) : dim;

    if (targetDim == 1) {
        if (rows <= len) return x;
        auto out = Value::matrix(len, cols, ValueType::DOUBLE, mr);
        size_t startRow = (side == "left") ? (rows - len) : 0;
        for (size_t c = 0; c < cols; ++c)
            for (size_t r = 0; r < len; ++r)
                out.doubleDataMut()[c * len + r] = x.elemAsDouble(c * rows + startRow + r);
        return out;
    } else {
        if (cols <= len) return x;
        auto out = Value::matrix(rows, len, ValueType::DOUBLE, mr);
        size_t startCol = (side == "left") ? (cols - len) : 0;
        for (size_t c = 0; c < len; ++c)
            for (size_t r = 0; r < rows; ++r)
                out.doubleDataMut()[c * rows + r] = x.elemAsDouble((startCol + c) * rows + r);
        return out;
    }
}

Value bsxfun(const std::string &op, const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    std::string name = op;
    if (!name.empty() && name[0] == '@') {
        name = name.substr(1);
    }
    if (name == "plus") return numkit::lang::plus(a, b, mr);
    if (name == "minus") return numkit::lang::minus(a, b, mr);
    if (name == "times") return numkit::lang::times(a, b, mr);
    if (name == "rdivide") return numkit::lang::rdivide(a, b, mr);
    if (name == "ldivide") return numkit::lang::mldivide(a, b, mr);
    if (name == "power") return numkit::lang::elementPower(a, b, mr);
    if (name == "eq") return numkit::lang::eq(a, b, mr);
    if (name == "ne") return numkit::lang::ne(a, b, mr);
    if (name == "lt") return numkit::lang::lt(a, b, mr);
    if (name == "le") return numkit::lang::le(a, b, mr);
    if (name == "gt") return numkit::lang::gt(a, b, mr);
    if (name == "ge") return numkit::lang::ge(a, b, mr);
    if (name == "and") return numkit::lang::logicalAnd(a, b, mr);
    if (name == "or") return numkit::lang::logicalOr(a, b, mr);
    if (name == "xor") return numkit::lang::xorOf(a, b, mr);
    throw std::runtime_error("bsxfun: unsupported operation '" + op + "'");
}

Value tril(const Value &x, int k, std::pmr::memory_resource *mr)
{
    return numkit::lang::tril(x, k, mr);
}

Value triu(const Value &x, int k, std::pmr::memory_resource *mr)
{
    return numkit::lang::triu(x, k, mr);
}

} // namespace numkit::builtin
