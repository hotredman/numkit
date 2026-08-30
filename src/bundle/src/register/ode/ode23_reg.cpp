// bundle/src/register/ode/ode23_reg.cpp — Engine adapter + embedded-.m registration relocated from the
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

void ode23_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ode23: requires (f, tspan, y0[, opts])",
                    0, 0, "ode23", "", "numkit:ode23:nargin");
    auto *mr = ctx.engine->resource();
    const Value opts_v = (args.size() > 3) ? args[3] : Value::Empty;
    const Value handle = args[0];
    auto cb = [&ctx, &handle](Span<const Value> a, Span<Value> o,
                              std::pmr::memory_resource * /*mr*/) {
        Value r = ctx.engine->callFunctionHandle(handle, a);
        if (!o.empty()) o[0] = std::move(r);
    };
    auto [tv, yv] = ode23(cb, args[1], args[2], opts_v, mr);
    outs[0] = std::move(tv);
    if (nargout >= 2) outs[1] = std::move(yv);
}

} // namespace detail

// ── ode23 as an embedded `.m` wrapper (vm_callbacks_plan.md) ──────────────────
// Same pattern as ode45: the RHS `f(t,y)` runs as bytecode (pausable); the
// Bogacki-Shampine 3(2) step + adaptive control + cubic-Hermite dense output
// are the natural `.m` algorithm, vectorised so the stage arithmetic still hits
// the SIMD kernels and is bit-identical to the retained `Value ode23(...)` API.
// Split into ode23 + nk_bs23_step + nk_bs23_hermite so no single chunk exceeds
// the 255-register VM limit (see CALLBACK_PAUSABILITY.md gotchas).
static const char *kOde23MSource = R"NKM(
function [t, y] = ode23(fn, tspan, y0, opts)
  rel_tol = 1e-3; abs_tol = 1e-6; max_step = inf; initial_step = 0; refine = 1;
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
  if nspan < 2, error('numkit:ode23:tspanShort', 'ode23: tspan must have at least 2 elements'); end
  t0 = ts(1); tf = ts(nspan);
  if t0 == tf, error('numkit:ode23:tspanDegenerate', 'ode23: tspan(end) must differ from tspan(1)'); end
  if tf > t0, dir = 1; else, dir = -1; end
  for i = 2:nspan
    if (ts(i) - ts(i-1))*dir <= 0
      error('numkit:ode23:tspanMono', 'ode23: tspan must be strictly monotonic');
    end
  end
  yc = y0(:); d = numel(yc);
  if numel(abs_tol) ~= 1 && numel(abs_tol) ~= d
    error('numkit:ode23:absTolSize', 'ode23: AbsTol must be scalar or match length(y0)');
  end
  k1 = fn(t0, yc); k1 = k1(:);
  if numel(k1) ~= d, error('numkit:ode23:badRhsSize', 'ode23: RHS size mismatch'); end
  h = initial_step;
  if ~(h > 0)
    sc0 = abs_tol + rel_tol*abs(yc);
    d0 = sqrt(sum((yc ./ sc0).^2) / d);
    d1 = sqrt(sum((k1 ./ sc0).^2) / d);
    if d0 < 1e-5 || d1 < 1e-5, h0 = 1e-6; else, h0 = 0.01*d0/d1; end
    h0 = min(h0, abs(tf - t0));
    k2t = fn(t0 + dir*h0, yc + dir*h0*k1); k2t = k2t(:);
    d2 = sqrt(sum(((k2t - k1) ./ sc0).^2) / d) / h0;
    if max(d1, d2) < 1e-15, h1 = max(1e-6, h0*1e-3); else, h1 = (0.01/max(d1,d2))^(1/3); end
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
    [ynew, err_norm, k4] = nk_bs23_step(fn, tc, yc, th, dir, k1, rel_tol, abs_tol);
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
            yint = nk_bs23_hermite(cl, dir, th, yc, ynew, k1, k4);
            T = [T; tt]; Y = [Y; yint'];
          end
          next_span = next_span + 1;
        end
      elseif refine > 1
        for r = 1:(refine-1)
          theta = r/refine;
          yint = nk_bs23_hermite(theta, dir, th, yc, ynew, k1, k4);
          T = [T; t_old + dir*th*theta]; Y = [Y; yint'];
        end
        T = [T; t_new]; Y = [Y; ynew'];
      else
        T = [T; t_new]; Y = [Y; ynew'];
      end
      tc = t_new; yc = ynew; k1 = k4; failed = 0;
      if err_norm < 1e-300, fac = 5; else, fac = 0.9*(1/err_norm)^(1/3); end
      fac = min(5, max(0.2, fac));
      h = min(th*fac, max_step);
    else
      failed = failed + 1;
      fac = max(0.1, 0.9*(1/err_norm)^(1/3));
      h = th*fac;
      if failed > 10, error('numkit:ode23:tooManyFailures', 'ode23: too many step rejections'); end
      if h < abs(tc)*1e-15, error('numkit:ode23:stepUnderflow', 'ode23: step size underflow'); end
    end
  end
  if step_count >= max_steps, error('numkit:ode23:tooManySteps', 'ode23: exceeded integration steps'); end
  t = T; y = Y;
end

function [ynew, err_norm, k4] = nk_bs23_step(fn, tc, yc, h, dir, k1, rel_tol, abs_tol)
  c2 = 1/2; c3 = 3/4;
  a21 = 1/2;
  a31 = 0; a32 = 3/4;
  b1 = 2/9; b2 = 1/3; b3 = 4/9;
  e1 = 2/9 - 7/24; e2 = 1/3 - 1/4; e3 = 4/9 - 1/3; e4 = 0 - 1/8;
  k2 = fn(tc + dir*c2*h, yc + dir*h*(a21*k1)); k2 = k2(:);
  k3 = fn(tc + dir*c3*h, yc + dir*h*(a31*k1 + a32*k2)); k3 = k3(:);
  ynew = yc + dir*h*(b1*k1 + b2*k2 + b3*k3);
  k4 = fn(tc + dir*h, ynew); k4 = k4(:);
  sc = abs_tol + rel_tol*max(abs(yc), abs(ynew));
  er = dir*h*(e1*k1 + e2*k2 + e3*k3 + e4*k4);
  err_norm = sqrt(sum((er ./ sc).^2) / numel(yc));
end

function out = nk_bs23_hermite(theta, dir, h, yn, ynew, k1, k4)
  one_m_t = 1 - theta;
  t_tm1 = theta*(theta - 1);
  one_m_2t = 1 - 2*theta;
  tm1 = theta - 1;
  dy = ynew - yn;
  out = one_m_t*yn + theta*ynew + t_tm1*(one_m_2t*dy + tm1*(dir*h)*k1 + theta*(dir*h)*k4);
end
)NKM";

void registerOde23M(Engine &engine)
{
    engine.registerBuiltinMSource(kOde23MSource);
}


} // namespace numkit::ode
