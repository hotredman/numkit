// libs/optim/src/local/fzero.cpp
//
// fzero — scalar root finding via Brent's method (with outward bracket
// expansion for the x0-only form).
// fminbnd — bounded scalar minimum via Brent's golden-section + parabolic
// interpolation hybrid.
// fminsearch — multi-dimensional unconstrained minimum via Nelder-Mead.
//
// All three accept a numkit::FnHandle callback for objective
// evaluation — no Engine dependency in the library API. Engine
// adapters at the bottom of this TU wrap a function-handle Value in
// a stack-resident lambda and pass it as FnHandle.

#include <numkit/optim/local/fzero.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "../_callback_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace numkit::optim {

namespace cb = ::numkit::optim::detail::callback;

namespace {

// Expand a bracket around x0 by stepping outward by an increasing
// factor until a sign change is detected. Throws if not found within
// kMaxExpansions iterations.
std::pair<double, double>
findBracket(FnHandle fn, double x0, std::pmr::memory_resource *mr)
{
    constexpr int kMaxExpansions = 60;
    double step = (x0 == 0.0) ? 0.02 : std::abs(x0) * 0.02;
    if (step == 0.0) step = 0.02;
    double a = x0, b = x0;
    double fa = cb::evalScalar(fn, a, mr);
    if (fa == 0.0) return {a, a};
    double fb = fa;
    for (int i = 0; i < kMaxExpansions; ++i) {
        const double s = step * std::pow(2.0, i);
        const double aPrev = a, fAprev = fa;
        a = x0 - s;
        b = x0 + s;
        fa = cb::evalScalar(fn, a, mr);
        if (fa == 0.0) return {a, a};
        fb = cb::evalScalar(fn, b, mr);
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
double brent(FnHandle fn, double a, double b, std::pmr::memory_resource *mr)
{
    constexpr int    kMaxIter = 200;
    constexpr double kEps     = 1e-15;

    double fa = cb::evalScalar(fn, a, mr);
    double fb = cb::evalScalar(fn, b, mr);
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
        fb = cb::evalScalar(fn, b, mr);
    }
    throw Error("fzero: failed to converge within iteration limit",
                 0, 0, "fzero", "", "m:fzero:noConverge");
}

} // namespace

Value fzero(FnHandle fn, double x0, std::pmr::memory_resource *mr)
{
    if (!std::isfinite(x0))
        throw Error("fzero: x0 must be finite",
                     0, 0, "fzero", "", "m:fzero:badX0");
    auto [a, b] = findBracket(fn, x0, mr);
    if (a == b) return Value::scalar(a, mr);
    if (a > b) std::swap(a, b);
    return Value::scalar(brent(fn, a, b, mr), mr);
}

Value fzero(FnHandle fn, double a, double b,
            std::pmr::memory_resource *mr)
{
    if (!std::isfinite(a) || !std::isfinite(b) || a >= b)
        throw Error("fzero: interval [a, b] must satisfy a < b and be finite",
                     0, 0, "fzero", "", "m:fzero:badInterval");
    return Value::scalar(brent(fn, a, b, mr), mr);
}

// ── fminbnd / fminsearch ─────────────────────────────────────────────
//
// fminbnd uses Brent's variant of golden-section + parabolic
// interpolation, classic NR / SciPy-style implementation. fminsearch
// runs Nelder-Mead with the standard reflection / expansion /
// contraction / shrink coefficients.

namespace {

double brentMin(FnHandle fn, double a, double b, double tol,
                std::pmr::memory_resource *mr)
{
    constexpr int    kMaxIter = 200;
    const double GOLD = 0.5 * (3.0 - std::sqrt(5.0));   // ≈ 0.381966
    constexpr double kEps = 1e-10;

    double x = a + GOLD * (b - a);
    double w = x, v = x;
    double fx = cb::evalScalar(fn, x, mr);
    double fw = fx, fv = fx;
    double d = 0.0, e = 0.0;

    for (int it = 0; it < kMaxIter; ++it) {
        const double m  = 0.5 * (a + b);
        const double t1 = tol * std::abs(x) + kEps;
        const double t2 = 2.0 * t1;
        if (std::abs(x - m) <= (t2 - 0.5 * (b - a))) return x;

        bool gold = true;
        if (std::abs(e) > t1) {
            // Try parabolic fit through (v, w, x).
            const double r1 = (x - w) * (fx - fv);
            double q = (x - v) * (fx - fw);
            double p = (x - v) * q - (x - w) * r1;
            q = 2.0 * (q - r1);
            if (q > 0) p = -p;
            q = std::abs(q);
            const double etemp = e;
            e = d;
            if (std::abs(p) < std::abs(0.5 * q * etemp) &&
                p > q * (a - x) && p < q * (b - x)) {
                d = p / q;
                const double u = x + d;
                if ((u - a) < t2 || (b - u) < t2)
                    d = (m >= x ? t1 : -t1);
                gold = false;
            }
        }
        if (gold) {
            e = (x >= m ? a - x : b - x);
            d = GOLD * e;
        }
        const double u = (std::abs(d) >= t1) ? x + d : x + (d >= 0 ? t1 : -t1);
        const double fu = cb::evalScalar(fn, u, mr);

        if (fu <= fx) {
            if (u >= x) a = x; else b = x;
            v = w; fv = fw;
            w = x; fw = fx;
            x = u; fx = fu;
        } else {
            if (u < x) a = u; else b = u;
            if (fu <= fw || w == x) {
                v = w; fv = fw;
                w = u; fw = fu;
            } else if (fu <= fv || v == x || v == w) {
                v = u; fv = fu;
            }
        }
    }
    return x;  // best so far
}

// Nelder-Mead with the standard coefficients. `n` is the dimension;
// tol is the f-value spread tolerance. The simplex is stored as a
// flat (n+1)*n row-of-vertices buffer; sx[i*n + j] is the j-th
// coordinate of vertex i.
ScratchVec<double> nelderMead(FnHandle fn,
                              const double *x0, size_t n, double tol,
                              std::pmr::memory_resource *mr)
{
    constexpr int kMaxIter = 500;
    constexpr double ALPHA = 1.0;   // reflection
    constexpr double GAMMA = 2.0;   // expansion
    constexpr double RHO   = 0.5;   // contraction
    constexpr double SIGMA = 0.5;   // shrink

    auto evalAt = [&](const double *x) -> double {
        return cb::evalVecToScalar(fn, x, n, mr);
    };

    // Buffers (all on the per-call arena passed in via mr).
    ScratchVec<double> sx((n + 1) * n, mr);
    ScratchVec<double> fv(n + 1, mr);

    // Initial simplex: x0 + 0.05·e_i (or 0.00025 if x0_i == 0).
    for (size_t j = 0; j < n; ++j) sx[j] = x0[j];
    fv[0] = evalAt(&sx[0]);
    for (size_t i = 1; i <= n; ++i) {
        for (size_t j = 0; j < n; ++j) sx[i * n + j] = x0[j];
        const double xi = x0[i - 1];
        sx[i * n + (i - 1)] = (xi != 0.0 ? 1.05 * xi : 0.00025);
        fv[i] = evalAt(&sx[i * n]);
    }

    ScratchVec<size_t> ord(n + 1, mr);
    ScratchVec<double> newSx((n + 1) * n, mr);
    ScratchVec<double> newFv(n + 1, mr);
    ScratchVec<double> centroid(n, mr);
    ScratchVec<double> xr(n, mr);
    ScratchVec<double> xe(n, mr);
    ScratchVec<double> xc(n, mr);

    for (int it = 0; it < kMaxIter; ++it) {
        // Sort vertices by fv ascending.
        for (size_t i = 0; i <= n; ++i) ord[i] = i;
        std::sort(ord.begin(), ord.end(),
                  [&](size_t a, size_t b) { return fv[a] < fv[b]; });

        for (size_t i = 0; i <= n; ++i) {
            const double *src = &sx[ord[i] * n];
            for (size_t j = 0; j < n; ++j) newSx[i * n + j] = src[j];
            newFv[i] = fv[ord[i]];
        }
        std::copy(newSx.begin(), newSx.end(), sx.begin());
        std::copy(newFv.begin(), newFv.end(), fv.begin());

        const double spread = fv[n] - fv[0];
        if (spread <= tol * std::max(1.0, std::abs(fv[0]))) break;

        for (size_t j = 0; j < n; ++j) centroid[j] = 0.0;
        for (size_t i = 0; i < n; ++i)
            for (size_t j = 0; j < n; ++j)
                centroid[j] += sx[i * n + j];
        for (size_t j = 0; j < n; ++j) centroid[j] /= static_cast<double>(n);

        for (size_t j = 0; j < n; ++j)
            xr[j] = centroid[j] + ALPHA * (centroid[j] - sx[n * n + j]);
        const double fxr = evalAt(xr.data());

        if (fxr < fv[0]) {
            for (size_t j = 0; j < n; ++j)
                xe[j] = centroid[j] + GAMMA * (xr[j] - centroid[j]);
            const double fxe = evalAt(xe.data());
            if (fxe < fxr) {
                for (size_t j = 0; j < n; ++j) sx[n * n + j] = xe[j];
                fv[n] = fxe;
            } else {
                for (size_t j = 0; j < n; ++j) sx[n * n + j] = xr[j];
                fv[n] = fxr;
            }
        } else if (fxr < fv[n - 1]) {
            for (size_t j = 0; j < n; ++j) sx[n * n + j] = xr[j];
            fv[n] = fxr;
        } else {
            const bool outside = fxr < fv[n];
            const double *base = outside ? xr.data() : &sx[n * n];
            for (size_t j = 0; j < n; ++j)
                xc[j] = centroid[j] + RHO * (base[j] - centroid[j]);
            const double fxc = evalAt(xc.data());
            const double fcompare = outside ? fxr : fv[n];
            if (fxc <= fcompare) {
                for (size_t j = 0; j < n; ++j) sx[n * n + j] = xc[j];
                fv[n] = fxc;
            } else {
                for (size_t i = 1; i <= n; ++i) {
                    for (size_t j = 0; j < n; ++j)
                        sx[i * n + j] = sx[j] + SIGMA * (sx[i * n + j] - sx[j]);
                    fv[i] = evalAt(&sx[i * n]);
                }
            }
        }
    }

    ScratchVec<double> best(n, mr);
    for (size_t j = 0; j < n; ++j) best[j] = sx[j];
    return best;
}

} // anon

Value fminbnd(FnHandle fn, double lo, double hi, double tol,
              std::pmr::memory_resource *mr)
{
    if (!std::isfinite(lo) || !std::isfinite(hi) || lo >= hi)
        throw Error("fminbnd: lo < hi must be finite",
                     0, 0, "fminbnd", "", "m:fminbnd:badRange");
    if (!(tol > 0)) tol = 1e-6;
    return Value::scalar(brentMin(fn, lo, hi, tol, mr), mr);
}

Value fminsearch(FnHandle fn, Span<const double> x0, double tol,
                 std::pmr::memory_resource *mr)
{
    const size_t n = x0.size();
    if (n == 0)
        throw Error("fminsearch: x0 must be non-empty",
                     0, 0, "fminsearch", "", "m:fminsearch:badX0");
    if (!(tol > 0)) tol = 1e-4;
    ScratchArena scratch(mr);
    auto best = nelderMead(fn, x0.data(), n, tol, &scratch);
    Value r = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < n; ++i) r.doubleDataMut()[i] = best[i];
    return r;
}

