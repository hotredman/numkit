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
                 0, 0, "fzero", "", "numkit:fzero:noBracket");
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
                     0, 0, "fzero", "", "numkit:fzero:noSignChange");

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
                 0, 0, "fzero", "", "numkit:fzero:noConverge");
}

} // namespace

Value fzero(FnHandle fn, double x0, std::pmr::memory_resource *mr)
{
    if (!std::isfinite(x0))
        throw Error("fzero: x0 must be finite",
                     0, 0, "fzero", "", "numkit:fzero:badX0");
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
                     0, 0, "fzero", "", "numkit:fzero:badInterval");
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

        // MATLAB convergence requires BOTH the function-value spread (TolFun)
        // AND the simplex size relative to the best vertex (TolX) to be within
        // tol. Previously only the f-spread was checked, so on a smooth
        // objective the simplex stopped ~100x short of the true minimum.
        const double fspread = fv[n] - fv[0];
        double xspread = 0.0;
        for (size_t i = 1; i <= n; ++i)
            for (size_t j = 0; j < n; ++j)
                xspread = std::max(xspread, std::abs(sx[i * n + j] - sx[j]));
        if (fspread <= tol && xspread <= tol) break;

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
                     0, 0, "fminbnd", "", "numkit:fminbnd:badRange");
    if (!(tol > 0)) tol = 1e-6;
    return Value::scalar(brentMin(fn, lo, hi, tol, mr), mr);
}

Value fminsearch(FnHandle fn, Span<const double> x0, double tol,
                 std::pmr::memory_resource *mr)
{
    const size_t n = x0.size();
    if (n == 0)
        throw Error("fminsearch: x0 must be non-empty",
                     0, 0, "fminsearch", "", "numkit:fminsearch:badX0");
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
                     0, 0, who, "", std::string("numkit:") + who + ":fnType");
}

// Evaluate the objective at a point (scalar or vector Value) → scalar.
double evalHandleScalar(CallContext &ctx, const Value &handle, const Value &pt)
{
    Value a[1] = { pt };
    auto r = ctx.engine->callFunctionHandleMulti(handle, Span<const Value>(a, 1), 1);
    return r.empty() ? std::nan("") : r[0].toScalar();
}

// Emit the optional fval / exitflag outputs (and reject the not-yet-
// supported `output` struct). `x` is the result point, `who` the fn name.
// Reaching here means the solver converged (it throws otherwise) → the
// MATLAB exit flag is 1.
void emitFvalExitflag(CallContext &ctx, const Value &handle, const Value &x,
                      Span<Value> outs, const char *who)
{
    auto *mr = ctx.engine->resource();
    if (outs.size() >= 2)
        outs[1] = Value::scalar(evalHandleScalar(ctx, handle, x), mr);
    if (outs.size() >= 3)
        outs[2] = Value::scalar(1.0, mr);   // converged
    if (outs.size() >= 4)
        throw Error(std::string(who) + ": the 4th output (output struct) is "
                     "not yet supported (use [x, fval, exitflag])",
                     0, 0, who, "", std::string("numkit:") + who + ":outputStruct");
}
} // anon

void fzero_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fzero: requires at least 2 arguments (fn, x0 or [a, b])",
                     0, 0, "fzero", "", "numkit:fzero:nargin");
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
                     0, 0, "fzero", "", "numkit:fzero:badX0");
    }
    emitFvalExitflag(ctx, args[0], outs[0], outs, "fzero");
}

void fminbnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("fminbnd: requires (fn, lo, hi[, tol])",
                     0, 0, "fminbnd", "", "numkit:fminbnd:nargin");
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
    emitFvalExitflag(ctx, args[0], outs[0], outs, "fminbnd");
}

void fminsearch_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fminsearch: requires (fn, x0[, tol])",
                     0, 0, "fminsearch", "", "numkit:fminsearch:nargin");
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
    emitFvalExitflag(ctx, args[0], outs[0], outs, "fminsearch");
}

} // namespace detail

