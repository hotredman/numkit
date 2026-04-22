// libs/builtin/src/MStdMath.cpp

#include <numkit/m/builtin/MStdMath.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"
#include "MStdReductionHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <random>

namespace numkit::m::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

// ── Elementwise unary — complex-promoting ──────────────────────────────
MValue sqrt(Allocator &alloc, const MValue &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::sqrt(c); }, &alloc);
    if (x.isScalar() && x.toScalar() < 0)
        return MValue::complexScalar(std::sqrt(Complex(x.toScalar(), 0.0)), &alloc);
    return unaryDouble(x, [](double v) { return std::sqrt(v); }, &alloc);
}

// abs() now lives in libs/builtin/src/backends/MStdAbs_{portable,simd}.cpp.
// CMake picks one based on NUMKIT_WITH_SIMD; the portable copy is the
// scalar reference, the SIMD copy is identical for tiny / complex inputs
// but dispatches the real-vector fast path to Highway.

// sin / cos / exp / log now live in
// libs/builtin/src/backends/MStdTranscendental_{portable,simd}.cpp.
// tan / asin / acos / atan stay here until they get SIMD backends —
// Highway's hwy/contrib/math has atan/asin/acos/tan equivalents, so
// they're reasonable follow-ups but weren't in the 7c bench scope.

MValue tan(Allocator &alloc, const MValue &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::tan(c); }, &alloc);
    return unaryDouble(x, [](double v) { return std::tan(v); }, &alloc);
}

MValue asin(Allocator &alloc, const MValue &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::asin(c); }, &alloc);
    return unaryDouble(x, [](double v) { return std::asin(v); }, &alloc);
}

MValue acos(Allocator &alloc, const MValue &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::acos(c); }, &alloc);
    return unaryDouble(x, [](double v) { return std::acos(v); }, &alloc);
}

MValue atan(Allocator &alloc, const MValue &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::atan(c); }, &alloc);
    return unaryDouble(x, [](double v) { return std::atan(v); }, &alloc);
}

// ── Elementwise unary — double only ───────────────────────────────────
MValue log2(Allocator &alloc, const MValue &x)
{
    return unaryDouble(x, [](double v) { return std::log2(v); }, &alloc);
}

MValue log10(Allocator &alloc, const MValue &x)
{
    return unaryDouble(x, [](double v) { return std::log10(v); }, &alloc);
}

MValue floor(Allocator &alloc, const MValue &x)
{
    return unaryDouble(x, [](double v) { return std::floor(v); }, &alloc);
}

MValue ceil(Allocator &alloc, const MValue &x)
{
    return unaryDouble(x, [](double v) { return std::ceil(v); }, &alloc);
}

MValue round(Allocator &alloc, const MValue &x)
{
    return unaryDouble(x, [](double v) { return std::round(v); }, &alloc);
}

MValue fix(Allocator &alloc, const MValue &x)
{
    return unaryDouble(x, [](double v) { return std::trunc(v); }, &alloc);
}

MValue sign(Allocator &alloc, const MValue &x)
{
    return unaryDouble(x,
                       [](double v) {
                           return std::isnan(v) ? v : (v > 0) ? 1.0 : (v < 0 ? -1.0 : 0.0);
                       },
                       &alloc);
}

MValue deg2rad(Allocator &alloc, const MValue &x)
{
    constexpr double k = 3.14159265358979323846 / 180.0;
    return unaryDouble(x, [k](double v) { return v * k; }, &alloc);
}

MValue rad2deg(Allocator &alloc, const MValue &x)
{
    constexpr double k = 180.0 / 3.14159265358979323846;
    return unaryDouble(x, [k](double v) { return v * k; }, &alloc);
}

// ── Elementwise binary ───────────────────────────────────────────────
MValue atan2(Allocator &alloc, const MValue &y, const MValue &x)
{
    return elementwiseDouble(y, x, [](double yy, double xx) { return std::atan2(yy, xx); }, &alloc);
}

MValue mod(Allocator &alloc, const MValue &a, const MValue &b)
{
    return elementwiseDouble(a, b,
                             [](double aa, double bb) {
                                 return bb != 0 ? aa - std::floor(aa / bb) * bb : aa;
                             },
                             &alloc);
}

