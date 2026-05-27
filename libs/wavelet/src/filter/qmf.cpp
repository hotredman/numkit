// libs/wavelet/src/filter/qmf.cpp
//
// Wavelet filter helpers: qmf, wrev.

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

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

namespace detail {

// y = wrev(x): reverse along the first non-singleton dimension. MATLAB
// behaviour:
//   row vector    -> reverse element order (= flip).
//   col vector    -> reverse element order.
//   matrix (M×N)  -> reverse each column independently (= flipud).
//   complex input -> preserve complex type.
//
// Bug fix 2026-05-08: previous impl treated the input as a flat
// numel-element vector and reversed in column-major order. For matrices
// that gave a full reversal (rows AND cols flipped) instead of MATLAB's
// per-column reverse. Also dropped imaginary parts on complex input
// (used elemAsDouble + doubleDataMut).
void wrev_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("wrev: requires one input vector",
                    0, 0, "wrev", "", "numkit:wrev:nargin");
    const Value &x = args[0];
    auto *mr = ctx.engine->resource();
    size_t rows, cols;
    readShape(x, rows, cols);
    const size_t N = rows * cols;
    const bool cplx = x.isComplex();
    Value y = cplx ? Value::complexMatrix(rows, cols, mr)
                   : Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (N == 0) { outs[0] = y; return; }

    // Treat row vectors as 1-D reverse along cols; everything else is
    // per-column reverse (= flipud).
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
    outs[0] = std::move(y);
}

// y = qmf(x[, p]): quadrature mirror filter.
//   y(k) = (-1)^(k-1)        · x(N-k+1)   if p == 0 (default)
//   y(k) = (-1)^k = -(-1)^(k-1) · x(N-k+1)   if p == 1
//
// Verified vs MATLAB R2025b:
//   qmf([1 2 3 4])    → [4 -3 2 -1]
//   qmf([1 2 3 4], 1) → [-4 3 -2 1]
void qmf_reg(Span<const Value> args, size_t /*nargout*/,
             Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("qmf: requires one input vector",
                    0, 0, "qmf", "", "numkit:qmf:nargin");
    const Value &x = args[0];
    int p = 0;
    if (args.size() >= 2) p = static_cast<int>(args[1].toScalar());
    p = ((p % 2) + 2) % 2;          // collapse to {0,1}

    size_t rows, cols;
    readShape(x, rows, cols);
    const size_t N = rows * cols;
    Value y = sameShape(ctx.engine->resource(), rows, cols);
    if (N == 0) { outs[0] = y; return; }
    double *yd = y.doubleDataMut();
    for (size_t k = 0; k < N; ++k) {
        const double v = x.elemAsDouble(N - 1 - k);
        const bool neg = (k % 2 == 1);
        const double s = (neg ^ (p == 1)) ? -v : v;
        yd[k] = s;
    }
    outs[0] = y;
}

} // namespace detail
} // namespace numkit::wavelet
