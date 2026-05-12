// libs/signal/src/convolution/extras.cpp
//
// cconv / convmtx / xcorr2 / finddelay / alignsignals.

#include <numkit/signal/convolution/extras.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>
#include <numkit/signal/convolution/convolution.hpp>     // xcorr
#include <numkit/signal/transforms/fft.hpp>              // fft / ifft

#include <algorithm>
#include <cmath>
#include <complex>
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
Value cconv(const Value &x, const Value &y, size_t n, std::pmr::memory_resource *mr)
{
    const size_t nx = x.numel(), ny = y.numel();
    if (n == 0) {
        if (nx == 0 || ny == 0) n = 0;
        else                    n = nx + ny - 1;
    }
    auto out = Value::matrix(1, n, ValueType::DOUBLE, mr);
    if (n == 0) return out;
    double *dst = out.doubleDataMut();
    std::fill(dst, dst + n, 0.0);

    // Fast path: when n ≥ nx + ny - 1 (the default form), circular
    // conv of period n equals linear conv truncated/padded to n. We
    // pad x and y to length nextPow2(n), FFT both, multiply, IFFT,
    // and copy the first n samples. Closes the ~30× perf gap to
    // FFTW-backed Octave / MATLAB.
    if (nx > 0 && ny > 0 && n >= nx + ny - 1) {
        // Promote both to length-n column vectors so fft picks up the
        // axis. Pad with zeros past nx / ny.
        auto xp = Value::matrix(n, 1, ValueType::DOUBLE, mr);
        auto yp = Value::matrix(n, 1, ValueType::DOUBLE, mr);
        double *xd = xp.doubleDataMut();
        double *yd = yp.doubleDataMut();
        std::fill(xd, xd + n, 0.0);
        std::fill(yd, yd + n, 0.0);
        for (size_t i = 0; i < nx; ++i) xd[i] = readReal(x, i);
        for (size_t i = 0; i < ny; ++i) yd[i] = readReal(y, i);
        // numkit's fft pads non-pow2 to nextPow2 internally, so for
        // arbitrary n this would corrupt the spectrum. We only enter
        // this branch when we don't need true length-n circular conv;
        // padding to any size ≥ nx+ny-1 yields the same first-n
        // samples of the linear conv. Force length-nextPow2 explicitly
        // for predictable behaviour.
        const size_t fftLen = [&]() {
            size_t r = 1; while (r < n) r <<= 1; return r;
        }();
        Value X = fft(xp, static_cast<int>(fftLen), /*dim=*/0, mr);
        Value Y = fft(yp, static_cast<int>(fftLen), /*dim=*/0, mr);
        // Pointwise multiply.
        auto Z = Value::complexMatrix(fftLen, 1, mr);
        const std::complex<double> *Xc = X.complexData();
        const std::complex<double> *Yc = Y.complexData();
        std::complex<double> *Zc = Z.complexDataMut();
        for (size_t k = 0; k < fftLen; ++k) Zc[k] = Xc[k] * Yc[k];
        Value z = ifft(Z, /*n=*/-1, /*dim=*/0, mr);
        // ifft can return REAL when the spectrum is conjugate-symmetric.
        if (z.type() == ValueType::COMPLEX) {
            const std::complex<double> *zd = z.complexData();
            for (size_t k = 0; k < n; ++k) dst[k] = zd[k].real();
        } else {
            const double *zd = z.doubleData();
            for (size_t k = 0; k < n; ++k) dst[k] = zd[k];
        }
        return out;
    }

    // Slow path: n < nx + ny - 1 means actual aliased circular conv;
    // use the direct O(n²) loop.
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
Value convmtx(const Value &h, size_t n, std::pmr::memory_resource *mr)
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
Value xcorr2(const Value &A, const Value &B, std::pmr::memory_resource *mr)
{
    const size_t rA = A.dims().rows(), cA = A.dims().cols();
    const size_t rB = B.dims().rows(), cB = B.dims().cols();
    const size_t rOut = rA + rB - 1;
    const size_t cOut = cA + cB - 1;
    auto out = Value::matrix(rOut, cOut, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    if (rOut == 0 || cOut == 0) return out;
    std::fill(dst, dst + rOut * cOut, 0.0);

    // FFT-based fast path: cross-correlation == conv2(A, flip(B)). Pad
    // A (zero) and the flipped B into rOut × cOut grids, fft2 both,
    // multiply pointwise, ifft2, take real part. Threshold below the
    // FFT path's startup cost — for tiny problems direct is faster.
    const size_t cellsOut = rOut * cOut;
    const size_t cellsDirect = rA * cA * rB * cB;
    const bool useFft = cellsDirect > 8192 && cellsOut >= 64;

    if (useFft) {
        auto Ap = Value::matrix(rOut, cOut, ValueType::DOUBLE, mr);
        auto Bf = Value::matrix(rOut, cOut, ValueType::DOUBLE, mr);
        double *ad = Ap.doubleDataMut();
        double *bd = Bf.doubleDataMut();
        std::fill(ad, ad + cellsOut, 0.0);
        std::fill(bd, bd + cellsOut, 0.0);
        // Place A in the top-left.
        for (size_t j = 0; j < cA; ++j)
            for (size_t i = 0; i < rA; ++i)
                ad[i + j * rOut] = A(i, j);
        // Place flipped B (both axes) in the top-left.
        for (size_t j = 0; j < cB; ++j)
            for (size_t i = 0; i < rB; ++i)
                bd[i + j * rOut] = B(rB - 1 - i, cB - 1 - j);

        Value FA = fft2(Ap, -1, -1, mr);
        Value FB = fft2(Bf, -1, -1, mr);
        const Complex *fa = FA.complexData();
        const Complex *fb = FB.complexData();
        auto Z = Value::complexMatrix(rOut, cOut, mr);
        Complex *zd = Z.complexDataMut();
        for (size_t k = 0; k < cellsOut; ++k) zd[k] = fa[k] * fb[k];
        Value z = ifft2(Z, -1, -1, mr);
        if (z.type() == ValueType::COMPLEX) {
            const Complex *zr = z.complexData();
            for (size_t k = 0; k < cellsOut; ++k) dst[k] = zr[k].real();
        } else {
            std::memcpy(dst, z.doubleData(), cellsOut * sizeof(double));
        }
        return out;
    }

    // Direct path for small inputs.
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
long finddelay(const Value &x, const Value &y, long max_lag, std::pmr::memory_resource *mr)
{
    auto [c, lags] = xcorr(x, y, mr);
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
alignsignals(const Value &x, const Value &y, long max_lag, std::pmr::memory_resource *mr)
{
    const long d = finddelay(x, y, max_lag, mr);
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
    outs[0] = cconv(args[0], args[1], n, ctx.engine->resource());
}

void convmtx_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("convmtx: requires (h, n)",
                     0, 0, "convmtx", "", "m:convmtx:nargin");
    outs[0] = convmtx(args[0], static_cast<size_t>(args[1].toScalar()), ctx.engine->resource());
}

void xcorr2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("xcorr2: requires at least 1 argument",
                     0, 0, "xcorr2", "", "m:xcorr2:nargin");
    const Value &A = args[0];
    const Value &B = (args.size() >= 2) ? args[1] : args[0];
    outs[0] = xcorr2(A, B, ctx.engine->resource());
}

void finddelay_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("finddelay: requires (x, y[, max_lag])",
                     0, 0, "finddelay", "", "m:finddelay:nargin");
    const long max_lag = (args.size() >= 3) ? static_cast<long>(args[2].toScalar()) : 0;
    const long d = finddelay(args[0], args[1], max_lag, ctx.engine->resource());
    outs[0] = Value::scalar(static_cast<double>(d), ctx.engine->resource());
}

void alignsignals_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("alignsignals: requires (x, y[, max_lag])",
                     0, 0, "alignsignals", "", "m:alignsignals:nargin");
    const long max_lag = (args.size() >= 3) ? static_cast<long>(args[2].toScalar()) : 0;
    auto [xa, ya] = alignsignals(args[0], args[1], max_lag, ctx.engine->resource());
    outs[0] = std::move(xa);
    if (nargout > 1) outs[1] = std::move(ya);
}

} // namespace detail

} // namespace numkit::signal