MValue rem(Allocator &alloc, const MValue &a, const MValue &b)
{
    return elementwiseDouble(a, b, [](double aa, double bb) { return std::fmod(aa, bb); }, &alloc);
}

MValue hypot(Allocator &alloc, const MValue &x, const MValue &y)
{
    return elementwiseDouble(x, y,
        [](double a, double b) { return std::hypot(a, b); }, &alloc);
}

// nthroot(x, n): real n-th root. For negative x with odd integer n,
// returns the negative real root (sign(x) * |x|^(1/n)). For negative x
// with non-odd n, returns NaN — MATLAB throws there but NaN keeps
// vectorised callers from having to special-case before the call.
MValue nthroot(Allocator &alloc, const MValue &x, const MValue &n)
{
    return elementwiseDouble(x, n, [](double xv, double nv) {
        if (nv == 0.0) return std::nan("");
        if (xv >= 0.0) return std::pow(xv, 1.0 / nv);
        const double rounded = std::round(nv);
        if (rounded != nv) return std::nan("");
        const long long ni = static_cast<long long>(rounded);
        if (ni % 2 == 0) return std::nan("");
        return -std::pow(-xv, 1.0 / nv);
    }, &alloc);
}

MValue expm1(Allocator &alloc, const MValue &x)
{
    return unaryDouble(x, [](double v) { return std::expm1(v); }, &alloc);
}

MValue log1p(Allocator &alloc, const MValue &x)
{
    return unaryDouble(x, [](double v) { return std::log1p(v); }, &alloc);
}

// ── Reductions (single-return) ───────────────────────────────────────
namespace {

// Generic column-/dim-wise reducer: applies op(acc, x) and initializes
// acc with init. For 2D: reduces across rows → row vector of cols. For 3D:
// reduces along first non-singleton dimension. For vectors/scalars: scalar.
template<typename Op>
MValue reduce(const MValue &x, Op op, double init, Allocator *alloc, bool meanMode = false)
{
    if (x.dims().isVector() || x.isScalar()) {
        double acc = init;
        for (size_t i = 0; i < x.numel(); ++i)
            acc = op(acc, x.doubleData()[i]);
        if (meanMode)
            acc /= static_cast<double>(x.numel());
        return MValue::scalar(acc, alloc);
    }

    const size_t R = x.dims().rows(), C = x.dims().cols();

    if (x.dims().is3D()) {
        const size_t P = x.dims().pages();
        const int redDim = (R > 1) ? 0 : (C > 1) ? 1 : 2;
        const size_t outR = (redDim == 0) ? 1 : R;
        const size_t outC = (redDim == 1) ? 1 : C;
        const size_t outP = (redDim == 2) ? 1 : P;
        const size_t N = (redDim == 0) ? R : (redDim == 1) ? C : P;
        auto r = MValue::matrix3d(outR, outC, outP, MType::DOUBLE, alloc);
        for (size_t pp = 0; pp < outP; ++pp)
            for (size_t c = 0; c < outC; ++c)
                for (size_t rr = 0; rr < outR; ++rr) {
                    double acc = init;
                    for (size_t k = 0; k < N; ++k) {
                        const size_t rIdx = (redDim == 0) ? k : rr;
                        const size_t cIdx = (redDim == 1) ? k : c;
                        const size_t pIdx = (redDim == 2) ? k : pp;
                        acc = op(acc, x(rIdx, cIdx, pIdx));
                    }
                    if (meanMode)
                        acc /= static_cast<double>(N);
                    r.doubleDataMut()[pp * outR * outC + c * outR + rr] = acc;
                }
        return r;
    }

    auto r = MValue::matrix(1, C, MType::DOUBLE, alloc);
    for (size_t c = 0; c < C; ++c) {
        double acc = init;
        for (size_t rr = 0; rr < R; ++rr)
            acc = op(acc, x(rr, c));
        if (meanMode)
            acc /= static_cast<double>(R);
        r.doubleDataMut()[c] = acc;
    }
    return r;
}

} // anonymous namespace

