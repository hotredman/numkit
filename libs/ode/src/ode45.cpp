// libs/ode/src/ode45.cpp
//
// Explicit Dormand-Prince 5(4) Runge-Kutta integrator.
//
// References:
//   • Hairer, Nørsett, Wanner (1993), "Solving Ordinary Differential
//     Equations I", §II.5 (DOPRI5 tableau, p. 178; PI step controller,
//     p. 167).
//   • Dormand & Prince (1980), "A family of embedded Runge-Kutta
//     formulae", J. Comput. Appl. Math. 6(1), pp. 19-26.
//   • Shampine & Reichelt (1997), "The MATLAB ODE Suite", §3.

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

// Dormand-Prince 5(4) coefficients (Hairer-Nørsett-Wanner Vol I, p. 178).
constexpr double c2 = 1.0 / 5.0;
constexpr double c3 = 3.0 / 10.0;
constexpr double c4 = 4.0 / 5.0;
constexpr double c5 = 8.0 / 9.0;

constexpr double a21 = 1.0 / 5.0;
constexpr double a31 = 3.0 / 40.0,   a32 = 9.0 / 40.0;
constexpr double a41 = 44.0 / 45.0,  a42 = -56.0 / 15.0, a43 = 32.0 / 9.0;
constexpr double a51 = 19372.0 / 6561.0,  a52 = -25360.0 / 2187.0;
constexpr double a53 = 64448.0 / 6561.0,  a54 = -212.0 / 729.0;
constexpr double a61 = 9017.0 / 3168.0,   a62 = -355.0 / 33.0;
constexpr double a63 = 46732.0 / 5247.0,  a64 = 49.0 / 176.0;
constexpr double a65 = -5103.0 / 18656.0;
constexpr double a71 = 35.0 / 384.0,      a73 = 500.0 / 1113.0;
constexpr double a74 = 125.0 / 192.0,     a75 = -2187.0 / 6784.0;
constexpr double a76 = 11.0 / 84.0;

// 5th-order solution weights (b = a7*).
constexpr double b1 = a71, b3 = a73, b4 = a74, b5 = a75, b6 = a76;

// Embedded 4th-order minus 5th-order (used for error estimation):
//   e_i = b_i - b_hat_i
constexpr double e1 = 71.0  / 57600.0;
constexpr double e3 = -71.0 / 16695.0;
constexpr double e4 = 71.0  / 1920.0;
constexpr double e5 = -17253.0 / 339200.0;
constexpr double e6 = 22.0  / 525.0;
constexpr double e7 = -1.0  / 40.0;

