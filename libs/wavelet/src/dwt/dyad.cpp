// libs/wavelet/src/dwt/dyad.cpp
//
// Dyadic upsample/downsample helpers and wmaxlev.

#include <numkit/wavelet/filter/wfilters.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include <cmath>
#include <string>

namespace numkit::wavelet {

namespace {

void readShape(const Value &x, size_t &rows, size_t &cols)
{
    rows = x.dims().rows();
    cols = x.dims().cols();
    if (rows == 0 && cols == 0) { rows = 1; cols = 0; }
}

// Return shape with dimension `outN` along whichever axis was the
// non-singleton in the input. For column inputs (rows>1, cols==1)
// the output is M×1; otherwise 1×M.
void outShape(size_t inRows, size_t inCols, size_t outN,
              size_t &outRows, size_t &outCols)
{
    if (inCols == 1 && inRows > 1) { outRows = outN; outCols = 1; }
    else                            { outRows = 1;   outCols = outN; }
}

} // anonymous

namespace detail {

// y = dyaddown(x[, ODD])
//   ODD == 0 (default): keep even-indexed (1-based) → x(2:2:end)
//   ODD == 1:           keep odd-indexed           → x(1:2:end)
//
// For a length-N input the output length is:
//   ODD == 0: floor(N/2)
//   ODD == 1: ceil (N/2) = floor((N+1)/2)
void dyaddown_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("dyaddown: requires an input vector",
                    0, 0, "dyaddown", "", "m:dyaddown:nargin");
    const Value &x = args[0];
    int odd = 0;
    if (args.size() >= 2) odd = static_cast<int>(args[1].toScalar());
    odd = (odd != 0) ? 1 : 0;

    size_t rows, cols;
    readShape(x, rows, cols);
    const size_t N = rows * cols;
    const size_t outN = (odd == 0) ? (N / 2) : ((N + 1) / 2);
    size_t outRows, outCols;
    outShape(rows, cols, outN, outRows, outCols);
    Value y = Value::matrix(outRows, outCols, ValueType::DOUBLE, ctx.engine->resource());
    if (outN == 0) { outs[0] = y; return; }
    double *yd = y.doubleDataMut();
    const size_t start = (odd == 0) ? 1 : 0;       // 0-based first kept index
    for (size_t k = 0; k < outN; ++k)
        yd[k] = x.elemAsDouble(start + 2 * k);
    outs[0] = y;
}

// y = dyadup(x[, ODD])
//   ODD == 0:           y = [x(1) 0 x(2) 0 … x(N)]                length 2N-1
//   ODD == 1 (default): y = [0 x(1) 0 x(2) 0 … x(N) 0]            length 2N+1
//
// Verified vs MATLAB R2025b:
//   dyadup([1 2 3], 0) → [1 0 2 0 3]
//   dyadup([1 2 3], 1) → [0 1 0 2 0 3 0]
//   dyadup([1 2 3])    → [0 1 0 2 0 3 0]   (default ODD = 1)
void dyadup_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("dyadup: requires an input vector",
                    0, 0, "dyadup", "", "m:dyadup:nargin");
    const Value &x = args[0];
    int odd = 1;                          // MATLAB default: leading-zero pattern
    if (args.size() >= 2) odd = static_cast<int>(args[1].toScalar());
    odd = (odd != 0) ? 1 : 0;

    size_t rows, cols;
    readShape(x, rows, cols);
    const size_t N = rows * cols;
    const size_t outN = (N == 0) ? 0
                                 : (odd == 0 ? (2 * N - 1) : (2 * N + 1));
    size_t outRows, outCols;
    outShape(rows, cols, outN, outRows, outCols);
    Value y = Value::matrix(outRows, outCols, ValueType::DOUBLE, ctx.engine->resource());
    if (outN == 0) { outs[0] = y; return; }
    double *yd = y.doubleDataMut();
    const size_t start = (odd == 0) ? 0 : 1;       // position of x(1) in y
    for (size_t k = 0; k < outN; ++k) yd[k] = 0.0;
    for (size_t i = 0; i < N; ++i)
        yd[start + 2 * i] = x.elemAsDouble(i);
    outs[0] = y;
}

// L = wmaxlev(N, wname)
//   N can be a scalar or a 2-vector [r c]; in the latter MATLAB uses
//   min(r, c) for a 2-D image. wname identifies the wavelet family
//   (filter length is taken from libs/wavelet/filter/wfilters.cpp).
//
//   L = floor(log2(N / (Lf - 1)))
//
// Verified vs MATLAB R2025b:
//   wmaxlev(64, 'db2')     → 4   (Lf = 4 → log2(64/3) ≈ 4.41 → 4)
//   wmaxlev([8 8], 'db1')  → 3   (Lf = 2 → log2(8/1) = 3)
void wmaxlev_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("wmaxlev: requires (N, wname)",
                    0, 0, "wmaxlev", "", "m:wmaxlev:nargin");
    const Value &Nv = args[0];
    if (!args[1].isChar() && !args[1].isString())
        throw Error("wmaxlev: wname must be a character vector",
                    0, 0, "wmaxlev", "", "m:wmaxlev:type");
    const std::string wname = args[1].toString();

    // Accept scalar N or a 1×k vector; use min(N) as the effective length.
    double Nmin = 0.0;
    const size_t k = Nv.numel();
    if (k == 0)
        throw Error("wmaxlev: N must not be empty",
                    0, 0, "wmaxlev", "", "m:wmaxlev:empty");
    Nmin = Nv.elemAsDouble(0);
    for (size_t i = 1; i < k; ++i) {
        const double v = Nv.elemAsDouble(i);
        if (v < Nmin) Nmin = v;
    }

    auto fb = wavelet_filters(wname);
    const size_t Lf = fb.Lo_D.size();
    if (Lf < 2)
        throw Error("wmaxlev: filter length must be ≥ 2",
                    0, 0, "wmaxlev", "", "m:wmaxlev:filter");

    double L = 0.0;
    if (Nmin >= static_cast<double>(Lf - 1) + 1.0) {
        const double r = Nmin / static_cast<double>(Lf - 1);
        L = std::floor(std::log2(r));
        if (L < 0.0) L = 0.0;
    }
    outs[0] = Value::scalar(L, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::wavelet