MValue sum(Allocator &alloc, const MValue &x)
{
    return reduce(x, [](double a, double b) { return a + b; }, 0.0, &alloc);
}

MValue sum(Allocator &alloc, const MValue &x, int dim)
{
    if (dim <= 0) return sum(alloc, x);
    const int d = detail::resolveDim(x, dim, "sum");
    return detail::applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            double acc = 0.0;
            for (size_t i = 0; i < n; ++i) acc += slice[i];
            return acc;
        }, &alloc);
}

MValue prod(Allocator &alloc, const MValue &x)
{
    return reduce(x, [](double a, double b) { return a * b; }, 1.0, &alloc);
}

MValue prod(Allocator &alloc, const MValue &x, int dim)
{
    if (dim <= 0) return prod(alloc, x);
    const int d = detail::resolveDim(x, dim, "prod");
    return detail::applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            double acc = 1.0;
            for (size_t i = 0; i < n; ++i) acc *= slice[i];
            return acc;
        }, &alloc);
}

MValue mean(Allocator &alloc, const MValue &x)
{
    return reduce(x, [](double a, double b) { return a + b; }, 0.0, &alloc, /*meanMode=*/true);
}

MValue mean(Allocator &alloc, const MValue &x, int dim)
{
    if (dim <= 0) return mean(alloc, x);
    const int d = detail::resolveDim(x, dim, "mean");
    return detail::applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            double acc = 0.0;
            for (size_t i = 0; i < n; ++i) acc += slice[i];
            return acc / static_cast<double>(n);
        }, &alloc);
}

// ── max/min with index ───────────────────────────────────────────────
namespace {

// Generic reducer that tracks (value, index). Cmp(a, b) returns true when
// a "wins" over b (for max: a > b, for min: a < b).
template<typename Cmp>
std::tuple<MValue, MValue>
reduceWithIndex(const MValue &x, Cmp cmp, Allocator *alloc)
{
    if (x.dims().isVector() || x.isScalar()) {
        double best = x.doubleData()[0];
        size_t bi = 0;
        for (size_t i = 1; i < x.numel(); ++i) {
            if (cmp(x.doubleData()[i], best)) {
                best = x.doubleData()[i];
                bi = i;
            }
        }
        return std::make_tuple(MValue::scalar(best, alloc),
                               MValue::scalar(static_cast<double>(bi + 1), alloc));
    }

    const size_t R = x.dims().rows(), C = x.dims().cols();

    if (x.dims().is3D()) {
        const size_t P = x.dims().pages();
        const int redDim = (R > 1) ? 0 : (C > 1) ? 1 : 2;
        const size_t outR = (redDim == 0) ? 1 : R;
        const size_t outC = (redDim == 1) ? 1 : C;
        const size_t outP = (redDim == 2) ? 1 : P;
        const size_t N = (redDim == 0) ? R : (redDim == 1) ? C : P;
        auto r = MValue::matrix3d(outR, outC, outP, MType::DOUBLE, alloc);
        auto idx = MValue::matrix3d(outR, outC, outP, MType::DOUBLE, alloc);
        for (size_t pp = 0; pp < outP; ++pp)
            for (size_t c = 0; c < outC; ++c)
                for (size_t rr = 0; rr < outR; ++rr) {
                    auto atK = [&](size_t k) {
                        const size_t rIdx = (redDim == 0) ? k : rr;
                        const size_t cIdx = (redDim == 1) ? k : c;
                        const size_t pIdx = (redDim == 2) ? k : pp;
                        return x(rIdx, cIdx, pIdx);
                    };
                    double best = atK(0);
                    size_t bi = 0;
                    for (size_t k = 1; k < N; ++k) {
                        const double v = atK(k);
                        if (cmp(v, best)) {
                            best = v;
                            bi = k;
                        }
                    }
                    const size_t o = pp * outR * outC + c * outR + rr;
                    r.doubleDataMut()[o] = best;
                    idx.doubleDataMut()[o] = static_cast<double>(bi + 1);
                }
        return std::make_tuple(std::move(r), std::move(idx));
    }

    auto r = MValue::matrix(1, C, MType::DOUBLE, alloc);
    auto idx = MValue::matrix(1, C, MType::DOUBLE, alloc);
    for (size_t c = 0; c < C; ++c) {
        double best = x(0, c);
        size_t bi = 0;
        for (size_t rr = 1; rr < R; ++rr) {
            if (cmp(x(rr, c), best)) {
                best = x(rr, c);
                bi = rr;
            }
        }
        r.doubleDataMut()[c] = best;
        idx.doubleDataMut()[c] = static_cast<double>(bi + 1);
    }
    return std::make_tuple(std::move(r), std::move(idx));
}

} // anonymous namespace