// Helpers: parse opts (RelTol, AbsTol can be vectors).
struct OdeOpts {
    double rel_tol = 1e-3;             // MATLAB ode45 default
    std::vector<double> abs_tol{1e-6}; // MATLAB ode45 default, scalar
    double max_step = std::numeric_limits<double>::infinity();
    double initial_step = 0.0;         // 0 means auto-pick
    int    refine = 4;                 // MATLAB ode45 default
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

// ── Shampine's free 4th-order dense output for DOPRI5 ───────────────
//
// Reference: Shampine, L.F. (1986). "Some practical Runge-Kutta
// formulas". Math. Comp. 46(173): 135-150 — §3 gives the interpolation
// polynomial used by MATLAB's ode45 (see also Hairer-Nørsett-Wanner
// Vol I, §II.6). The interpolant is "free" (reuses the 7 k-stages
// already computed by the step) and is 4th-order accurate.
//
// For θ ∈ [0,1] (normalised offset within the accepted step), it
// returns the polynomial bstar_i(θ) so that
//   y(t_n + θ·h) ≈ y_n + h · Σ bstar_i(θ) · k_i
//
// Verified: bstar_i(1) = b_i (the 5th-order solution weights), so the
// interpolant agrees with the step's endpoint by construction.
struct DenseCoeffs {
    double b1, b3, b4, b5, b6, b7;
};

inline DenseCoeffs dense_coeffs(double theta)
{
    const double t2 = theta * theta;
    const double t3 = t2 * theta;
    const double t4 = t3 * theta;
    DenseCoeffs c;
    c.b1 = theta + (-183.0 / 64.0) * t2 + (37.0 / 12.0) * t3
         + (-145.0 / 128.0) * t4;
    c.b3 = (1500.0 / 371.0) * t2 + (-1000.0 / 159.0) * t3
         + (1000.0 / 371.0) * t4;
    c.b4 = (-125.0 / 32.0) * t2 + (125.0 / 12.0) * t3
         + (-375.0 / 64.0) * t4;
    c.b5 = (9477.0 / 3392.0) * t2 + (-729.0 / 106.0) * t3
         + (25515.0 / 6784.0) * t4;
    c.b6 = (-11.0 / 7.0) * t2 + (11.0 / 3.0) * t3
         + (-55.0 / 28.0) * t4;
    c.b7 = (3.0 / 2.0) * t2 + (-4.0) * t3 + (5.0 / 2.0) * t4;
    return c;
}

// Evaluate dense output at θ ∈ [0,1] within the accepted step
// (t_n, t_n + dir·h, k1..k7).
void dense_eval(double theta, double dir, double h,
                const std::vector<double> &y_n,
                const std::vector<double> &k1, const std::vector<double> &k3,
                const std::vector<double> &k4, const std::vector<double> &k5,
                const std::vector<double> &k6, const std::vector<double> &k7,
                std::vector<double> &out)
{
    const DenseCoeffs c = dense_coeffs(theta);
    const std::size_t d = y_n.size();
    out.resize(d);
    for (std::size_t i = 0; i < d; ++i) {
        out[i] = y_n[i] + dir * h
               * (c.b1 * k1[i] + c.b3 * k3[i] + c.b4 * k4[i]
                + c.b5 * k5[i] + c.b6 * k6[i] + c.b7 * k7[i]);
    }
}

// Evaluate the RHS callback at (t, y) → dy/dt (length d). Uses the
// Engine to call the function-handle Value directly (FnHandle-based
// path had brittle capture semantics when the Engine round-trips back
// into the lambda).
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
        throw Error("ode45: RHS returned " + std::to_string(out_buf.numel())
                  + " values but expected " + std::to_string(d),
                    0, 0, "ode45", "", "numkit:ode45:badRhsSize");
    dydt.resize(d);
    for (std::size_t i = 0; i < d; ++i) dydt[i] = out_buf.elemAsDouble(i);
}

} // anonymous

