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

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

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

} // namespace detail

} // namespace numkit::stats
