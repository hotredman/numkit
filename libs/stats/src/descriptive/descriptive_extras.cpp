// libs/stats/src/descriptive/descriptive_extras.cpp
//
// Descriptive stats extras (B2): bounds, iqr, maxk, mink, rmse.

#include <numkit/stats/descriptive/descriptive.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"
#include "reduction_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace numkit::stats {

namespace {

using ::numkit::builtin::detail::applyAlongDim;
using ::numkit::builtin::detail::firstNonSingletonDim;
using ::numkit::builtin::detail::validateDim;
using ::numkit::builtin::detail::applyAlongDimWithIndex;

int resolveDim(const Value &x, int dim, const char *fn)
{
    if (dim == 0) return firstNonSingletonDim(x);
    return validateDim(x, dim, fn);
}

double sliceMin(const double *s, size_t n)
{
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    double m = s[0];
    for (size_t i = 1; i < n; ++i)
        if (s[i] < m || std::isnan(m)) m = s[i];
    return m;
}

double sliceMax(const double *s, size_t n)
{
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    double m = s[0];
    for (size_t i = 1; i < n; ++i)
        if (s[i] > m || std::isnan(m)) m = s[i];
    return m;
}

// Linear-interpolation quantile of a slice (MATLAB default, type 7).
double sliceQuantile(double *s, size_t n, double p)
{
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    std::sort(s, s + n);
    if (n == 1) return s[0];
    const double h = p * (static_cast<double>(n) - 1.0);
    const size_t lo = static_cast<size_t>(std::floor(h));
    const size_t hi = std::min(lo + 1, n - 1);
    const double frac = h - static_cast<double>(lo);
    return s[lo] + frac * (s[hi] - s[lo]);
}

} // namespace

// ── bounds ────────────────────────────────────────────────────────────
std::tuple<Value, Value>
bounds(std::pmr::memory_resource *mr, const Value &x, int dim)
{
    const int d = resolveDim(x, dim, "bounds");
    auto lo = applyAlongDim(x, d,
        [](size_t, const double *s, size_t n) { return sliceMin(s, n); }, mr);
    auto hi = applyAlongDim(x, d,
        [](size_t, const double *s, size_t n) { return sliceMax(s, n); }, mr);
    return std::make_tuple(std::move(lo), std::move(hi));
}

// ── iqr ───────────────────────────────────────────────────────────────
Value iqr(std::pmr::memory_resource *mr, const Value &x, int dim)
{
    const int d = resolveDim(x, dim, "iqr");
    return applyAlongDim(x, d,
        [](size_t, const double *s, size_t n) -> double {
            if (n == 0) return std::numeric_limits<double>::quiet_NaN();
            std::vector<double> buf(s, s + n);
            const double q3 = sliceQuantile(buf.data(), n, 0.75);
            std::vector<double> buf2(s, s + n);
            const double q1 = sliceQuantile(buf2.data(), n, 0.25);
            return q3 - q1;
        }, mr);
}

