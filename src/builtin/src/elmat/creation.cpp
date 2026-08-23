// src/builtin/src/elmat/creation.cpp
//
// Array and elementary matrix creation implementations for numkit::builtin.

#include <numkit/builtin/elmat.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/lang/arrays/matrix.hpp>

#include <cmath>
#include <stdexcept>
#include <vector>

namespace numkit::builtin {

Value zeros(Span<const size_t> dims, ValueType dtype, std::pmr::memory_resource *mr)
{
    if (dims.empty()) return Value::matrix(0, 0, dtype, mr);
    if (dims.size() == 1) return Value::matrix(dims[0], dims[0], dtype, mr);
    if (dims.size() == 2) return Value::matrix(dims[0], dims[1], dtype, mr);
    if (dims.size() == 3) return Value::matrix3d(dims[0], dims[1], dims[2], dtype, mr);
    return Value::matrixND(dims.data(), static_cast<int>(dims.size()), dtype, mr);
}

Value zeros(size_t rows, size_t cols, ValueType dtype, std::pmr::memory_resource *mr)
{
    return Value::matrix(rows, cols, dtype, mr);
}

Value ones(Span<const size_t> dims, ValueType dtype, std::pmr::memory_resource *mr)
{
    Value v = zeros(dims, dtype, mr);
    if (v.type() == ValueType::DOUBLE) {
        double *p = v.doubleDataMut();
        for (size_t i = 0; i < v.numel(); ++i) p[i] = 1.0;
    } else if (v.type() == ValueType::LOGICAL) {
        uint8_t *p = v.logicalDataMut();
        for (size_t i = 0; i < v.numel(); ++i) p[i] = 1;
    } else if (v.type() == ValueType::INT32) {
        int32_t *p = v.int32DataMut();
        for (size_t i = 0; i < v.numel(); ++i) p[i] = 1;
    } else if (v.type() == ValueType::INT64) {
        int64_t *p = v.int64DataMut();
        for (size_t i = 0; i < v.numel(); ++i) p[i] = 1;
    }
    return v;
}

Value ones(size_t rows, size_t cols, ValueType dtype, std::pmr::memory_resource *mr)
{
    Value v = Value::matrix(rows, cols, dtype, mr);
    if (v.type() == ValueType::DOUBLE) {
        double *p = v.doubleDataMut();
        for (size_t i = 0; i < v.numel(); ++i) p[i] = 1.0;
    } else if (v.type() == ValueType::LOGICAL) {
        uint8_t *p = v.logicalDataMut();
        for (size_t i = 0; i < v.numel(); ++i) p[i] = 1;
    } else if (v.type() == ValueType::INT32) {
        int32_t *p = v.int32DataMut();
        for (size_t i = 0; i < v.numel(); ++i) p[i] = 1;
    } else if (v.type() == ValueType::INT64) {
        int64_t *p = v.int64DataMut();
        for (size_t i = 0; i < v.numel(); ++i) p[i] = 1;
    }
    return v;
}

Value eye(size_t n, ValueType /*dtype*/, std::pmr::memory_resource *mr)
{
    return numkit::lang::eye(n, n, mr);
}

Value eye(size_t m, size_t n, ValueType /*dtype*/, std::pmr::memory_resource *mr)
{
    return numkit::lang::eye(m, n, mr);
}

Value linspace(double a, double b, size_t n, std::pmr::memory_resource *mr)
{
    if (n == 0) return Value::matrix(1, 0, ValueType::DOUBLE, mr);
    if (n == 1) {
        Value res = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        res.doubleDataMut()[0] = b;
        return res;
    }
    Value res = Value::matrix(1, n, ValueType::DOUBLE, mr);
    double *data = res.doubleDataMut();
    double step = (b - a) / static_cast<double>(n - 1);
    for (size_t i = 0; i < n - 1; ++i) {
        data[i] = a + static_cast<double>(i) * step;
    }
    data[n - 1] = b;
    return res;
}

Value logspace(double a, double b, size_t n, std::pmr::memory_resource *mr)
{
    constexpr double kPi = 3.14159265358979323846;
    double endVal = (std::fabs(b - kPi) < 1e-14) ? kPi : std::pow(10.0, b);
    if (n == 0) return Value::matrix(1, 0, ValueType::DOUBLE, mr);
    if (n == 1) {
        Value res = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        res.doubleDataMut()[0] = endVal;
        return res;
    }
    Value res = Value::matrix(1, n, ValueType::DOUBLE, mr);
    double *data = res.doubleDataMut();
    double step = (b - a) / static_cast<double>(n - 1);
    for (size_t i = 0; i < n - 1; ++i) {
        data[i] = std::pow(10.0, a + static_cast<double>(i) * step);
    }
    data[n - 1] = endVal;
    return res;
}

Value magic(size_t n, std::pmr::memory_resource *mr)
{
    return numkit::lang::magic(n, mr);
}

Value hilb(size_t n, std::pmr::memory_resource *mr)
{
    return numkit::lang::hilb(n, mr);
}

Value invhilb(size_t n, std::pmr::memory_resource *mr)
{
    return numkit::lang::invhilb(n, mr);
}

Value pascal(size_t n, int k, std::pmr::memory_resource *mr)
{
    if (n == 0) return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (k == 0) return numkit::lang::pascal(n, mr);
    auto M = Value::matrix(n, n, ValueType::DOUBLE, mr);
    if (k == 1) {
        for (size_t i = 0; i < n; ++i) {
            M.elem(i, 0) = 1.0;
            for (size_t j = 1; j <= i; ++j) {
                M.elem(i, j) = M.elem(i - 1, j) + M.elem(i - 1, j - 1);
            }
        }
        return M;
    }
    if (k == 2) {
        for (size_t i = 0; i < n; ++i) {
            M.elem(0, i) = (i % 2 == 0 ? 1.0 : -1.0);
        }
        for (size_t i = 1; i < n; ++i) {
            for (size_t j = i; j < n; ++j) {
                M.elem(i, j) = -(M.elem(i - 1, j) + M.elem(i, j - 1));
            }
        }
        return M;
    }
    throw std::runtime_error("pascal: k must be 0, 1, or 2");
}

Value toeplitz(const Value &c, const Value &r, std::pmr::memory_resource *mr)
{
    return numkit::lang::toeplitz(c, r, mr);
}

Value vander(const Value &v, std::pmr::memory_resource *mr)
{
    return numkit::lang::vander(v, mr);
}

Value wilkinson(size_t n, std::pmr::memory_resource *mr)
{
    return numkit::lang::wilkinson(n, mr);
}

Value rosser(std::pmr::memory_resource *mr)
{
    return numkit::lang::rosser(mr);
}

Value hadamard(size_t n, std::pmr::memory_resource *mr)
{
    return numkit::lang::hadamard(n, mr);
}

Value hankel(const Value &c, const Value &r, std::pmr::memory_resource *mr)
{
    return numkit::lang::hankel(c, r, mr);
}

Value compan(const Value &p, std::pmr::memory_resource *mr)
{
    return numkit::lang::compan(p, mr);
}

Meshgrid2D meshgrid(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    auto [X, Y] = numkit::lang::meshgrid(x, y, mr);
    return { std::move(X), std::move(Y) };
}

Meshgrid2D meshgrid(const Value &x, std::pmr::memory_resource *mr)
{
    auto [X, Y] = numkit::lang::meshgrid(x, x, mr);
    return { std::move(X), std::move(Y) };
}

Meshgrid3D meshgrid(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr)
{
    auto [X, Y, Z] = numkit::lang::meshgrid(x, y, z, mr);
    return { std::move(X), std::move(Y), std::move(Z) };
}

Meshgrid2D ndgrid(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    auto [X, Y] = numkit::lang::ndgrid(x, y, mr);
    return { std::move(X), std::move(Y) };
}

Meshgrid3D ndgrid(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr)
{
    auto [X, Y, Z] = numkit::lang::ndgrid(x, y, z, mr);
    return { std::move(X), std::move(Y), std::move(Z) };
}

} // namespace numkit::builtin