std::tuple<MValue, MValue> max(Allocator &alloc, const MValue &x)
{
    return reduceWithIndex(x, [](double v, double best) { return v > best; }, &alloc);
}

std::tuple<MValue, MValue> min(Allocator &alloc, const MValue &x)
{
    return reduceWithIndex(x, [](double v, double best) { return v < best; }, &alloc);
}

std::tuple<MValue, MValue> max(Allocator &alloc, const MValue &x, int dim)
{
    if (dim <= 0) return max(alloc, x);
    const int d = detail::resolveDim(x, dim, "max");
    auto [v, idx] = detail::applyAlongDimWithIndex(x, d,
        [](double *slice, size_t n, double &outV, size_t &outIdx) {
            double best = slice[0]; size_t bi = 0;
            for (size_t i = 1; i < n; ++i)
                if (slice[i] > best) { best = slice[i]; bi = i; }
            outV = best; outIdx = bi;
        }, &alloc);
    return std::make_tuple(std::move(v), std::move(idx));
}

std::tuple<MValue, MValue> min(Allocator &alloc, const MValue &x, int dim)
{
    if (dim <= 0) return min(alloc, x);
    const int d = detail::resolveDim(x, dim, "min");
    auto [v, idx] = detail::applyAlongDimWithIndex(x, d,
        [](double *slice, size_t n, double &outV, size_t &outIdx) {
            double best = slice[0]; size_t bi = 0;
            for (size_t i = 1; i < n; ++i)
                if (slice[i] < best) { best = slice[i]; bi = i; }
            outV = best; outIdx = bi;
        }, &alloc);
    return std::make_tuple(std::move(v), std::move(idx));
}

MValue max(Allocator &alloc, const MValue &a, const MValue &b)
{
    return elementwiseDouble(a, b, [](double aa, double bb) { return std::max(aa, bb); }, &alloc);
}

MValue min(Allocator &alloc, const MValue &a, const MValue &b)
{
    return elementwiseDouble(a, b, [](double aa, double bb) { return std::min(aa, bb); }, &alloc);
}

// ── Generators ───────────────────────────────────────────────────────
MValue linspace(Allocator &alloc, double a, double b, size_t n)
{
    auto r = MValue::matrix(1, n, MType::DOUBLE, &alloc);
    if (n == 0)
        return r;
    if (n == 1) {
        r.doubleDataMut()[0] = b;
        return r;
    }
    for (size_t i = 0; i < n; ++i)
        r.doubleDataMut()[i] = a + (b - a) * static_cast<double>(i) / static_cast<double>(n - 1);
    return r;
}

MValue logspace(Allocator &alloc, double a, double b, size_t n)
{
    auto r = MValue::matrix(1, n, MType::DOUBLE, &alloc);
    if (n == 0)
        return r;
    if (n == 1) {
        r.doubleDataMut()[0] = std::pow(10.0, b);
        return r;
    }
    for (size_t i = 0; i < n; ++i) {
        const double exponent = a + (b - a) * static_cast<double>(i) / static_cast<double>(n - 1);
        r.doubleDataMut()[i] = std::pow(10.0, exponent);
    }
    return r;
}

MValue rand(Allocator &alloc, std::mt19937 &rng, size_t rows, size_t cols, size_t pages)
{
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    auto m = (pages > 0) ? MValue::matrix3d(rows, cols, pages, MType::DOUBLE, &alloc)
                         : MValue::matrix(rows, cols, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < m.numel(); ++i)
        m.doubleDataMut()[i] = dist(rng);
    return m;
}

