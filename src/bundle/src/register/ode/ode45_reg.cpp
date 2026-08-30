// bundle/src/register/ode/ode45_reg.cpp — Engine adapter + embedded-.m registration relocated from the
// ode toolbox in Phase D (solver 3-way split): the toolbox keeps the
// Engine-free FnHandle kernel; this core-coupled glue lives in bundle.
#include <numkit/ode/solvers.hpp>
#include <numkit/ode/options.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>

namespace numkit::ode {

// ── Engine adapter ──────────────────────────────────────────────────

namespace detail {

void ode45_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ode45: requires (f, tspan, y0[, opts])",
                    0, 0, "ode45", "", "numkit:ode45:nargin");
    auto *mr = ctx.engine->resource();
    const Value opts_v = (args.size() > 3) ? args[3] : Value::Empty;
    // Bridge the MATLAB function-handle into an Engine-free FnHandle: the
    // engine dispatches it (single output dy/dt), the library core just calls.
    const Value handle = args[0];
    auto cb = [&ctx, &handle](Span<const Value> a, Span<Value> o,
                              std::pmr::memory_resource * /*mr*/) {
        Value r = ctx.engine->callFunctionHandle(handle, a);
        if (!o.empty()) o[0] = std::move(r);
    };
    auto [tv, yv] = ode45(cb, args[1], args[2], opts_v, mr);
    outs[0] = std::move(tv);
    if (nargout >= 2) outs[1] = std::move(yv);
}

} // namespace detail