namespace {

// k-largest along a generic dim. Output keeps the input shape but with
// the chosen dim shrunk to k. We allocate via createMatrix on the
// 2D / 3D fast path, and fall back to matrixND otherwise.
Value topKAlongDim(std::pmr::memory_resource *mr, const Value &x, int dim,
                   int kReq, bool ascending, const char *fn)
{
    if (kReq < 0)
        throw Error(std::string(fn) + ": k must be non-negative",
                     0, 0, fn, "", std::string("m:") + fn + ":badK");
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // Vector / scalar — single-slice fast path.
    if (x.dims().isVector() || x.isScalar()) {
        const size_t n = x.numel();
        const size_t k = std::min<size_t>(static_cast<size_t>(kReq), n);
        std::vector<double> buf(n);
        for (size_t i = 0; i < n; ++i) buf[i] = x.elemAsDouble(i);
        std::sort(buf.begin(), buf.end(),
                  [ascending](double a, double b) {
                      if (std::isnan(a)) return false;
                      if (std::isnan(b)) return true;
                      return ascending ? (a < b) : (a > b);
                  });
        // Output orientation matches input row/col-vector orientation.
        const bool isRow = (x.dims().rows() == 1);
        auto out = isRow
                    ? Value::matrix(1, k, ValueType::DOUBLE, mr)
                    : Value::matrix(k, 1, ValueType::DOUBLE, mr);
        for (size_t i = 0; i < k; ++i) out.doubleDataMut()[i] = buf[i];
        return out;
    }

    const int d = resolveDim(x, dim, fn);
    const auto &dd = x.dims();
    if (dd.ndim() >= 4)
        throw Error(std::string(fn) + ": ND (rank>=4) input not yet supported",
                     0, 0, fn, "", std::string("m:") + fn + ":nd");

    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    const size_t pageStride = R * C;
    size_t Ro = R, Co = C, Po = P;
    size_t sliceLen = 0;
    if (d == 1)      { sliceLen = R; Ro = std::min<size_t>(static_cast<size_t>(kReq), R); }
    else if (d == 2) { sliceLen = C; Co = std::min<size_t>(static_cast<size_t>(kReq), C); }
    else if (d == 3) { sliceLen = P; Po = std::min<size_t>(static_cast<size_t>(kReq), P); }
    else {
        // Out-of-rank → identity copy.
        return createLike(x, ValueType::DOUBLE, mr);
    }

    auto out = dd.is3D()
                ? Value::matrix3d(Ro, Co, Po, ValueType::DOUBLE, mr)
                : Value::matrix(Ro, Co, ValueType::DOUBLE, mr);
    const double *src = x.doubleData();
    double *dst = out.doubleDataMut();
    std::vector<double> buf(sliceLen);

    auto sortBuf = [&]() {
        std::sort(buf.begin(), buf.end(),
            [ascending](double a, double b) {
                if (std::isnan(a)) return false;
                if (std::isnan(b)) return true;
                return ascending ? (a < b) : (a > b);
            });
    };

    if (d == 1) {
        const size_t outPageStride = Ro * Co;
        for (size_t p = 0; p < P; ++p)
            for (size_t c = 0; c < C; ++c) {
                for (size_t r = 0; r < R; ++r)
                    buf[r] = src[p * pageStride + c * R + r];
                sortBuf();
                for (size_t i = 0; i < Ro; ++i)
                    dst[p * outPageStride + c * Ro + i] = buf[i];
            }
    } else if (d == 2) {
        const size_t outPageStride = Ro * Co;
        for (size_t p = 0; p < P; ++p)
            for (size_t r = 0; r < R; ++r) {
                for (size_t c = 0; c < C; ++c)
                    buf[c] = src[p * pageStride + c * R + r];
                sortBuf();
                for (size_t i = 0; i < Co; ++i)
                    dst[p * outPageStride + i * Ro + r] = buf[i];
            }
    } else { // d == 3
        const size_t outPageStride = Ro * Co;
        for (size_t c = 0; c < C; ++c)
            for (size_t r = 0; r < R; ++r) {
                for (size_t p = 0; p < P; ++p)
                    buf[p] = src[p * pageStride + c * R + r];
                sortBuf();
                for (size_t i = 0; i < Po; ++i)
                    dst[i * outPageStride + c * Ro + r] = buf[i];
            }
    }
    return out;
}

} // namespace

// ── maxk / mink ───────────────────────────────────────────────────────
Value maxk(std::pmr::memory_resource *mr, const Value &x, int k, int dim)
{
    return topKAlongDim(mr, x, dim, k, /*ascending=*/false, "maxk");
}

Value mink(std::pmr::memory_resource *mr, const Value &x, int k, int dim)
{
    return topKAlongDim(mr, x, dim, k, /*ascending=*/true, "mink");
}

