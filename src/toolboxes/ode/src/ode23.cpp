// toolboxes/ode/src/ode23.cpp
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

#include <numkit/value/error.hpp>

#include "ode_detail.hpp"

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

// OdeOpts + read_opts live in the shared ode_detail.hpp (factored out of the
// ode45/ode23 duplication). ode23's Refine default is 1 (passed to read_opts).

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

// The FnHandle RHS bridge lives in ode_detail.hpp (callOdeRhs); this thin
// binder keeps the call sites below unqualified and names ode23 in errors.
void eval_rhs(FnHandle rhs, double t,
              const std::vector<double> &y,
              std::vector<double> &dydt, std::pmr::memory_resource *mr)
{
    callOdeRhs(rhs, t, y, dydt, mr, "ode23");
}

} // anonymous

std::tuple<Value, Value>
ode23(FnHandle rhs, const Value &tspan, const Value &y0,
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

    OdeOpts O = read_opts(opts, 1);
    if (O.abs_tol.size() != 1 && O.abs_tol.size() != d)
        throw Error("ode23: AbsTol must be scalar or match length(y0)",
                    0, 0, "ode23", "", "numkit:ode23:absTolSize");
    auto atol_i = [&](std::size_t i) {
        return (O.abs_tol.size() == 1) ? O.abs_tol[0] : O.abs_tol[i];
    };

    // ── Initial step size (Hairer-Nørsett-Wanner I, Eq. 4.14) ──────
    // 3rd-order method ⇒ exponent 1/3.
    std::vector<double> k1(d), k2(d), k3(d), k4(d);
    eval_rhs(rhs, t0, y, k1, mr);

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
        eval_rhs(rhs, t0 + dir * h0, y1, k2_trial, mr);
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
        eval_rhs(rhs, t + dir * c2 * trial_h, ytmp, k2, mr);
        // k3 at (t + c3 h, y + h (a31 k1 + a32 k2))
        for (std::size_t i = 0; i < d; ++i)
            ytmp[i] = y[i] + dir * trial_h * (a31 * k1[i] + a32 * k2[i]);
        eval_rhs(rhs, t + dir * c3 * trial_h, ytmp, k3, mr);
        // 3rd-order solution
        std::vector<double> y_new(d);
        for (std::size_t i = 0; i < d; ++i)
            y_new[i] = y[i] + dir * trial_h
                     * (b1 * k1[i] + b2 * k2[i] + b3 * k3[i]);
        // k4 at (t + h, y_new) — FSAL
        eval_rhs(rhs, t + dir * trial_h, y_new, k4, mr);
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

} // namespace numkit::ode