// ── fzero as an embedded `.m` wrapper (VM_CALLBACKS_PLAN.md) ──────────────────
// The registered, user-facing fzero is implemented in `.m` so the objective `f`
// is called from bytecode (CALL_INDIRECT) — pausable under the debugger for
// free, no C++ state machine. A faithful port of findBracket + brent above; the
// C++ `Value fzero(...)` API stays as the synchronous path for embedders.
static const char *kFzeroMSource = R"NKM(
function [x, fval, exitflag] = fzero(fn, x0)
  if ~(strcmp(class(fn), 'function_handle') || iscell(fn))
    error('numkit:fzero:notHandle', 'fzero: first argument must be a function handle');
  end
  if numel(x0) == 1
    [a, b] = nk_fzero_bracket(fn, x0);
    if a == b
      x = a;
    else
      if a > b
        t = a; a = b; b = t;
      end
      x = nk_fzero_brent(fn, a, b);
    end
  elseif numel(x0) == 2
    a = x0(1); b = x0(2);
    if ~(isfinite(a) && isfinite(b)) || a >= b
      error('numkit:fzero:badInterval', 'fzero: interval [a, b] must satisfy a < b and be finite');
    end
    x = nk_fzero_brent(fn, a, b);
  else
    error('numkit:fzero:badX0', 'fzero: 2nd argument must be a scalar x0 or a 2-element interval [a, b]');
  end
  fval = fn(x);
  exitflag = 1;
end

function [a, b] = nk_fzero_bracket(fn, x0)
  if ~isfinite(x0)
    error('numkit:fzero:badX0', 'fzero: x0 must be finite');
  end
  if x0 == 0
    step = 0.02;
  else
    step = abs(x0) * 0.02;
  end
  if step == 0
    step = 0.02;
  end
  a = x0; b = x0;
  fa = fn(a);
  if fa == 0
    b = a; return;
  end
  for i = 0:59
    s = step * 2^i;
    aprev = a; faprev = fa;
    a = x0 - s;
    b = x0 + s;
    fa = fn(a);
    if fa == 0
      b = a; return;
    end
    fb = fn(b);
    if fb == 0
      a = b; return;
    end
    if (fa < 0) ~= (fb < 0)
      return;
    end
    if (faprev < 0) ~= (fa < 0)
      b = aprev; return;
    end
  end
  error('numkit:fzero:noBracket', 'fzero: failed to find a bracket containing a sign change near x0');
end

function r = nk_fzero_brent(fn, a, b)
  ep = 1e-15;
  fa = fn(a); fb = fn(b);
  if fa == 0, r = a; return; end
  if fb == 0, r = b; return; end
  if (fa < 0) == (fb < 0)
    error('numkit:fzero:noSignChange', 'fzero: f(a) and f(b) must have opposite signs (no sign change in the supplied interval)');
  end
  c = a; fc = fa; d = b - a; e = d;
  for it = 1:200
    if (fb < 0) == (fc < 0)
      c = a; fc = fa; d = b - a; e = d;
    end
    if abs(fc) < abs(fb)
      a = b; b = c; c = a;
      fa = fb; fb = fc; fc = fa;
    end
    tol1 = 2*ep*abs(b) + 0.5e-15;
    xm = 0.5*(c - b);
    if abs(xm) <= tol1 || fb == 0
      r = b; return;
    end
    if abs(e) >= tol1 && abs(fa) > abs(fb)
      s = fb/fa;
      if a == c
        p = 2*xm*s; q = 1 - s;
      else
        rr = fb/fc; sa = fa/fc;
        p = s*(2*xm*sa*(sa - rr) - (b - a)*(rr - 1));
        q = (sa - 1)*(rr - 1)*(s - 1);
      end
      if p > 0, q = -q; end
      p = abs(p);
      mm1 = 3*xm*q - abs(tol1*q);
      mm2 = abs(e*q);
      if 2*p < min(mm1, mm2)
        e = d; d = p/q;
      else
        d = xm; e = d;
      end
    else
      d = xm; e = d;
    end
    a = b; fa = fb;
    if abs(d) > tol1
      b = b + d;
    else
      if xm > 0
        b = b + abs(tol1);
      else
        b = b - abs(tol1);
      end
    end
    fb = fn(b);
  end
  error('numkit:fzero:noConverge', 'fzero: failed to converge within iteration limit');