// ── rmse ──────────────────────────────────────────────────────────────
Value rmse(std::pmr::memory_resource *mr, const Value &f, const Value &a, int dim)
{
    if (f.dims() != a.dims() && !(f.isScalar() || a.isScalar()))
        throw Error("rmse: F and A must have compatible sizes",
                     0, 0, "rmse", "", "m:rmse:sizeMismatch");
    // Build the squared-difference array, then reduce.
    const size_t n = std::max(f.numel(), a.numel());
    auto diff = (f.numel() >= a.numel())
                  ? createLike(f, ValueType::DOUBLE, mr)
                  : createLike(a, ValueType::DOUBLE, mr);
    double *dst = diff.doubleDataMut();
    if (f.isScalar()) {
        const double fs = f.toScalar();
        for (size_t i = 0; i < n; ++i) {
            const double d = a.elemAsDouble(i) - fs;
            dst[i] = d * d;
        }
    } else if (a.isScalar()) {
        const double as = a.toScalar();
        for (size_t i = 0; i < n; ++i) {
            const double d = f.elemAsDouble(i) - as;
            dst[i] = d * d;
        }
    } else {
        for (size_t i = 0; i < n; ++i) {
            const double d = f.elemAsDouble(i) - a.elemAsDouble(i);
            dst[i] = d * d;
        }
    }
    const int dResolved = resolveDim(diff, dim, "rmse");
    auto v = applyAlongDim(diff, dResolved,
        [](size_t, const double *s, size_t k) -> double {
            if (k == 0) return std::numeric_limits<double>::quiet_NaN();
            double sum = 0.0;
            for (size_t i = 0; i < k; ++i) sum += s[i];
            return std::sqrt(sum / static_cast<double>(k));
        }, mr);
    return v;
}

// ── mape ──────────────────────────────────────────────────────────────
Value mape(std::pmr::memory_resource *mr, const Value &f, const Value &a, int dim)
{
    if (f.dims() != a.dims() && !(f.isScalar() || a.isScalar()))
        throw Error("mape: F and A must have compatible sizes",
                     0, 0, "mape", "", "m:mape:sizeMismatch");
    const size_t n = std::max(f.numel(), a.numel());
    auto pct = (f.numel() >= a.numel())
                  ? createLike(f, ValueType::DOUBLE, mr)
                  : createLike(a, ValueType::DOUBLE, mr);
    double *dst = pct.doubleDataMut();

    if (f.isScalar()) {
        const double fs = f.toScalar();
        for (size_t i = 0; i < n; ++i) {
            const double ai = a.elemAsDouble(i);
            dst[i] = std::abs((ai - fs) / ai) * 100.0;
        }
    } else if (a.isScalar()) {
        const double as = a.toScalar();
        for (size_t i = 0; i < n; ++i) {
            const double fi = f.elemAsDouble(i);
            dst[i] = std::abs((as - fi) / as) * 100.0;
        }
    } else {
        for (size_t i = 0; i < n; ++i) {
            const double fi = f.elemAsDouble(i);
            const double ai = a.elemAsDouble(i);
            dst[i] = std::abs((ai - fi) / ai) * 100.0;
        }
    }

    const int dResolved = resolveDim(pct, dim, "mape");
    return applyAlongDim(pct, dResolved,
        [](size_t, const double *s, size_t k) -> double {
            if (k == 0) return std::numeric_limits<double>::quiet_NaN();
            double sum = 0.0;
            for (size_t i = 0; i < k; ++i) sum += s[i];
            return sum / static_cast<double>(k);
        }, mr);
}

// ── prepareCurveData / prepareSurfaceData ─────────────────────────────

namespace {

inline bool finite_double(double v) {
    return !std::isnan(v) && !std::isinf(v);
}

// Flatten + filter helper. `srcs` give pointer/numel pairs (already
// linearised), `keep_mask[i]` indicates whether row i survives.
Value pack_filtered(std::pmr::memory_resource *mr,
                    const std::vector<double> &src,
                    const std::vector<uint8_t> &keep)
{
    size_t kept = 0;
    for (uint8_t k : keep) if (k) ++kept;
    Value out = Value::matrix(kept, 1, ValueType::DOUBLE, mr);
    if (kept == 0) return out;
    double *od = out.doubleDataMut();
    size_t j = 0;
    for (size_t i = 0; i < src.size(); ++i)
        if (keep[i]) od[j++] = src[i];
    return out;
}

} // anonymous

