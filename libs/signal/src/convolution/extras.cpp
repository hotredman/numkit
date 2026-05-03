// libs/signal/src/convolution/extras.cpp
//
// cconv / convmtx / xcorr2 / finddelay / alignsignals.

#include <numkit/signal/convolution/extras.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>
#include <numkit/signal/convolution/convolution.hpp>     // xcorr

#include <algorithm>
#include <cmath>
#include <cstring>

namespace numkit::signal {

namespace {

double readReal(const Value &v, size_t i)
{
    return v.elemAsDouble(i);
}

} // namespace

// ── cconv ─────────────────────────────────────────────────────────────
// MATLAB default for cconv(x, y) (no N): N = length(x) + length(y) - 1
// — the linear-convolution length. The 3-arg form cconv(x, y, n) does
// true circular convolution with period n. See BUGS.md #33.
Value cconv(std::pmr::memory_resource *mr, const Value &x, const Value &y, size_t n)
{
    const size_t nx = x.numel(), ny = y.numel();
    if (n == 0) {
        if (nx == 0 || ny == 0) n = 0;
        else                    n = nx + ny - 1;
    }
    auto out = Value::matrix(1, n, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    std::fill(dst, dst + n, 0.0);
    for (size_t k = 0; k < n; ++k) {
        double s = 0.0;
        for (size_t i = 0; i < n; ++i) {
            const double xi = (i < nx) ? readReal(x, i) : 0.0;
            const size_t j = (k + n - i) % n;     // (k - i) mod n
            const double yj = (j < ny) ? readReal(y, j) : 0.0;
            s += xi * yj;
        }
        dst[k] = s;
    }
    return out;
}

// ── convmtx ───────────────────────────────────────────────────────────
// MATLAB shape rules (from `help convmtx`):
//   * h is a row    → returns n × (n+nh-1) matrix; row k is h shifted
//     right by k (so X*h_col == conv(x, h)).
//   * h is a column → returns (n+nh-1) × n matrix; column k is h
//     shifted down by k.
// See BUGS.md #34.
Value convmtx(std::pmr::memory_resource *mr, const Value &h, size_t n)
{
    if (n == 0)
        throw Error("convmtx: n must be positive",
                     0, 0, "convmtx", "", "m:convmtx:badN");
    const size_t nh = h.numel();
    if (nh == 0)
        throw Error("convmtx: h must be non-empty",
                     0, 0, "convmtx", "", "m:convmtx:emptyH");

    // Default to row-form when h is a vector with rows() == 1 OR is
    // 1-D / scalar. Column form only when h is explicitly Nx1 (rows>1).
    const bool hIsColumn = (h.dims().rows() > 1 && h.dims().cols() == 1);

    if (hIsColumn) {
        const size_t rows = n + nh - 1;
        auto out = Value::matrix(rows, n, ValueType::DOUBLE, mr);
        double *dst = out.doubleDataMut();
        std::fill(dst, dst + rows * n, 0.0);
        for (size_t col = 0; col < n; ++col) {
            for (size_t i = 0; i < nh; ++i) {
                // Column-major: index = row + col * rows.
                dst[(col + i) + col * rows] = readReal(h, i);
            }
        }
        return out;
    }

    // Row form: out(row, row+i) = h(i), for row in [0, n) and i in [0, nh).
    // Output is n × (n + nh - 1).
    const size_t cols = n + nh - 1;
    auto out = Value::matrix(n, cols, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    std::fill(dst, dst + n * cols, 0.0);
    for (size_t row = 0; row < n; ++row) {
        for (size_t i = 0; i < nh; ++i) {
            // Column-major: index = row + (row+i) * n.
            dst[row + (row + i) * n] = readReal(h, i);
        }
    }
    return out;
}

// ── xcorr2 ────────────────────────────────────────────────────────────
// Direct O(rA*cA*rB*cB). Output shape (rA+rB-1) × (cA+cB-1).
Value xcorr2(std::pmr::memory_resource *mr, const Value &A, const Value &B)
{
    const size_t rA = A.dims().rows(), cA = A.dims().cols();
    const size_t rB = B.dims().rows(), cB = B.dims().cols();
    const size_t rOut = rA + rB - 1;
    const size_t cOut = cA + cB - 1;
    auto out = Value::matrix(rOut, cOut, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    std::fill(dst, dst + rOut * cOut, 0.0);

    // C[k+rB-1, l+cB-1] = sum_{i,j} A[i+k, j+l] * B[i, j]
    // for k in [-(rB-1) .. rA-1], l in [-(cB-1) .. cA-1].
    for (size_t i = 0; i < rA; ++i) {
        for (size_t j = 0; j < cA; ++j) {
            const double a = A(i, j);
            if (a == 0.0) continue;
            for (size_t p = 0; p < rB; ++p) {
                const long kRow = static_cast<long>(i) - static_cast<long>(p) + static_cast<long>(rB) - 1;
                if (kRow < 0 || kRow >= static_cast<long>(rOut)) continue;
                for (size_t q = 0; q < cB; ++q) {
                    const long kCol = static_cast<long>(j) - static_cast<long>(q) + static_cast<long>(cB) - 1;
                    if (kCol < 0 || kCol >= static_cast<long>(cOut)) continue;
                    dst[kRow + kCol * rOut] += a * B(p, q);
                }
            }
        }
    }
    return out;
}

// ── finddelay ─────────────────────────────────────────────────────────
long finddelay(std::pmr::memory_resource *mr, const Value &x, const Value &y, long max_lag)
{
    auto [c, lags] = xcorr(mr, x, y);
    const size_t n = c.numel();
    const double *cp = c.doubleData();
    const double *lp = lags.doubleData();
    long best_lag = 0;
    double best_mag = -1.0;
    for (size_t i = 0; i < n; ++i) {
        const long lag = static_cast<long>(lp[i]);
        if (max_lag > 0 && std::abs(lag) > max_lag) continue;
        const double mag = std::abs(cp[i]);
        if (mag > best_mag) {
            best_mag = mag;
            best_lag = lag;
        }
    }
    // MATLAB convention: positive lag means y is delayed relative to x
    //   → finddelay returns -lag_of_xcorr_peak.
    return -best_lag;
}

// ── alignsignals ──────────────────────────────────────────────────────
std::tuple<Value, Value>
alignsignals(std::pmr::memory_resource *mr, const Value &x, const Value &y, long max_lag)
{
    const long d = finddelay(mr, x, y, max_lag);
    const size_t nx = x.numel(), ny = y.numel();

    // Lead-pad whichever signal is "leading" so peaks line up:
    //   d > 0: y is delayed by d → prepend d zeros to x.
    //   d < 0: x is delayed by |d| → prepend |d| zeros to y.
    const size_t pad_x = (d > 0) ? static_cast<size_t>(d) : 0;
    const size_t pad_y = (d < 0) ? static_cast<size_t>(-d) : 0;
    // Then trail-pad whichever ends short so both outputs are same length.
    const size_t finalLen = std::max(nx + pad_x, ny + pad_y);

    auto orient = [&](const Value &src, size_t lead_pad) {
        const bool isRow = (src.dims().rows() == 1);
        auto out = isRow
                    ? Value::matrix(1, finalLen, ValueType::DOUBLE, mr)
                    : Value::matrix(finalLen, 1, ValueType::DOUBLE, mr);
        double *dst = out.doubleDataMut();
        std::fill(dst, dst + finalLen, 0.0);
        const size_t n = src.numel();
        for (size_t i = 0; i < n; ++i)
            dst[lead_pad + i] = src.elemAsDouble(i);
        return out;
    };
    return std::make_tuple(orient(x, pad_x), orient(y, pad_y));
}

namespace detail {

void cconv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("cconv: requires (x, y[, n])",
                     0, 0, "cconv", "", "m:cconv:nargin");
    const size_t n = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 0;
    outs[0] = cconv(ctx.engine->resource(), args[0], args[1], n);
}

void convmtx_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("convmtx: requires (h, n)",
                     0, 0, "convmtx", "", "m:convmtx:nargin");
    outs[0] = convmtx(ctx.engine->resource(), args[0],
                      static_cast<size_t>(args[1].toScalar()));
}

void xcorr2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("xcorr2: requires at least 1 argument",
                     0, 0, "xcorr2", "", "m:xcorr2:nargin");
    const Value &A = args[0];
    const Value &B = (args.size() >= 2) ? args[1] : args[0];
    outs[0] = xcorr2(ctx.engine->resource(), A, B);
}

void finddelay_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("finddelay: requires (x, y[, max_lag])",
                     0, 0, "finddelay", "", "m:finddelay:nargin");
    const long max_lag = (args.size() >= 3) ? static_cast<long>(args[2].toScalar()) : 0;
    const long d = finddelay(ctx.engine->resource(), args[0], args[1], max_lag);
    outs[0] = Value::scalar(static_cast<double>(d), ctx.engine->resource());
}

void alignsignals_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("alignsignals: requires (x, y[, max_lag])",
                     0, 0, "alignsignals", "", "m:alignsignals:nargin");
    const long max_lag = (args.size() >= 3) ? static_cast<long>(args[2].toScalar()) : 0;
    auto [xa, ya] = alignsignals(ctx.engine->resource(), args[0], args[1], max_lag);
    outs[0] = std::move(xa);
    if (nargout > 1) outs[1] = std::move(ya);
}

} // namespace detail

} // namespace numkit::signal