// ── Engine adapter ───────────────────────────────────────────────────
//
// Each adapter validates that the user passed a function handle, then
// wraps the (Engine, handle) pair into a stack-resident lambda that
// looks like a numkit::FnHandle to the library functions. The lambda
// outlives the call into fzero/fminbnd/fminsearch.
namespace detail {

namespace {
// Validate args[0] is a callable handle (handle or cell-of-handle).
void requireFuncHandle(const Value &fn, const char *who)
{
    if (!fn.isFuncHandle()
        && !(fn.isCell() && fn.numel() >= 1 && fn.cellAt(0).isFuncHandle()))
        throw Error(std::string(who) + ": 1st argument must be a function handle",
                     0, 0, who, "", std::string("m:") + who + ":fnType");
}
} // anon

void fzero_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fzero: requires at least 2 arguments (fn, x0 or [a, b])",
                     0, 0, "fzero", "", "m:fzero:nargin");
    requireFuncHandle(args[0], "fzero");
    auto handle = args[0];
    auto cb = [&ctx, &handle](Span<const Value> ar, Span<Value> ou,
                               std::pmr::memory_resource * /*mr*/) {
        auto r = ctx.engine->callFunctionHandleMulti(handle, ar, ou.size());
        for (size_t i = 0; i < ou.size() && i < r.size(); ++i)
            ou[i] = std::move(r[i]);
    };
    auto *mr = ctx.engine->resource();
    const Value &v = args[1];
    if (v.numel() == 1) {
        outs[0] = fzero(cb, v.toScalar(), mr);
    } else if (v.numel() == 2) {
        outs[0] = fzero(cb, v.elemAsDouble(0), v.elemAsDouble(1), mr);
    } else {
        throw Error("fzero: 2nd argument must be a scalar x0 or a 2-element "
                     "interval [a, b]",
                     0, 0, "fzero", "", "m:fzero:badX0");
    }
}

void fminbnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("fminbnd: requires (fn, lo, hi[, tol])",
                     0, 0, "fminbnd", "", "m:fminbnd:nargin");
    requireFuncHandle(args[0], "fminbnd");
    const double lo = args[1].toScalar();
    const double hi = args[2].toScalar();
    const double tol = (args.size() >= 4 && !args[3].isEmpty()) ? args[3].toScalar() : 1e-6;
    auto handle = args[0];
    auto cb = [&ctx, &handle](Span<const Value> ar, Span<Value> ou,
                               std::pmr::memory_resource * /*mr*/) {
        auto r = ctx.engine->callFunctionHandleMulti(handle, ar, ou.size());
        for (size_t i = 0; i < ou.size() && i < r.size(); ++i)
            ou[i] = std::move(r[i]);
    };
    outs[0] = fminbnd(cb, lo, hi, tol, ctx.engine->resource());
}

void fminsearch_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fminsearch: requires (fn, x0[, tol])",
                     0, 0, "fminsearch", "", "m:fminsearch:nargin");
    requireFuncHandle(args[0], "fminsearch");
    const double tol = (args.size() >= 3 && !args[2].isEmpty()) ? args[2].toScalar() : 1e-4;
    auto handle = args[0];
    auto cb = [&ctx, &handle](Span<const Value> ar, Span<Value> ou,
                               std::pmr::memory_resource * /*mr*/) {
        auto r = ctx.engine->callFunctionHandleMulti(handle, ar, ou.size());
        for (size_t i = 0; i < ou.size() && i < r.size(); ++i)
            ou[i] = std::move(r[i]);
    };
    auto *mr = ctx.engine->resource();

    // Extract x0 into a temporary double buffer; the library API takes
    // Span<const double>. Buffer lives until the fminsearch call returns.
    const Value &x0v = args[1];
    const size_t n = x0v.numel();
    ScratchArena scratch(mr);
    ScratchVec<double> x0buf(n, &scratch);
    for (size_t i = 0; i < n; ++i) x0buf[i] = x0v.elemAsDouble(i);

    Value r = fminsearch(cb, Span<const double>(x0buf.data(), n), tol, mr);

    // MATLAB convention: output shape mirrors input. Library returns
    // a column; if the user passed a row, copy into a row Value.
    if (x0v.dims().rows() == 1 && x0v.dims().cols() >= 1) {
        Value row = Value::matrix(1, n, ValueType::DOUBLE, mr);
        for (size_t i = 0; i < n; ++i)
            row.doubleDataMut()[i] = r.doubleData()[i];
        r = std::move(row);
    }
    outs[0] = std::move(r);
}

} // namespace detail

} // namespace numkit::optim