std::tuple<Value, Value, Value>
prepareCurveData(std::pmr::memory_resource *mr,
                 const Value &x, const Value &y, const Value &w)
{
    const size_t Nx = x.numel();
    const size_t Ny = y.numel();
    const bool   hasW = !w.isEmpty();
    const size_t Nw = hasW ? w.numel() : Nx;
    if (Nx != Ny || (hasW && Nw != Nx))
        throw Error("prepareCurveData: x, y" +
                    std::string(hasW ? ", w" : "") + " must be same length",
                    0, 0, "prepareCurveData", "", "m:prepCD:size");

    std::vector<double> xv(Nx), yv(Nx), wv(hasW ? Nx : 0);
    std::vector<uint8_t> keep(Nx, 1);
    for (size_t i = 0; i < Nx; ++i) {
        xv[i] = x.elemAsDouble(i);
        yv[i] = y.elemAsDouble(i);
        if (hasW) wv[i] = w.elemAsDouble(i);
        if (!finite_double(xv[i]) || !finite_double(yv[i]) ||
            (hasW && !finite_double(wv[i])))
            keep[i] = 0;
    }

    Value xo = pack_filtered(mr, xv, keep);
    Value yo = pack_filtered(mr, yv, keep);
    Value wo = hasW ? pack_filtered(mr, wv, keep)
                    : Value::matrix(0, 1, ValueType::DOUBLE, mr);
    return {std::move(xo), std::move(yo), std::move(wo)};
}

std::tuple<Value, Value, Value>
prepareSurfaceData(std::pmr::memory_resource *mr,
                   const Value &x, const Value &y, const Value &z)
{
    // For surface fits MATLAB lets x and y be either vectors of length
    // numel(z), or matrices the same shape as z (meshgrid). Normalise
    // by linearising in column-major order; numel must match.
    const size_t Nz = z.numel();
    const size_t Nx = x.numel();
    const size_t Ny = y.numel();

    auto broadcastTo = [&](const Value &v, size_t target) -> std::vector<double> {
        std::vector<double> out(target);
        if (v.numel() == target) {
            for (size_t i = 0; i < target; ++i) out[i] = v.elemAsDouble(i);
        } else {
            // Allow x = row of length cols, y = col of length rows for
            // implicit meshgrid (matches MATLAB behaviour).
            const auto &dz = z.dims();
            const size_t rows = dz.rows();
            const size_t cols = dz.cols();
            if (v.numel() == cols) {
                // treat as x-coords per column
                for (size_t c = 0; c < cols; ++c)
                    for (size_t r = 0; r < rows; ++r)
                        out[r + c * rows] = v.elemAsDouble(c);
            } else if (v.numel() == rows) {
                for (size_t c = 0; c < cols; ++c)
                    for (size_t r = 0; r < rows; ++r)
                        out[r + c * rows] = v.elemAsDouble(r);
            } else {
                throw Error("prepareSurfaceData: x, y, z size mismatch",
                            0, 0, "prepareSurfaceData", "", "m:prepSD:size");
            }
        }
        return out;
    };

    std::vector<double> xv = broadcastTo(x, Nz);
    std::vector<double> yv = broadcastTo(y, Nz);
    std::vector<double> zv(Nz);
    for (size_t i = 0; i < Nz; ++i) zv[i] = z.elemAsDouble(i);

    std::vector<uint8_t> keep(Nz, 1);
    for (size_t i = 0; i < Nz; ++i) {
        if (!finite_double(xv[i]) || !finite_double(yv[i]) ||
            !finite_double(zv[i]))
            keep[i] = 0;
    }

    Value xo = pack_filtered(mr, xv, keep);
    Value yo = pack_filtered(mr, yv, keep);
    Value zo = pack_filtered(mr, zv, keep);
    (void)Nx; (void)Ny;
    return {std::move(xo), std::move(yo), std::move(zo)};
}

