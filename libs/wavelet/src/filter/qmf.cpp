// libs/wavelet/src/filter/qmf.cpp
//
// Wavelet filter helpers: qmf, wrev.

#include <numkit/wavelet/filter/qmf.hpp>

// Compute-only TU: Value substrate + Error, no engine. The wrev / qmf
// builtins (CallContext wrappers) live in filter/qmf_reg.cpp.
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

namespace numkit::wavelet {

namespace {

// Read shape (rows, cols). For non-2D arrays we treat them as flat row.
void readShape(const Value &x, size_t &rows, size_t &cols)
{
    rows = x.dims().rows();
    cols = x.dims().cols();
    if (rows == 0 && cols == 0) { rows = 1; cols = 0; }
}

Value sameShape(std::pmr::memory_resource *mr,
                size_t rows, size_t cols)
{
    return Value::matrix(rows, cols, ValueType::DOUBLE, mr);
}

} // anonymous

// ── Public C++ API (see filter/qmf.hpp) ───────────────────────────────

// wrev: reverse along the first non-singleton dimension. Row/col vector ->
// element reverse; matrix (M×N) -> per-column reverse (= flipud). Complex
// preserved. (2026-05-08 fix: per-column, not flat column-major; keep the
// imaginary part on complex input.)
Value wrev(const Value &x, std::pmr::memory_resource *mr)
{
    size_t rows, cols;
    readShape(x, rows, cols);
    const size_t N = rows * cols;
    const bool cplx = x.isComplex();
    Value y = cplx ? Value::complexMatrix(rows, cols, mr)
                   : Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (N == 0) return y;

    const bool isRowVec = (rows == 1);
    if (isRowVec) {
        if (cplx) {
            const Complex *src = x.complexData();
            Complex *dst = y.complexDataMut();
            for (size_t i = 0; i < N; ++i) dst[i] = src[N - 1 - i];
        } else {
            const double *src = x.doubleData();
            double *dst = y.doubleDataMut();
            for (size_t i = 0; i < N; ++i) dst[i] = src[N - 1 - i];
        }
    } else {
        // Column-major: column c starts at offset c*rows; reverse each.
        if (cplx) {
            const Complex *src = x.complexData();
            Complex *dst = y.complexDataMut();
            for (size_t c = 0; c < cols; ++c) {
                const size_t base = c * rows;
                for (size_t r = 0; r < rows; ++r)
                    dst[base + r] = src[base + (rows - 1 - r)];
            }
        } else {
            const double *src = x.doubleData();
            double *dst = y.doubleDataMut();
            for (size_t c = 0; c < cols; ++c) {
                const size_t base = c * rows;
                for (size_t r = 0; r < rows; ++r)
                    dst[base + r] = src[base + (rows - 1 - r)];
            }
        }
    }
    return y;
}

// qmf: quadrature mirror filter. y(k) = (-1)^(k-1) * x(N-k+1) for p == 0
// (default); the whole result is negated for p == 1.
//   qmf([1 2 3 4]) -> [4 -3 2 -1]; qmf([1 2 3 4], 1) -> [-4 3 -2 1].
Value qmf(const Value &x, int p, std::pmr::memory_resource *mr)
{
    p = ((p % 2) + 2) % 2;          // collapse to {0,1}
    size_t rows, cols;
    readShape(x, rows, cols);
    const size_t N = rows * cols;
    Value y = sameShape(mr, rows, cols);
    if (N == 0) return y;
    double *yd = y.doubleDataMut();
    for (size_t k = 0; k < N; ++k) {
        const double v = x.elemAsDouble(N - 1 - k);
        const bool neg = (k % 2 == 1);
        const double s = (neg ^ (p == 1)) ? -v : v;
        yd[k] = s;
    }
    return y;
}

} // namespace numkit::wavelet
