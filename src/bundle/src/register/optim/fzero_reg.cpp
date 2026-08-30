// fzero_reg.cpp — Engine adapters + embedded-.m registration relocated from
// the optim toolbox in Phase D. Toolbox keeps the Engine-free FnHandle
// kernels (fzero/fminbnd/fminsearch); this core-coupled glue lives in bundle.
#include <numkit/optim/local/fzero.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>

namespace numkit::optim {

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

// ── fzero as an embedded `.m` wrapper (vm_callbacks_plan.md) ──────────────────
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

// ── fminsearch as an embedded `.m` wrapper (vm_callbacks_plan.md) ─────────────
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