// ── datastats ─────────────────────────────────────────────────────────

std::tuple<Value, Value, Value, Value, Value, Value, Value>
datastats(std::pmr::memory_resource *mr, const Value &x)
{
    const size_t N = x.numel();
    std::vector<double> v;
    v.reserve(N);
    for (size_t i = 0; i < N; ++i) v.push_back(x.elemAsDouble(i));

    const auto nan = std::numeric_limits<double>::quiet_NaN();
    if (N == 0) {
        return std::make_tuple(Value::scalar(0.0, mr),
                               Value::scalar(nan, mr),
                               Value::scalar(nan, mr),
                               Value::scalar(nan, mr),
                               Value::scalar(nan, mr),
                               Value::scalar(nan, mr),
                               Value::scalar(nan, mr));
    }

    double mn = v[0], mx = v[0], sum = 0.0;
    for (double vi : v) {
        if (vi < mn) mn = vi;
        if (vi > mx) mx = vi;
        sum += vi;
    }
    const double mean = sum / static_cast<double>(N);

    std::vector<double> sorted = v;
    std::sort(sorted.begin(), sorted.end());
    double median;
    if (N % 2 == 1) median = sorted[N / 2];
    else            median = 0.5 * (sorted[N / 2 - 1] + sorted[N / 2]);

    double sd = 0.0;
    if (N > 1) {
        double sq = 0.0;
        for (double vi : v) { const double d = vi - mean; sq += d * d; }
        sd = std::sqrt(sq / static_cast<double>(N - 1));
    }
    const double range = mx - mn;

    return std::make_tuple(Value::scalar(static_cast<double>(N), mr),
                           Value::scalar(mx,     mr),
                           Value::scalar(mn,     mr),
                           Value::scalar(mean,   mr),
                           Value::scalar(median, mr),
                           Value::scalar(range,  mr),
                           Value::scalar(sd,     mr));
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void prepareCurveData_reg(Span<const Value> args, size_t nargout,
                          Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("prepareCurveData: requires (X, Y[, W])",
                    0, 0, "prepareCurveData", "", "m:prepCD:nargin");
    auto *mr = ctx.engine->resource();
    Value w_empty = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    const Value &w = (args.size() >= 3) ? args[2] : w_empty;
    auto [xo, yo, wo] = prepareCurveData(mr, args[0], args[1], w);
    outs[0] = std::move(xo);
    if (nargout > 1) outs[1] = std::move(yo);
    if (nargout > 2) outs[2] = std::move(wo);
}

void prepareSurfaceData_reg(Span<const Value> args, size_t nargout,
                            Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("prepareSurfaceData: requires (X, Y, Z)",
                    0, 0, "prepareSurfaceData", "", "m:prepSD:nargin");
    auto [xo, yo, zo] = prepareSurfaceData(ctx.engine->resource(),
                                            args[0], args[1], args[2]);
    outs[0] = std::move(xo);
    if (nargout > 1) outs[1] = std::move(yo);
    if (nargout > 2) outs[2] = std::move(zo);
}

void datastats_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("datastats: requires X[, Y]",
                    0, 0, "datastats", "", "m:datastats:nargin");
    auto build = [&](const Value &v) {
        auto [num, mx, mn, me, md, rg, sd] =
            datastats(ctx.engine->resource(), v);
        Value s = Value::structure(ctx.engine->resource());
        s.field("num")    = num;
        s.field("max")    = mx;
        s.field("min")    = mn;
        s.field("mean")   = me;
        s.field("median") = md;
        s.field("range")  = rg;
        s.field("std")    = sd;
        return s;
    };
    outs[0] = build(args[0]);
    // Two-arg form returns separate stats structs for x and y.
    // We only fill outs[1] if a second argument was supplied AND the
    // caller actually requested two outputs (otherwise it's a no-op).
    if (args.size() >= 2 && outs.size() > 1)
        outs[1] = build(args[1]);
}

