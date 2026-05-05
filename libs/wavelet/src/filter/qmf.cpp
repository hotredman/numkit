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

// y = wrev(x): reverse a vector (or, for matrices, every column read in
// column-major order then written back element-wise reversed).  MATLAB
// preserves the input shape; element k of the output equals element
// N-k+1 of the input under linear (1-based) indexing.
void wrev_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("wrev: requires one input vector",
                    0, 0, "wrev", "", "m:wrev:nargin");
    const Value &x = args[0];
    size_t rows, cols;
    readShape(x, rows, cols);
    const size_t N = rows * cols;
    Value y = sameShape(ctx.engine->resource(), rows, cols);
    if (N == 0) { outs[0] = y; return; }
    double *yd = y.doubleDataMut();
    for (size_t i = 0; i < N; ++i)
        yd[i] = x.elemAsDouble(N - 1 - i);
    outs[0] = y;
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
                    0, 0, "qmf", "", "m:qmf:nargin");
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