// ── ode45 as an embedded `.m` wrapper (vm_callbacks_plan.md) ──────────────────
// The registered ode45 is implemented in `.m`: the RHS `f(t,y)` is evaluated
// from bytecode (pausable), and the Dormand-Prince 5(4) step loop + adaptive
// control + Shampine dense output are the natural `.m` algorithm. A faithful,
// vectorised transcription of the C++ above — the stage/dense linear algebra is
// vector ops (`y + h*(a21*k1)`, `sum((er./sc).^2)`), so it still runs on the
// SIMD-backed kernels and is bit-identical to the retained `Value ode45(...)`
// API. (numkit doesn't bind varargin with no extra args → fixed `opts` param.)
static const char *kOde45MSource = R"NKM(
function [t, y] = ode45(fn, tspan, y0, opts)
  rel_tol = 1e-3; abs_tol = 1e-6; max_step = inf; initial_step = 0; refine = 4;
  if nargin >= 4 && isstruct(opts)
    if isfield(opts,'RelTol')      && ~isempty(opts.RelTol),      rel_tol = opts.RelTol; end
    if isfield(opts,'AbsTol')      && ~isempty(opts.AbsTol),      abs_tol = opts.AbsTol(:); end
    if isfield(opts,'MaxStep')     && ~isempty(opts.MaxStep),     max_step = opts.MaxStep; end
    if isfield(opts,'InitialStep') && ~isempty(opts.InitialStep), initial_step = opts.InitialStep; end
    if isfield(opts,'Refine')      && ~isempty(opts.Refine)
      rr = opts.Refine;
      if rr >= 1, refine = floor(rr); else, refine = 1; end
    end
  end
  ts = tspan(:); nspan = numel(ts);
  if nspan < 2, error('numkit:ode45:tspanShort', 'ode45: tspan must have at least 2 elements'); end
  t0 = ts(1); tf = ts(nspan);
  if t0 == tf, error('numkit:ode45:tspanDegenerate', 'ode45: tspan(end) must differ from tspan(1)'); end
  if tf > t0, dir = 1; else, dir = -1; end
  for i = 2:nspan
    if (ts(i) - ts(i-1))*dir <= 0
      error('numkit:ode45:tspanMono', 'ode45: tspan must be strictly monotonic');
    end
  end
  yc = y0(:); d = numel(yc);
  if numel(abs_tol) ~= 1 && numel(abs_tol) ~= d
    error('numkit:ode45:absTolSize', 'ode45: AbsTol must be scalar or match length(y0)');
  end
  k1 = fn(t0, yc); k1 = k1(:);
  if numel(k1) ~= d, error('numkit:ode45:badRhsSize', 'ode45: RHS size mismatch'); end
  h = initial_step;
  if ~(h > 0)
    sc0 = abs_tol + rel_tol*abs(yc);
    d0 = sqrt(sum((yc ./ sc0).^2) / d);
    d1 = sqrt(sum((k1 ./ sc0).^2) / d);
    if d0 < 1e-5 || d1 < 1e-5, h0 = 1e-6; else, h0 = 0.01*d0/d1; end
    h0 = min(h0, abs(tf - t0));
    k2t = fn(t0 + dir*h0, yc + dir*h0*k1); k2t = k2t(:);
    d2 = sqrt(sum(((k2t - k1) ./ sc0).^2) / d) / h0;
    if max(d1, d2) < 1e-15, h1 = max(1e-6, h0*1e-3); else, h1 = (0.01/max(d1,d2))^(1/5); end
    h = min([100*h0, h1, max_step, abs(tf - t0)]);
    if ~(h > 0), h = max(1e-6, abs(tf - t0)*1e-3); end
  end
  T = t0; Y = yc';
  emit_at_tspan = (nspan > 2);
  if emit_at_tspan, refine = 1; else, refine = max(1, refine); end
  next_span = 2;
  tc = t0; max_steps = 100000; step_count = 0; failed = 0;
  while ((dir > 0 && tc < tf) || (dir < 0 && tc > tf)) && step_count < max_steps
    step_count = step_count + 1;
    th = min(min(h, (tf - tc)*dir), max_step);
    [ynew, err_norm, k7, k3, k4, k5, k6] = nk_dopri5_step(fn, tc, yc, th, dir, k1, rel_tol, abs_tol);
    if err_norm <= 1
      t_old = tc; t_new = tc + dir*th;
      if emit_at_tspan
        while next_span <= nspan
          tt = ts(next_span);
          theta = (tt - t_old) / (dir*th);
          if theta > 1 + 1e-12, break; end
          cl = max(0, min(1, theta));
          if abs(cl - 1) < 1e-15
            T = [T; tt]; Y = [Y; ynew'];
          else
            yint = nk_ode_dense(cl, dir, th, yc, k1, k3, k4, k5, k6, k7);
            T = [T; tt]; Y = [Y; yint'];
          end
          next_span = next_span + 1;
        end
      elseif refine > 1
        for r = 1:(refine-1)
          theta = r/refine;
          yint = nk_ode_dense(theta, dir, th, yc, k1, k3, k4, k5, k6, k7);
          T = [T; t_old + dir*th*theta]; Y = [Y; yint'];
        end
        T = [T; t_new]; Y = [Y; ynew'];
      else
        T = [T; t_new]; Y = [Y; ynew'];
      end
      tc = t_new; yc = ynew; k1 = k7; failed = 0;
      if err_norm < 1e-300, fac = 5; else, fac = 0.9*(1/err_norm)^(1/5); end
      fac = min(5, max(0.2, fac));
      h = min(th*fac, max_step);
    else
      failed = failed + 1;
      fac = max(0.1, 0.9*(1/err_norm)^(1/5));
      h = th*fac;
      if failed > 10, error('numkit:ode45:tooManyFailures', 'ode45: too many step rejections'); end
      if h < abs(tc)*1e-15, error('numkit:ode45:stepUnderflow', 'ode45: step size underflow'); end
    end
  end
  if step_count >= max_steps, error('numkit:ode45:tooManySteps', 'ode45: exceeded integration steps'); end
  t = T; y = Y;
end

function [ynew, err_norm, k7, k3, k4, k5, k6] = nk_dopri5_step(fn, tc, yc, h, dir, k1, rel_tol, abs_tol)
  c2 = 1/5; c3 = 3/10; c4 = 4/5; c5 = 8/9;
  a21 = 1/5;
  a31 = 3/40; a32 = 9/40;
  a41 = 44/45; a42 = -56/15; a43 = 32/9;
  a51 = 19372/6561; a52 = -25360/2187; a53 = 64448/6561; a54 = -212/729;
  a61 = 9017/3168; a62 = -355/33; a63 = 46732/5247; a64 = 49/176; a65 = -5103/18656;
  b1 = 35/384; b3 = 500/1113; b4 = 125/192; b5 = -2187/6784; b6 = 11/84;
  e1 = 71/57600; e3 = -71/16695; e4 = 71/1920; e5 = -17253/339200; e6 = 22/525; e7 = -1/40;
  k2 = fn(tc + dir*c2*h, yc + dir*h*(a21*k1)); k2 = k2(:);
  k3 = fn(tc + dir*c3*h, yc + dir*h*(a31*k1 + a32*k2)); k3 = k3(:);
  k4 = fn(tc + dir*c4*h, yc + dir*h*(a41*k1 + a42*k2 + a43*k3)); k4 = k4(:);
  k5 = fn(tc + dir*c5*h, yc + dir*h*(a51*k1 + a52*k2 + a53*k3 + a54*k4)); k5 = k5(:);
  k6 = fn(tc + dir*h, yc + dir*h*(a61*k1 + a62*k2 + a63*k3 + a64*k4 + a65*k5)); k6 = k6(:);
  ynew = yc + dir*h*(b1*k1 + b3*k3 + b4*k4 + b5*k5 + b6*k6);
  k7 = fn(tc + dir*h, ynew); k7 = k7(:);
  sc = abs_tol + rel_tol*max(abs(yc), abs(ynew));
  er = dir*h*(e1*k1 + e3*k3 + e4*k4 + e5*k5 + e6*k6 + e7*k7);
  err_norm = sqrt(sum((er ./ sc).^2) / numel(yc));
end

function out = nk_ode_dense(theta, dir, h, yn, k1, k3, k4, k5, k6, k7)
  t2 = theta*theta; t3 = t2*theta; t4 = t3*theta;
  d1 = theta + (-183/64)*t2 + (37/12)*t3 + (-145/128)*t4;
  d3 = (1500/371)*t2 + (-1000/159)*t3 + (1000/371)*t4;
  d4 = (-125/32)*t2 + (125/12)*t3 + (-375/64)*t4;
  d5 = (9477/3392)*t2 + (-729/106)*t3 + (25515/6784)*t4;
  d6 = (-11/7)*t2 + (11/3)*t3 + (-55/28)*t4;
  d7 = (3/2)*t2 + (-4)*t3 + (5/2)*t4;
  out = yn + dir*h*(d1*k1 + d3*k3 + d4*k4 + d5*k5 + d6*k6 + d7*k7);
end
)NKM";

void registerOde45M(Engine &engine)
{
    engine.registerBuiltinMSource(kOde45MSource);
}


} // namespace numkit::ode