MValue randn(Allocator &alloc, std::mt19937 &rng, size_t rows, size_t cols, size_t pages)
{
    std::normal_distribution<double> dist(0.0, 1.0);
    auto m = (pages > 0) ? MValue::matrix3d(rows, cols, pages, MType::DOUBLE, &alloc)
                         : MValue::matrix(rows, cols, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < m.numel(); ++i)
        m.doubleDataMut()[i] = dist(rng);
    return m;
}

// ════════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

// Helper to reduce boilerplate — unary adapter that calls Fn(alloc, args[0]).
#define NK_UNARY_ADAPTER(name, fn)                                              \
    void name##_reg(Span<const MValue> args, size_t /*nargout*/,                \
                    Span<MValue> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw MError(#name ": requires 1 argument",                          \
                         0, 0, #name, "", "m:" #name ":nargin");           \
        outs[0] = fn(ctx.engine->allocator(), args[0]);                         \
    }

// Same as NK_UNARY_ADAPTER but passes &outs[0] through as an
// output-buffer reuse hint. The callee MAY overwrite *outs[0]'s
// existing buffer instead of allocating fresh, when the hint is
// a uniquely-owned heap double of matching shape. The VM CALL
// handler pre-fills outs[0] with R[I.a] (the destination register)
// when none of the args alias the destination — so `z = sin(x)`
// in a loop reuses z's buffer iteration after iteration.
#define NK_UNARY_ADAPTER_HINT(name, fn)                                         \
    void name##_reg(Span<const MValue> args, size_t /*nargout*/,                \
                    Span<MValue> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw MError(#name ": requires 1 argument",                          \
                         0, 0, #name, "", "m:" #name ":nargin");           \
        outs[0] = fn(ctx.engine->allocator(), args[0], &outs[0]);               \
    }

NK_UNARY_ADAPTER(sqrt,    sqrt)
NK_UNARY_ADAPTER_HINT(abs,     abs)
NK_UNARY_ADAPTER_HINT(sin,     sin)
NK_UNARY_ADAPTER_HINT(cos,     cos)
NK_UNARY_ADAPTER(tan,     tan)
NK_UNARY_ADAPTER(asin,    asin)
NK_UNARY_ADAPTER(acos,    acos)
NK_UNARY_ADAPTER(atan,    atan)
NK_UNARY_ADAPTER_HINT(exp,     exp)
NK_UNARY_ADAPTER_HINT(log,     log)
NK_UNARY_ADAPTER(log2,    log2)
NK_UNARY_ADAPTER(log10,   log10)
NK_UNARY_ADAPTER(floor,   floor)
NK_UNARY_ADAPTER(ceil,    ceil)
NK_UNARY_ADAPTER(round,   round)
NK_UNARY_ADAPTER(fix,     fix)
NK_UNARY_ADAPTER(sign,    sign)
NK_UNARY_ADAPTER(deg2rad, deg2rad)
NK_UNARY_ADAPTER(rad2deg, rad2deg)

#undef NK_UNARY_ADAPTER

// sum/prod/mean — accept an optional dim argument as args[1].
// MATLAB allows sum(X), sum(X, dim) and sum(X, 'all'); the 'all' form
// is not yet supported. Numeric args[1] is interpreted as 1-based dim.
#define NK_REDUCTION_ADAPTER(name, fn)                                          \
    void name##_reg(Span<const MValue> args, size_t /*nargout*/,                \
                    Span<MValue> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw MError(#name ": requires at least 1 argument",                 \
                         0, 0, #name, "", "m:" #name ":nargin");                 \
        int dim = 0;                                                             \
        if (args.size() >= 2 && !args[1].isEmpty())                              \
            dim = static_cast<int>(args[1].toScalar());                          \
        outs[0] = (dim > 0)                                                      \
                     ? fn(ctx.engine->allocator(), args[0], dim)                 \
                     : fn(ctx.engine->allocator(), args[0]);                     \
    }

NK_REDUCTION_ADAPTER(sum,  sum)
NK_REDUCTION_ADAPTER(prod, prod)
NK_REDUCTION_ADAPTER(mean, mean)

#undef NK_REDUCTION_ADAPTER