end
)NKM";

void registerFzeroM(Engine &engine)
{
    engine.registerBuiltinMSource(kFzeroMSource);
}

// ── fminsearch as an embedded `.m` wrapper (VM_CALLBACKS_PLAN.md) ─────────────
// The objective is always user code, so fminsearch takes the `.m` path: the
// Nelder-Mead simplex search is the natural `.m` algorithm and every objective
// evaluation `fn(x)` runs as bytecode → pausable under the debugger. A faithful
// transcription of the C++ `nelderMead` above (same reflection/expansion/
// contraction/shrink constants, same dual TolFun+TolX convergence, same simplex
// seeding, kMaxIter=500). The point is always passed to `fn` as a 1×n row, like
// the C++ `evalVecToScalar`. The C++ `Value fminsearch(...)` API is retained.
// Split into fminsearch + nk_nelder_mead + nk_nm_eval so no chunk approaches the
// 255-register VM limit (CALLBACK_PAUSABILITY.md gotcha).
static const char *kFminsearchMSource = R"NKM(
function [x, fval, exitflag] = fminsearch(fn, x0, tol)
  if ~(strcmp(class(fn), 'function_handle') || iscell(fn))
    error('numkit:fminsearch:fnType', 'fminsearch: 1st argument must be a function handle');
  end
  n = numel(x0);
  if n == 0
    error('numkit:fminsearch:badX0', 'fminsearch: x0 must be non-empty');
  end
  if nargin < 3 || isempty(tol) || ~(tol > 0)
    tol = 1e-4;
  end
  best = nk_nelder_mead(fn, reshape(x0, 1, n), n, tol);
  if size(x0, 1) == 1
    x = best;
  else
    x = best';
  end
  fval = nk_nm_eval(fn, x);
  exitflag = 1;
end

function s = nk_nm_eval(fn, pt)
  v = fn(pt);
  s = v(1);
end

function best = nk_nelder_mead(fn, x0, n, tol)
  refl = 1; expd = 2; conr = 0.5; shrk = 0.5; max_iter = 500;
  S = zeros(n+1, n); fv = zeros(n+1, 1);
  S(1,:) = x0; fv(1) = nk_nm_eval(fn, x0);
  for i = 2:(n+1)
    S(i,:) = x0;
    xi = x0(i-1);
    if xi ~= 0, S(i, i-1) = 1.05*xi; else, S(i, i-1) = 0.00025; end
    fv(i) = nk_nm_eval(fn, S(i,:));
  end
  for it = 1:max_iter
    [fv, ord] = sort(fv);
    S = S(ord, :);
    fspread = fv(n+1) - fv(1);
    df = abs(S(2:(n+1),:) - S(1,:));
    xspread = max(df(:));
    if fspread <= tol && xspread <= tol, break; end
    centroid = sum(S(1:n,:), 1) / n;
    worst = S(n+1,:);
    xr = centroid + refl*(centroid - worst);
    fxr = nk_nm_eval(fn, xr);
    if fxr < fv(1)
      xe = centroid + expd*(xr - centroid);
      fxe = nk_nm_eval(fn, xe);
      if fxe < fxr
        S(n+1,:) = xe; fv(n+1) = fxe;
      else
        S(n+1,:) = xr; fv(n+1) = fxr;
      end
    elseif fxr < fv(n)
      S(n+1,:) = xr; fv(n+1) = fxr;
    else
      outside = fxr < fv(n+1);
      if outside, base = xr; else, base = S(n+1,:); end
      xc = centroid + conr*(base - centroid);
      fxc = nk_nm_eval(fn, xc);
      if outside, fcmp = fxr; else, fcmp = fv(n+1); end
      if fxc <= fcmp
        S(n+1,:) = xc; fv(n+1) = fxc;
      else
        for i = 2:(n+1)
          S(i,:) = S(1,:) + shrk*(S(i,:) - S(1,:));
          fv(i) = nk_nm_eval(fn, S(i,:));
        end
      end
    end
  end
  best = S(1,:);
end
)NKM";

void registerFminsearchM(Engine &engine)
{
    engine.registerBuiltinMSource(kFminsearchMSource);
}

} // namespace numkit::optim