void bounds_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bounds: requires at least 1 argument",
                     0, 0, "bounds", "", "m:bounds:nargin");
    const int dim = (args.size() >= 2) ? static_cast<int>(args[1].toScalar()) : 0;
    auto [lo, hi] = bounds(ctx.engine->resource(), args[0], dim);
    outs[0] = std::move(lo);
    if (nargout > 1) outs[1] = std::move(hi);
}

void iqr_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("iqr: requires at least 1 argument",
                     0, 0, "iqr", "", "m:iqr:nargin");
    const int dim = (args.size() >= 2) ? static_cast<int>(args[1].toScalar()) : 0;
    outs[0] = iqr(ctx.engine->resource(), args[0], dim);
}

void maxk_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("maxk: requires at least 2 arguments (x, k)",
                     0, 0, "maxk", "", "m:maxk:nargin");
    const int k = static_cast<int>(args[1].toScalar());
    const int dim = (args.size() >= 3) ? static_cast<int>(args[2].toScalar()) : 0;
    outs[0] = maxk(ctx.engine->resource(), args[0], k, dim);
}

void mink_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mink: requires at least 2 arguments (x, k)",
                     0, 0, "mink", "", "m:mink:nargin");
    const int k = static_cast<int>(args[1].toScalar());
    const int dim = (args.size() >= 3) ? static_cast<int>(args[2].toScalar()) : 0;
    outs[0] = mink(ctx.engine->resource(), args[0], k, dim);
}

void mape_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mape: requires 2 arguments (F, A)",
                     0, 0, "mape", "", "m:mape:nargin");
    const int dim = (args.size() >= 3) ? static_cast<int>(args[2].toScalar()) : 0;
    outs[0] = mape(ctx.engine->resource(), args[0], args[1], dim);
}

void rmse_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rmse: requires at least 2 arguments (F, A)",
                     0, 0, "rmse", "", "m:rmse:nargin");
    const int dim = (args.size() >= 3) ? static_cast<int>(args[2].toScalar()) : 0;
    outs[0] = rmse(ctx.engine->resource(), args[0], args[1], dim);
}

void ecdf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ecdf: requires 1 argument (y)",
                     0, 0, "ecdf", "", "m:ecdf:nargin");
    auto [f, x] = ecdf(ctx.engine->resource(), args[0]);
    outs[0] = std::move(f);
    if (nargout > 1) outs[1] = std::move(x);
}

void ecdfhist_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ecdfhist: requires (f, x [, m])",
                     0, 0, "ecdfhist", "", "m:ecdfhist:nargin");
    int m = 10;
    if (args.size() >= 3 && !args[2].isEmpty())
        m = static_cast<int>(args[2].toScalar());
    auto [n, c] = ecdfhist(ctx.engine->resource(), args[0], args[1], m);
    outs[0] = std::move(n);
    if (nargout > 1) outs[1] = std::move(c);
}

} // namespace detail