std::tuple<Value, Value>
ode45(Engine &eng, const Value &fnh, const Value &tspan, const Value &y0,
      const Value &opts, std::pmr::memory_resource *mr)
{
    // ── Validate inputs ────────────────────────────────────────────
    const std::size_t nspan = tspan.numel();
    if (nspan < 2)
        throw Error("ode45: tspan must have at least 2 elements",
                    0, 0, "ode45", "", "numkit:ode45:tspanShort");
    std::vector<double> ts(nspan);
    for (std::size_t i = 0; i < nspan; ++i) ts[i] = tspan.elemAsDouble(i);
    const double t0 = ts.front();
    const double tf = ts.back();
    if (t0 == tf)
        throw Error("ode45: tspan(end) must differ from tspan(1)",
                    0, 0, "ode45", "", "numkit:ode45:tspanDegenerate");
    const double dir = (tf > t0) ? 1.0 : -1.0;
    for (std::size_t i = 1; i < nspan; ++i) {
        if ((ts[i] - ts[i - 1]) * dir <= 0.0)
            throw Error("ode45: tspan must be strictly monotonic",
                        0, 0, "ode45", "", "numkit:ode45:tspanMono");
    }

    const std::size_t d = y0.numel();
    std::vector<double> y(d);
    for (std::size_t i = 0; i < d; ++i) y[i] = y0.elemAsDouble(i);

    OdeOpts O = read_opts(opts);
    if (O.abs_tol.size() != 1 && O.abs_tol.size() != d)
        throw Error("ode45: AbsTol must be scalar or match length(y0)",
                    0, 0, "ode45", "", "numkit:ode45:absTolSize");
    auto atol_i = [&](std::size_t i) {
        return (O.abs_tol.size() == 1) ? O.abs_tol[0] : O.abs_tol[i];
    };

    // ── Initial step size (Hairer-Nørsett-Wanner I, Eq. (4.14)) ────
    std::vector<double> k1(d), k2(d), k3(d), k4(d), k5(d), k6(d), k7(d);
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
        // Trial step.
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
                  : std::pow(0.01 / std::max(d1, d2), 1.0 / 5.0);
        h = std::min({100.0 * h0, h1, O.max_step, std::fabs(tf - t0)});
        if (!(h > 0.0)) h = std::max(1e-6, std::fabs(tf - t0) * 1e-3);
    }

    // ── Output buffers ─────────────────────────────────────────────
    std::vector<double> t_out;
    std::vector<std::vector<double>> y_out;   // each entry length d
    t_out.reserve(64); y_out.reserve(64);
    t_out.push_back(t0); y_out.push_back(y);

    // For explicit tspan (nspan > 2), we emit only at the requested
    // points (no Refine). For [t0 tf], we emit every accepted step
    // plus (Refine-1) interpolated points per step (MATLAB default
    // Refine = 4).
    const bool emit_at_tspan = (nspan > 2);
    const int refine = emit_at_tspan ? 1 : std::max(1, O.refine);
    std::size_t next_span_idx = 1;    // next target tspan index

    double t = t0;
    const int max_steps = 100000;
    int step_count = 0;
    int failed_in_a_row = 0;

    while ((dir > 0.0 ? t < tf : t > tf) && step_count < max_steps) {
        ++step_count;
        // Cap trial step at end of integration interval (so we never
        // overshoot tf) and at MaxStep. We do NOT cap at intermediate
        // tspan targets — dense output handles those.
        const double dist_to_end = (tf - t) * dir;
        double trial_h = std::min(h, dist_to_end);
        trial_h = std::min(trial_h, O.max_step);

        // Stages k2..k7 with k1 carried in (FSAL).
        std::vector<double> ytmp(d);
        // k2 at (t + c2 h, y + h a21 k1)
        for (std::size_t i = 0; i < d; ++i)
            ytmp[i] = y[i] + dir * trial_h * (a21 * k1[i]);
        eval_rhs(eng, fnh, t + dir * c2 * trial_h, ytmp, k2, mr);
        // k3 at (t + c3 h, y + h (a31 k1 + a32 k2))
        for (std::size_t i = 0; i < d; ++i)
            ytmp[i] = y[i] + dir * trial_h * (a31 * k1[i] + a32 * k2[i]);
        eval_rhs(eng, fnh, t + dir * c3 * trial_h, ytmp, k3, mr);
        // k4
        for (std::size_t i = 0; i < d; ++i)
            ytmp[i] = y[i] + dir * trial_h
                    * (a41 * k1[i] + a42 * k2[i] + a43 * k3[i]);
        eval_rhs(eng, fnh, t + dir * c4 * trial_h, ytmp, k4, mr);
        // k5
        for (std::size_t i = 0; i < d; ++i)
            ytmp[i] = y[i] + dir * trial_h
                    * (a51 * k1[i] + a52 * k2[i] + a53 * k3[i] + a54 * k4[i]);
        eval_rhs(eng, fnh, t + dir * c5 * trial_h, ytmp, k5, mr);
        // k6
        for (std::size_t i = 0; i < d; ++i)
            ytmp[i] = y[i] + dir * trial_h
                    * (a61 * k1[i] + a62 * k2[i] + a63 * k3[i]
                     + a64 * k4[i] + a65 * k5[i]);
        eval_rhs(eng, fnh, t + dir * trial_h, ytmp, k6, mr);
        // 5th-order solution
        std::vector<double> y_new(d);
        for (std::size_t i = 0; i < d; ++i)
            y_new[i] = y[i] + dir * trial_h
                     * (b1 * k1[i] + b3 * k3[i] + b4 * k4[i]
                      + b5 * k5[i] + b6 * k6[i]);
        // k7 at (t + h, y_new) — FSAL
        eval_rhs(eng, fnh, t + dir * trial_h, y_new, k7, mr);
        // Error estimate
        double err_norm = 0.0;
        for (std::size_t i = 0; i < d; ++i) {
            const double sc = atol_i(i)
                            + O.rel_tol * std::max(std::fabs(y[i]),
                                                   std::fabs(y_new[i]));
            const double er = dir * trial_h
                            * (e1 * k1[i] + e3 * k3[i] + e4 * k4[i]
                             + e5 * k5[i] + e6 * k6[i] + e7 * k7[i]);
            err_norm += (er / sc) * (er / sc);
        }
        err_norm = std::sqrt(err_norm / d);

        if (err_norm <= 1.0) {
            // ── Accept step ───────────────────────────────────────
            const double t_old = t;
            const double t_new = t + dir * trial_h;

            if (emit_at_tspan) {
                // Interpolate via dense output AT every requested ts
                // that lies in (t_old, t_new].
                std::vector<double> yint;
                while (next_span_idx < nspan) {
                    const double tt = ts[next_span_idx];
                    const double theta = (tt - t_old) / (dir * trial_h);
                    if (theta > 1.0 + 1e-12) break;
                    const double clamped = std::max(0.0, std::min(1.0, theta));
                    if (std::fabs(clamped - 1.0) < 1e-15) {
                        // Exact endpoint — use y_new for bit-equality.
                        t_out.push_back(tt);
                        y_out.push_back(y_new);
                    } else {
                        dense_eval(clamped, dir, trial_h, y,
                                   k1, k3, k4, k5, k6, k7, yint);
                        t_out.push_back(tt);
                        y_out.push_back(yint);
                    }
                    ++next_span_idx;
                }
            } else if (refine > 1) {
                // Emit (refine-1) interior points + endpoint.
                std::vector<double> yint;
                for (int r = 1; r < refine; ++r) {
                    const double theta = double(r) / double(refine);
                    dense_eval(theta, dir, trial_h, y,
                               k1, k3, k4, k5, k6, k7, yint);
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
            k1 = k7;        // FSAL
            failed_in_a_row = 0;
            // Step size update (standard formula, safety = 0.9).
            const double exp = 1.0 / 5.0;
            double fac;
            if (err_norm < 1e-300) fac = 5.0;
            else                   fac = 0.9 * std::pow(1.0 / err_norm, exp);
            fac = std::min(5.0, std::max(0.2, fac));
            h = trial_h * fac;
            h = std::min(h, O.max_step);
        } else {
            // ── Reject step — shrink h. ───────────────────────────
            ++failed_in_a_row;
            const double exp = 1.0 / 5.0;
            double fac = 0.9 * std::pow(1.0 / err_norm, exp);
            fac = std::max(0.1, fac);
            h = trial_h * fac;
            if (failed_in_a_row > 10)
                throw Error("ode45: 10 consecutive step rejections; "
                            "tolerances may be too tight or RHS too stiff",
                            0, 0, "ode45", "", "numkit:ode45:tooManyFailures");
            if (h < std::fabs(t) * 1e-15)
                throw Error("ode45: step size underflow",
                            0, 0, "ode45", "", "numkit:ode45:stepUnderflow");
        }
    }
    if (step_count >= max_steps)
        throw Error("ode45: exceeded " + std::to_string(max_steps)
                  + " integration steps", 0, 0, "ode45", "", "numkit:ode45:tooManySteps");

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

void ode45_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ode45: requires (f, tspan, y0[, opts])",
                    0, 0, "ode45", "", "numkit:ode45:nargin");
    auto *mr = ctx.engine->resource();
    const Value opts_v = (args.size() > 3) ? args[3] : Value::Empty;
    auto [tv, yv] = ode45(*ctx.engine, args[0], args[1], args[2], opts_v, mr);
    outs[0] = std::move(tv);
    if (nargout >= 2) outs[1] = std::move(yv);
}

} // namespace detail
} // namespace numkit::ode
