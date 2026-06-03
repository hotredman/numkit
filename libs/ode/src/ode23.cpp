// libs/ode/src/ode23.cpp
//
// Explicit Bogacki-Shampine 3(2) Runge-Kutta integrator.
//
// References:
//   • Bogacki, P. & Shampine, L.F. (1989), "A 3(2) pair of Runge-Kutta
//     formulas", Appl. Math. Lett. 2(4), pp. 321-325 — the 4-stage
//     embedded pair with FSAL property used by MATLAB's ode23.
//   • Shampine, L.F. & Reichelt, M.W. (1997), "The MATLAB ODE Suite",
//     SIAM J. Sci. Comput. 18(1) — describes ode23 usage and the
//     cubic Hermite dense-output interpolant used between steps.
//   • Hairer, Nørsett, Wanner (1993), "Solving Ordinary Differential
//     Equations I" (Springer, 2nd ed.) — general adaptive step-size
//     control formula (§II.4, Eq. 4.13).

#include <numkit/ode/solvers.hpp>
#include <numkit/ode/options.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace numkit::ode {

namespace {

// Bogacki-Shampine 3(2) coefficients (Bogacki-Shampine 1989, Table 1).
constexpr double c2 = 1.0 / 2.0;
constexpr double c3 = 3.0 / 4.0;

constexpr double a21 = 1.0 / 2.0;
constexpr double a31 = 0.0;
constexpr double a32 = 3.0 / 4.0;
constexpr double a41 = 2.0 / 9.0;
constexpr double a42 = 1.0 / 3.0;
constexpr double a43 = 4.0 / 9.0;

// 3rd-order solution weights (equal to a4_).
constexpr double b1 = a41, b2 = a42, b3 = a43;
// b4 = 0 (3rd-order weight on k4 is zero — k4 is FSAL only).

// Error coefficients e_i = b_i - bhat_i where bhat is the 2nd-order
// embedded solution {7/24, 1/4, 1/3, 1/8}.
constexpr double e1 = 2.0 / 9.0 - 7.0 / 24.0;   // = -5/72
constexpr double e2 = 1.0 / 3.0 - 1.0 / 4.0;    // =  1/12
constexpr double e3 = 4.0 / 9.0 - 1.0 / 3.0;    // =  1/9
constexpr double e4 = 0.0 - 1.0 / 8.0;          // = -1/8

// Local copy of OdeOpts parser — shared design (RelTol, AbsTol, MaxStep,
// InitialStep, NormControl, Stats) plus Refine (default = 1 for ode23,
// per MATLAB R2025b).
struct OdeOpts {
    double rel_tol = 1e-3;             // MATLAB ode23 default
    std::vector<double> abs_tol{1e-6}; // MATLAB ode23 default
    double max_step = std::numeric_limits<double>::infinity();
    double initial_step = 0.0;
    int    refine = 1;                 // MATLAB ode23 default
    bool   norm_control = false;
    bool   stats = false;
};

OdeOpts read_opts(const Value &opts)
{
    OdeOpts o;
    if (opts.isEmpty() || !opts.isStruct()) return o;
    auto get_double = [&](const char *name, double def) -> double {
        if (opts.hasField(name)) {
            const Value &v = opts.field(name);
            if (!v.isEmpty()) return v.toScalar();
        }
        return def;
    };
    o.rel_tol = get_double("RelTol", 1e-3);
    o.max_step = get_double("MaxStep", std::numeric_limits<double>::infinity());
    o.initial_step = get_double("InitialStep", 0.0);
    if (opts.hasField("Refine")) {
        const Value &v = opts.field("Refine");
        if (!v.isEmpty()) {
            const double r = v.toScalar();
            o.refine = (r >= 1.0) ? static_cast<int>(r) : 1;
        }
    }
    if (opts.hasField("NormControl")) {
        const Value &v = opts.field("NormControl");
        if (!v.isEmpty()) {
            const std::string s = v.toString();
            o.norm_control = (!s.empty() && (s[0] == 'o' || s[0] == 'O'));
        }
    }
    if (opts.hasField("AbsTol")) {
        const Value &v = opts.field("AbsTol");
        if (!v.isEmpty()) {
            const std::size_t n = v.numel();
            o.abs_tol.resize(n);
            for (std::size_t i = 0; i < n; ++i) o.abs_tol[i] = v.elemAsDouble(i);
        }
    }
    if (opts.hasField("Stats")) {
        const Value &v = opts.field("Stats");
        if (!v.isEmpty()) {
            const std::string s = v.toString();
            o.stats = (!s.empty() && (s[0] == 'o' || s[0] == 'O'));
        }
    }
    return o;
}

// Cubic Hermite dense-output interpolant.
//
// For θ ∈ [0,1] (normalised step offset), interpolates between
// (t_n, y_n) with derivative k1 and (t_n + dir·h, y_new) with
// derivative k4. The interpolant is 3rd-order accurate, sufficient
// for the 3rd-order Bogacki-Shampine method (no extra k stages
// required — uses only k1 and the FSAL stage k4). Reference:
// Shampine-Reichelt 1997 — MATLAB's ode23 uses exactly this
// interpolant.
//
// y(θ) = (1-θ)·y_n + θ·y_new + θ(θ-1)·[(1-2θ)·(y_new-y_n)
//                                    + (θ-1)·h·k1 + θ·h·k4]
void hermite_interp(double theta, double dir, double h,
                    const std::vector<double> &y_n,
                    const std::vector<double> &y_new,
                    const std::vector<double> &k1,
                    const std::vector<double> &k4,
                    std::vector<double> &out)
{
    const double one_m_t = 1.0 - theta;
    const double t_tm1   = theta * (theta - 1.0);
    const double one_m_2t = 1.0 - 2.0 * theta;
    const double tm1     = theta - 1.0;
    const std::size_t d = y_n.size();
    out.resize(d);
    for (std::size_t i = 0; i < d; ++i) {
        const double dyi = y_new[i] - y_n[i];
        out[i] = one_m_t * y_n[i] + theta * y_new[i]
               + t_tm1 * (one_m_2t * dyi
                        + tm1 * (dir * h) * k1[i]
                        + theta * (dir * h) * k4[i]);
    }
}

// Evaluate the RHS callback at (t, y) → dy/dt. Same plumbing as ode45:
// the function-handle is invoked through the Engine to keep
// func-handle semantics intact (function_ref's type-erasure
// previously dropped them when round-tripping the value).
void eval_rhs(Engine &eng, const Value &fnh, double t,
              const std::vector<double> &y,
              std::vector<double> &dydt, std::pmr::memory_resource *mr)
{
    const std::size_t d = y.size();
    Value tv = Value::scalar(t, mr);
    Value yv = Value::matrix(d, 1, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < d; ++i) yv.doubleDataMut()[i] = y[i];
    std::array<Value, 2> args_buf{tv, yv};
    Value out_buf = eng.callFunctionHandle(
        fnh, Span<const Value>(args_buf.data(), 2));
    if (out_buf.numel() != d)
        throw Error("ode23: RHS returned " + std::to_string(out_buf.numel())
                  + " values but expected " + std::to_string(d),
                    0, 0, "ode23", "", "numkit:ode23:badRhsSize");
    dydt.resize(d);
    for (std::size_t i = 0; i < d; ++i) dydt[i] = out_buf.elemAsDouble(i);
}

} // anonymous

