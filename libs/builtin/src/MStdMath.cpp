// libs/builtin/src/MStdMath.cpp

#include <numkit/m/builtin/MStdMath.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

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

MValue abs(Allocator &alloc, const MValue &x)
{
    if (x.isComplex()) {
        if (x.isScalar())
            return MValue::scalar(std::abs(x.toComplex()), &alloc);
        auto r = createLike(x, MType::DOUBLE, &alloc);
        for (size_t i = 0; i < x.numel(); ++i)
            r.doubleDataMut()[i] = std::abs(x.complexData()[i]);
        return r;
    }
    return unaryDouble(x, [](double v) { return std::abs(v); }, &alloc);
}

MValue sin(Allocator &alloc, const MValue &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::sin(c); }, &alloc);
    return unaryDouble(x, [](double v) { return std::sin(v); }, &alloc);
}

MValue cos(Allocator &alloc, const MValue &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::cos(c); }, &alloc);
    return unaryDouble(x, [](double v) { return std::cos(v); }, &alloc);
}

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

MValue exp(Allocator &alloc, const MValue &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::exp(c); }, &alloc);
    return unaryDouble(x, [](double v) { return std::exp(v); }, &alloc);
}

MValue log(Allocator &alloc, const MValue &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::log(c); }, &alloc);
    if (x.isScalar() && x.toScalar() < 0)
        return MValue::complexScalar(std::log(Complex(x.toScalar(), 0.0)), &alloc);
    return unaryDouble(x, [](double v) { return std::log(v); }, &alloc);
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

MValue prod(Allocator &alloc, const MValue &x)
{
    return reduce(x, [](double a, double b) { return a * b; }, 1.0, &alloc);
}

MValue mean(Allocator &alloc, const MValue &x)
{
    return reduce(x, [](double a, double b) { return a + b; }, 0.0, &alloc, /*meanMode=*/true);
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

NK_UNARY_ADAPTER(sqrt,    sqrt)
NK_UNARY_ADAPTER(abs,     abs)
NK_UNARY_ADAPTER(sin,     sin)
NK_UNARY_ADAPTER(cos,     cos)
NK_UNARY_ADAPTER(tan,     tan)
NK_UNARY_ADAPTER(asin,    asin)
NK_UNARY_ADAPTER(acos,    acos)
NK_UNARY_ADAPTER(atan,    atan)
NK_UNARY_ADAPTER(exp,     exp)
NK_UNARY_ADAPTER(log,     log)
NK_UNARY_ADAPTER(log2,    log2)
NK_UNARY_ADAPTER(log10,   log10)
NK_UNARY_ADAPTER(floor,   floor)
NK_UNARY_ADAPTER(ceil,    ceil)
NK_UNARY_ADAPTER(round,   round)
NK_UNARY_ADAPTER(fix,     fix)
NK_UNARY_ADAPTER(sign,    sign)
NK_UNARY_ADAPTER(deg2rad, deg2rad)
NK_UNARY_ADAPTER(rad2deg, rad2deg)
NK_UNARY_ADAPTER(sum,     sum)
NK_UNARY_ADAPTER(prod,    prod)
NK_UNARY_ADAPTER(mean,    mean)

#undef NK_UNARY_ADAPTER

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

// max/min: two MATLAB forms:
//   max(x)    — reduction, returns (value, index)
//   max(a, b) — elementwise, single return
void max_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("max: requires at least 1 argument",
                     0, 0, "max", "", "m:max:nargin");
    if (args.size() >= 2 && !args[1].isEmpty()) {
        outs[0] = max(ctx.engine->allocator(), args[0], args[1]);
        return;
    }
    auto [val, idx] = max(ctx.engine->allocator(), args[0]);
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
    auto [val, idx] = min(ctx.engine->allocator(), args[0]);
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

// TODO: RNG state is currently process-wide (static). Known bug — multi-engine
// calls share the same sequence. Fix is to extract RngState into Engine and
// plumb through CallContext, which is a separate refactor (see project_architecture
// memory). For now we preserve the pre-migration behavior bit-for-bit.
void rand_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    static std::mt19937 gen(std::random_device{}());
    auto d = parseDimsArgs(args);
    outs[0] = rand(ctx.engine->allocator(), gen, d.rows, d.cols, d.pages);
}

void randn_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    static std::mt19937 gen(std::random_device{}());
    auto d = parseDimsArgs(args);
    outs[0] = randn(ctx.engine->allocator(), gen, d.rows, d.cols, d.pages);
}

} // namespace detail

} // namespace numkit::m::builtin
