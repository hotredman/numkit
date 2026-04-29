// libs/builtin/src/math/optim/fzero.cpp
//
// fzero — scalar root finding via Brent's method (with outward bracket
// expansion for the x0-only form). Shares the engine-callback helper
// with integral.cpp via the inline header below.

#include <numkit/builtin/math/optim/fzero.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "../_callback_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace numkit::builtin {

namespace cb = ::numkit::builtin::detail::callback;

namespace {

// Expand a bracket around x0 by stepping outward by an increasing
// factor until a sign change is detected. Throws if not found within
// kMaxExpansions iterations.
std::pair<double, double>
findBracket(Engine *engine, const Value &fn, double x0)
{
    constexpr int kMaxExpansions = 60;
    double step = (x0 == 0.0) ? 0.02 : std::abs(x0) * 0.02;
    if (step == 0.0) step = 0.02;
    double a = x0, b = x0;
    double fa = cb::evalCallback(engine, fn, a);
    if (fa == 0.0) return {a, a};
    double fb = fa;
    for (int i = 0; i < kMaxExpansions; ++i) {
        const double s = step * std::pow(2.0, i);
        const double aPrev = a, fAprev = fa;
        a = x0 - s;
        b = x0 + s;
        fa = cb::evalCallback(engine, fn, a);
        if (fa == 0.0) return {a, a};
        fb = cb::evalCallback(engine, fn, b);
        if (fb == 0.0) return {b, b};
        if ((fa < 0) != (fb < 0)) return {a, b};
        if ((fAprev < 0) != (fa < 0)) return {a, aPrev};
    }
    throw Error("fzero: failed to find a bracket containing a sign change "
                 "near x0",
                 0, 0, "fzero", "", "m:fzero:noBracket");
}

// Brent's method on [a, b] with f(a)*f(b) < 0 (or one of them == 0).
// Returns the root.
double brent(Engine *engine, const Value &fn, double a, double b)
{
    constexpr int    kMaxIter = 200;
    constexpr double kEps     = 1e-15;

    double fa = cb::evalCallback(engine, fn, a);
    double fb = cb::evalCallback(engine, fn, b);
    if (fa == 0.0) return a;
    if (fb == 0.0) return b;
    if ((fa < 0) == (fb < 0))
        throw Error("fzero: f(a) and f(b) must have opposite signs "
                     "(no sign change in the supplied interval)",
                     0, 0, "fzero", "", "m:fzero:noSignChange");

    double c = a, fc = fa, d = b - a, e = d;
    for (int it = 0; it < kMaxIter; ++it) {
        if ((fb < 0) == (fc < 0)) {
            c = a; fc = fa; d = b - a; e = d;
        }
        if (std::abs(fc) < std::abs(fb)) {
            a = b;  b = c;  c = a;
            fa = fb; fb = fc; fc = fa;
        }
        const double tol1 = 2.0 * kEps * std::abs(b) + 0.5 * 1e-15;
        const double xm   = 0.5 * (c - b);
        if (std::abs(xm) <= tol1 || fb == 0.0) return b;

        if (std::abs(e) >= tol1 && std::abs(fa) > std::abs(fb)) {
            const double s = fb / fa;
            double p, q;
            if (a == c) {
                p = 2.0 * xm * s;
                q = 1.0 - s;
            } else {
                const double r = fb / fc;
                const double sa = fa / fc;
                p = s * (2.0 * xm * sa * (sa - r) - (b - a) * (r - 1.0));
                q = (sa - 1.0) * (r - 1.0) * (s - 1.0);
            }
            if (p > 0) q = -q;
            p = std::abs(p);
            const double min1 = 3.0 * xm * q - std::abs(tol1 * q);
            const double min2 = std::abs(e * q);
            if (2.0 * p < std::min(min1, min2)) {
                e = d;
                d = p / q;
            } else {
                d = xm; e = d;
            }
        } else {
            d = xm; e = d;
        }
        a = b; fa = fb;
        if (std::abs(d) > tol1)
            b += d;
        else
            b += (xm > 0 ? std::abs(tol1) : -std::abs(tol1));
        fb = cb::evalCallback(engine, fn, b);
    }
    throw Error("fzero: failed to converge within iteration limit",
                 0, 0, "fzero", "", "m:fzero:noConverge");
}

} // namespace

Value fzero(std::pmr::memory_resource *mr, const Value &fn, const Value &x0OrInterval,
             Engine *engine)
{
    if (engine == nullptr)
        throw Error("fzero: requires an Engine pointer (callback API)",
                     0, 0, "fzero", "", "m:fzero:noEngine");
    if (!fn.isFuncHandle()
        && !(fn.isCell() && fn.numel() >= 1 && fn.cellAt(0).isFuncHandle()))
        throw Error("fzero: 1st argument must be a function handle",
                     0, 0, "fzero", "", "m:fzero:fnType");

    if (x0OrInterval.numel() == 2) {
        const double a = x0OrInterval.elemAsDouble(0);
        const double b = x0OrInterval.elemAsDouble(1);
        if (!std::isfinite(a) || !std::isfinite(b) || a >= b)
            throw Error("fzero: interval [a, b] must satisfy a < b and be finite",
                         0, 0, "fzero", "", "m:fzero:badInterval");
        return Value::scalar(brent(engine, fn, a, b), mr);
    }

    if (!x0OrInterval.isScalar())
        throw Error("fzero: 2nd argument must be a scalar x0 or a 2-element "
                     "interval",
                     0, 0, "fzero", "", "m:fzero:badX0");
    const double x0 = x0OrInterval.toScalar();
    if (!std::isfinite(x0))
        throw Error("fzero: x0 must be finite",
                     0, 0, "fzero", "", "m:fzero:badX0");
    auto [a, b] = findBracket(engine, fn, x0);
    if (a == b) return Value::scalar(a, mr);
    if (a > b) std::swap(a, b);
    return Value::scalar(brent(engine, fn, a, b), mr);
}

// ── Engine adapter ───────────────────────────────────────────────────
namespace detail {

void fzero_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fzero: requires at least 2 arguments (fn, x0 or [a, b])",
                     0, 0, "fzero", "", "m:fzero:nargin");
    outs[0] = fzero(ctx.engine->resource(), args[0], args[1], ctx.engine);
}

} // namespace detail

} // namespace numkit::builtin