// Phase 7: hypot/nthroot are binary; expm1/log1p are unary.
void hypot_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
               CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("hypot: requires 2 arguments",
                     0, 0, "hypot", "", "m:hypot:nargin");
    outs[0] = hypot(ctx.engine->allocator(), args[0], args[1]);
}

void nthroot_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("nthroot: requires 2 arguments",
                     0, 0, "nthroot", "", "m:nthroot:nargin");
    outs[0] = nthroot(ctx.engine->allocator(), args[0], args[1]);
}

void expm1_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
               CallContext &ctx)
{
    if (args.empty())
        throw MError("expm1: requires 1 argument",
                     0, 0, "expm1", "", "m:expm1:nargin");
    outs[0] = expm1(ctx.engine->allocator(), args[0]);
}

void log1p_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
               CallContext &ctx)
{
    if (args.empty())
        throw MError("log1p: requires 1 argument",
                     0, 0, "log1p", "", "m:log1p:nargin");
    outs[0] = log1p(ctx.engine->allocator(), args[0]);
}

// Binary adapters follow a slightly different pattern (variable name for 2nd arg)
void atan2_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("atan2: requires 2 arguments",
                     0, 0, "atan2", "", "m:atan2:nargin");
    outs[0] = atan2(ctx.engine->allocator(), args[0], args[1]);
}

void mod_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("mod: requires 2 arguments",
                     0, 0, "mod", "", "m:mod:nargin");
    outs[0] = mod(ctx.engine->allocator(), args[0], args[1]);
}

void rem_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("rem: requires 2 arguments",
                     0, 0, "rem", "", "m:rem:nargin");
    outs[0] = rem(ctx.engine->allocator(), args[0], args[1]);
}

// max/min: three MATLAB forms:
//   max(X)         — reduction along first non-singleton dim, (value, idx)
//   max(A, B)      — elementwise (with broadcasting), single return
//   max(X, [], dim) — reduction along explicit dim, (value, idx)
// Distinguishing the two-arg forms: max(X, []) hits the dim path with
// auto-detect (idx returned); max(X, B) with non-empty B is elementwise.
void max_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("max: requires at least 1 argument",
                     0, 0, "max", "", "m:max:nargin");
    if (args.size() >= 2 && !args[1].isEmpty()) {
        // Elementwise max(A, B) — single-return form.
        outs[0] = max(ctx.engine->allocator(), args[0], args[1]);
        return;
    }
    // Reduction: optional dim as args[2].
    int dim = 0;
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    auto [val, idx] = max(ctx.engine->allocator(), args[0], dim);
    outs[0] = std::move(val);
    if (nargout > 1)
        outs[1] = std::move(idx);
}

void min_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("min: requires at least 1 argument",
                     0, 0, "min", "", "m:min:nargin");
    if (args.size() >= 2 && !args[1].isEmpty()) {
        outs[0] = min(ctx.engine->allocator(), args[0], args[1]);
        return;
    }
    int dim = 0;
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    auto [val, idx] = min(ctx.engine->allocator(), args[0], dim);
    outs[0] = std::move(val);
    if (nargout > 1)
        outs[1] = std::move(idx);
}

// Generators
void linspace_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("linspace: requires at least 2 arguments",
                     0, 0, "linspace", "", "m:linspace:nargin");
    const double a = args[0].toScalar();
    const double b = args[1].toScalar();
    const size_t n = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 100u;
    outs[0] = linspace(ctx.engine->allocator(), a, b, n);
}

void logspace_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("logspace: requires at least 2 arguments",
                     0, 0, "logspace", "", "m:logspace:nargin");
    const double a = args[0].toScalar();
    const double b = args[1].toScalar();
    const size_t n = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 50u;
    outs[0] = logspace(ctx.engine->allocator(), a, b, n);
}

// rand_reg / randn_reg moved to MStdRng.cpp — they share a single
// process-static engine with randi / randperm so MATLAB-style
// rng(seed) controls all of them. The C++ public APIs rand(alloc,
// rng, …) / randn(alloc, rng, …) above stay here unchanged.

} // namespace detail

} // namespace numkit::m::builtin