// ── ecdfhist ─────────────────────────────────────────────────────────
// Convert (f, x) from ecdf into a probability-density histogram.
// Algorithm: probs[k] = f[k+1] - f[k]; vals[k] = x[k+1] (the unique
// data values). Bin into m equal-width bins over [min(x), max(x)],
// each height = sum(probs in bin) / bin_width.
std::tuple<Value, Value>
ecdfhist(std::pmr::memory_resource *mr, const Value &f, const Value &x, int m)
{
    if (m < 1)
        throw Error("ecdfhist: number of bins must be >= 1",
                    0, 0, "ecdfhist", "", "m:ecdfhist:nbins");
    const size_t Lf = f.numel();
    const size_t Lx = x.numel();
    if (Lf != Lx)
        throw Error("ecdfhist: f and x must have the same length",
                    0, 0, "ecdfhist", "", "m:ecdfhist:size");
    if (Lf < 2) {
        Value n_empty = Value::matrix(1, static_cast<size_t>(m), ValueType::DOUBLE, mr);
        Value c_empty = Value::matrix(1, static_cast<size_t>(m), ValueType::DOUBLE, mr);
        return {std::move(n_empty), std::move(c_empty)};
    }

    // Build (vals, probs) from the ecdf step structure.
    const size_t K = Lf - 1;
    std::vector<double> vals(K), probs(K);
    for (size_t k = 0; k < K; ++k) {
        vals[k]  = x.elemAsDouble(k + 1);
        probs[k] = f.elemAsDouble(k + 1) - f.elemAsDouble(k);
    }

    const double xmin = vals.front();
    const double xmax = vals.back();
    const double width = (xmax > xmin)
        ? (xmax - xmin) / static_cast<double>(m)
        : 1.0;  // degenerate single-value case: arbitrary width=1

    Value n_out = Value::matrix(1, static_cast<size_t>(m), ValueType::DOUBLE, mr);
    Value c_out = Value::matrix(1, static_cast<size_t>(m), ValueType::DOUBLE, mr);
    double *nd = n_out.doubleDataMut();
    double *cd = c_out.doubleDataMut();

    for (int k = 0; k < m; ++k) {
        cd[k] = xmin + (k + 0.5) * width;
        nd[k] = 0.0;
    }
    if (xmax <= xmin) {
        // All-equal data: drop full mass into the centre bin.
        nd[m / 2] = 1.0 / width;
        return {std::move(n_out), std::move(c_out)};
    }

    for (size_t k = 0; k < K; ++k) {
        const double v = vals[k];
        int bin = static_cast<int>(std::floor((v - xmin) / width));
        if (bin >= m) bin = m - 1;  // last bin includes right edge
        if (bin < 0)  bin = 0;
        nd[bin] += probs[k];
    }
    for (int k = 0; k < m; ++k) nd[k] /= width;
    return {std::move(n_out), std::move(c_out)};
}

// ── ecdf ─────────────────────────────────────────────────────────────
// Empirical CDF. Sort data, drop NaN, then for each unique value produce
// a (cumcount/N) jump. Output: 2 column vectors of length K+1.
std::tuple<Value, Value>
ecdf(std::pmr::memory_resource *mr, const Value &y)
{
    const size_t n = y.numel();
    std::vector<double> v;
    v.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        const double s = y.elemAsDouble(i);
        if (!std::isnan(s)) v.push_back(s);
    }
    const size_t N = v.size();
    if (N == 0) {
        // MATLAB returns empty 0x1 columns on all-NaN / empty input.
        Value fEmpty = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        Value xEmpty = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        return {std::move(fEmpty), std::move(xEmpty)};
    }
    std::sort(v.begin(), v.end());

    // Walk through sorted v and emit (cumulative count, value) at each
    // value transition. Output size = K + 1, where K is the number of
    // distinct values.
    std::vector<double> fs, xs;
    fs.push_back(0.0);
    xs.push_back(v[0]);  // F = 0 at x = min(y)
    size_t i = 0;
    while (i < N) {
        size_t j = i + 1;
        while (j < N && v[j] == v[i]) ++j;
        // j - i copies of v[i]; cumulative count after this group is j.
        fs.push_back(static_cast<double>(j) / static_cast<double>(N));
        xs.push_back(v[i]);
        i = j;
    }

    const size_t L = fs.size();
    Value fOut = Value::matrix(L, 1, ValueType::DOUBLE, mr);
    Value xOut = Value::matrix(L, 1, ValueType::DOUBLE, mr);
    double *fd = fOut.doubleDataMut();
    double *xd = xOut.doubleDataMut();
    for (size_t k = 0; k < L; ++k) { fd[k] = fs[k]; xd[k] = xs[k]; }
    return {std::move(fOut), std::move(xOut)};
}

} // namespace numkit::stats