std::tuple<Value, Value>
ode23(Engine &eng, const Value &fnh, const Value &tspan, const Value &y0,
      const Value &opts, std::pmr::memory_resource *mr)
{
    // ── Validate inputs ────────────────────────────────────────────
    const std::size_t nspan = tspan.numel();
    if (nspan < 2)
        throw Error("ode23: tspan must have at least 2 elements",
                    0, 0, "ode23", "", "numkit:ode23:tspanShort");
    std::vector<double> ts(nspan);
    for (std::size_t i = 0; i < nspan; ++i) ts[i] = tspan.elemAsDouble(i);
    const double t0 = ts.front();
    const double tf = ts.back();
    if (t0 == tf)
        throw Error("ode23: tspan(end) must differ from tspan(1)",
                    0, 0, "ode23", "", "numkit:ode23:tspanDegenerate");
    const double dir = (tf > t0) ? 1.0 : -1.0;
    for (std::size_t i = 1; i < nspan; ++i) {
        if ((ts[i] - ts[i - 1]) * dir <= 0.0)
            throw Error("ode23: tspan must be strictly monotonic",
                        0, 0, "ode23", "", "numkit:ode23:tspanMono");
    }

    const std::size_t d = y0.numel();
    std::vector<double> y(d);
    for (std::size_t i = 0; i < d; ++i) y[i] = y0.elemAsDouble(i);

    OdeOpts O = read_opts(opts);
    if (O.abs_tol.size() != 1 && O.abs_tol.size() != d)
        throw Error("ode23: AbsTol must be scalar or match length(y0)",
                    0, 0, "ode23", "", "numkit:ode23:absTolSize");
    auto atol_i = [&](std::size_t i) {
        return (O.abs_tol.size() == 1) ? O.abs_tol[0] : O.abs_tol[i];
    };

    // ── Initial step size (Hairer-Nørsett-Wanner I, Eq. 4.14) ──────
    // 3rd-order method ⇒ exponent 1/3.
    std::vector<double> k1(d), k2(d), k3(d), k4(d);
    eval_rhs(eng, fnh, t0, y, k1, mr);

    double h = O.initial_step;
    if (!(h > 0.0)) {
        double d0 = 0.0, d1 = 0.0;
        for (std::size_t i = 0; i < d; ++i) {
            const double sc = atol_i(i) + O.rel_tol * std::fabs(y[i]);
            d0 += (y[i] / sc) * (y[i] / sc);
            d1 += (k1[i] / sc) * (k1[i] / sc);
        }
        d0 = std::sqrt(d0 / d); d1 = std::sqrt(d1 / d);
        double h0 = (d0 < 1e-5 || d1 < 1e-5) ? 1e-6 : 0.01 * d0 / d1;
        h0 = std::min(h0, std::fabs(tf - t0));
        std::vector<double> y1(d), k2_trial(d);
        for (std::size_t i = 0; i < d; ++i) y1[i] = y[i] + dir * h0 * k1[i];
        eval_rhs(eng, fnh, t0 + dir * h0, y1, k2_trial, mr);
        double d2 = 0.0;
        for (std::size_t i = 0; i < d; ++i) {
            const double sc = atol_i(i) + O.rel_tol * std::fabs(y[i]);
            const double diff = (k2_trial[i] - k1[i]) / sc;
            d2 += diff * diff;
        }
        d2 = std::sqrt(d2 / d) / h0;
        double h1 = (std::max(d1, d2) < 1e-15)
                  ? std::max(1e-6, h0 * 1e-3)
                  : std::pow(0.01 / std::max(d1, d2), 1.0 / 3.0);
        h = std::min({100.0 * h0, h1, O.max_step, std::fabs(tf - t0)});
        if (!(h > 0.0)) h = std::max(1e-6, std::fabs(tf - t0) * 1e-3);
    }

    // ── Output buffers ─────────────────────────────────────────────
    std::vector<double> t_out;
    std::vector<std::vector<double>> y_out;
    t_out.reserve(64); y_out.reserve(64);
    t_out.push_back(t0); y_out.push_back(y);

    const bool emit_at_tspan = (nspan > 2);
    const int refine = emit_at_tspan ? 1 : std::max(1, O.refine);
    std::size_t next_span_idx = 1;

    double t = t0;
    const int max_steps = 100000;
    int step_count = 0;
    int failed_in_a_row = 0;

    while ((dir > 0.0 ? t < tf : t > tf) && step_count < max_steps) {
        ++step_count;
        const double dist_to_end = (tf - t) * dir;
        double trial_h = std::min(h, dist_to_end);
        trial_h = std::min(trial_h, O.max_step);

        // Stages k2..k4 (k1 carried in via FSAL).
        std::vector<double> ytmp(d);
        // k2 at (t + c2 h, y + h a21 k1)
        for (std::size_t i = 0; i < d; ++i)
            ytmp[i] = y[i] + dir * trial_h * (a21 * k1[i]);
        eval_rhs(eng, fnh, t + dir * c2 * trial_h, ytmp, k2, mr);
        // k3 at (t + c3 h, y + h (a31 k1 + a32 k2))
        for (std::size_t i = 0; i < d; ++i)
            ytmp[i] = y[i] + dir * trial_h * (a31 * k1[i] + a32 * k2[i]);
        eval_rhs(eng, fnh, t + dir * c3 * trial_h, ytmp, k3, mr);
        // 3rd-order solution
        std::vector<double> y_new(d);
        for (std::size_t i = 0; i < d; ++i)
            y_new[i] = y[i] + dir * trial_h
                     * (b1 * k1[i] + b2 * k2[i] + b3 * k3[i]);
        // k4 at (t + h, y_new) — FSAL
        eval_rhs(eng, fnh, t + dir * trial_h, y_new, k4, mr);
        // Error estimate
        double err_norm = 0.0;
        for (std::size_t i = 0; i < d; ++i) {
            const double sc = atol_i(i)
                            + O.rel_tol * std::max(std::fabs(y[i]),
                                                   std::fabs(y_new[i]));
            const double er = dir * trial_h
                            * (e1 * k1[i] + e2 * k2[i]
                             + e3 * k3[i] + e4 * k4[i]);
            err_norm += (er / sc) * (er / sc);
        }
        err_norm = std::sqrt(err_norm / d);

        if (err_norm <= 1.0) {
            // ── Accept step ───────────────────────────────────────
            const double t_old = t;
            const double t_new = t + dir * trial_h;

            if (emit_at_tspan) {
                std::vector<double> yint;
                while (next_span_idx < nspan) {
                    const double tt = ts[next_span_idx];
                    const double theta = (tt - t_old) / (dir * trial_h);
                    if (theta > 1.0 + 1e-12) break;
                    const double clamped = std::max(0.0, std::min(1.0, theta));
                    if (std::fabs(clamped - 1.0) < 1e-15) {
                        t_out.push_back(tt);
                        y_out.push_back(y_new);
                    } else {
                        hermite_interp(clamped, dir, trial_h,
                                       y, y_new, k1, k4, yint);
                        t_out.push_back(tt);
                        y_out.push_back(yint);
                    }
                    ++next_span_idx;
                }
            } else if (refine > 1) {
                std::vector<double> yint;
                for (int r = 1; r < refine; ++r) {
                    const double theta = double(r) / double(refine);
                    hermite_interp(theta, dir, trial_h,
                                   y, y_new, k1, k4, yint);
                    t_out.push_back(t_old + dir * trial_h * theta);
                    y_out.push_back(yint);
                }
                t_out.push_back(t_new);
                y_out.push_back(y_new);
            } else {
                t_out.push_back(t_new);
                y_out.push_back(y_new);
            }

            t = t_new;
            y = y_new;
            k1 = k4;        // FSAL
            failed_in_a_row = 0;
            // Step size update — 3rd-order exponent.
            const double exp = 1.0 / 3.0;
            double fac;
            if (err_norm < 1e-300) fac = 5.0;
            else                   fac = 0.9 * std::pow(1.0 / err_norm, exp);
            fac = std::min(5.0, std::max(0.2, fac));
            h = trial_h * fac;
            h = std::min(h, O.max_step);
        } else {
            // ── Reject step — shrink h. ───────────────────────────
            ++failed_in_a_row;
            const double exp = 1.0 / 3.0;
            double fac = 0.9 * std::pow(1.0 / err_norm, exp);
            fac = std::max(0.1, fac);
            h = trial_h * fac;
            if (failed_in_a_row > 10)
                throw Error("ode23: 10 consecutive step rejections; "
                            "tolerances may be too tight or RHS too stiff",
                            0, 0, "ode23", "", "numkit:ode23:tooManyFailures");
            if (h < std::fabs(t) * 1e-15)
                throw Error("ode23: step size underflow",
                            0, 0, "ode23", "", "numkit:ode23:stepUnderflow");
        }
    }
    if (step_count >= max_steps)
        throw Error("ode23: exceeded " + std::to_string(max_steps)
                  + " integration steps",
                    0, 0, "ode23", "", "numkit:ode23:tooManySteps");

    // Assemble output: t (m × 1) and y (m × d).
    const std::size_t m = t_out.size();
    Value t_val = Value::matrix(m, 1, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < m; ++i) t_val.doubleDataMut()[i] = t_out[i];
    Value y_val = Value::matrix(m, d, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < m; ++i)
        for (std::size_t j = 0; j < d; ++j)
            y_val.doubleDataMut()[j * m + i] = y_out[i][j];
    return {std::move(t_val), std::move(y_val)};
}

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
    auto [tv, yv] = ode23(*ctx.engine, args[0], args[1], args[2], opts_v, mr);
    outs[0] = std::move(tv);
    if (nargout >= 2) outs[1] = std::move(yv);
}

} // namespace detail

// ── ode23 as an embedded `.m` wrapper (VM_CALLBACKS_PLAN.md) ──────────────────
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
