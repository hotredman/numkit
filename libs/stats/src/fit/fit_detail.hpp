// libs/.../fit_detail.hpp — private compute/register substrate (anon-in-
// header, internal linkage per TU) shared by fit.cpp + fit_reg.cpp.
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include "reduction_helpers.hpp"  // engine-free numkit::builtin::detail dim-infra (ops re-export)

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::stats {

namespace {

void mean_var(const Value &x, double &mean, double &var, size_t &N) {
    N = x.numel();
    if (N == 0) { mean = 0.0; var = 0.0; return; }
    double s = 0.0;
    for (size_t i = 0; i < N; ++i) s += x.elemAsDouble(i);
    mean = s / double(N);
    if (N < 2) { var = 0.0; return; }
    double sq = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double d = x.elemAsDouble(i) - mean;
        sq += d * d;
    }
    var = sq / double(N - 1);
}

Value rowCI(double lo, double hi, std::pmr::memory_resource *mr) {
    Value v = Value::matrix(2, 1, ValueType::DOUBLE, mr);
    double *d = v.doubleDataMut();
    d[0] = lo;
    d[1] = hi;
    return v;
}

double tinv_scalar(double p, double nu, std::pmr::memory_resource *mr) {
    Value pv = Value::scalar(p, mr);
    return tinv(pv, nu, mr).toScalar();
}

double chi2inv_scalar(double p, double k, std::pmr::memory_resource *mr) {
    Value pv = Value::scalar(p, mr);
    return chi2inv(pv, k, mr).toScalar();
}

// ── Normal MLE helpers (used by normfit / lognfit cens+freq paths) ────

constexpr double kPi = 3.14159265358979323846;

// Inverse-Mills ratio λ(α) = φ(α)/Φ(-α). For right-censored normal MLE.
inline double inv_mills(double a) {
    const double pdf  = std::exp(-0.5 * a * a) / std::sqrt(2.0 * kPi);
    const double surv = 0.5 * std::erfc(a / std::sqrt(2.0));  // Φ(-a)
    return (surv > 1e-300) ? pdf / surv : 0.0;
}

// z = -norminv(α/2) via Newton iteration on std::erf.
double zNorm(double alpha) {
    const double y_target = 1.0 - alpha;
    double e = y_target;
    for (int it = 0; it < 50; ++it) {
        const double f  = std::erf(e) - y_target;
        const double fp = (2.0 / std::sqrt(kPi)) * std::exp(-e * e);
        const double step = f / fp;
        e -= step;
        if (std::fabs(step) < 1e-15) break;
    }
    return std::sqrt(2.0) * e;
}

// Result of a normal-MLE fit on observations `y` with optional right-
// censoring + freq weights. Used by both normfit (raw x) and lognfit
// (y = log(x)).
struct NormalFitOut {
    double mu, sd;
    double mu_lo, mu_hi;
    double sd_lo, sd_hi;
    bool   ok;
};

NormalFitOut
normal_fit_mle(const ScratchVec<double> &y, const ScratchVec<double> &fr, const ScratchVec<uint8_t> &cn, double alpha, std::pmr::memory_resource *mr)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    NormalFitOut R{nan, nan, nan, nan, nan, nan, false};
    const size_t N = y.size();
    if (N < 2) return R;

    double sumf = 0.0;
    int nuncens = 0;
    for (size_t i = 0; i < N; ++i) {
        sumf += fr[i];
        if (!cn[i]) ++nuncens;
    }
    if (sumf < 2.0 || nuncens < 1) return R;

    const bool any_cens = (nuncens != (int)N);

    if (!any_cens) {
        // Closed-form weighted moments.
        double s = 0.0;
        for (size_t i = 0; i < N; ++i) s += fr[i] * y[i];
        const double mu = s / sumf;
        double sq = 0.0;
        for (size_t i = 0; i < N; ++i) {
            const double d = y[i] - mu;
            sq += fr[i] * d * d;
        }
        const double sd  = std::sqrt(sq / (sumf - 1.0));
        const double dof = sumf - 1.0;
        const double t   = tinv_scalar(1.0 - alpha / 2.0, dof, mr);
        const double sem = sd / std::sqrt(sumf);
        const double chiU = chi2inv_scalar(1.0 - alpha / 2.0, dof, mr);
        const double chiL = chi2inv_scalar(alpha / 2.0, dof, mr);
        R.mu = mu; R.sd = sd;
        R.mu_lo = mu - t * sem; R.mu_hi = mu + t * sem;
        R.sd_lo = std::sqrt(dof * sd * sd / chiU);
        R.sd_hi = std::sqrt(dof * sd * sd / chiL);
        R.ok = true;
        return R;
    }

    // Censored MLE via EM on truncated-normal moments. Init from
    // uncensored data only.
    double mu = 0.0, sd = 0.0;
    {
        double s_un = 0.0, w_un = 0.0;
        for (size_t i = 0; i < N; ++i) if (!cn[i]) {
            s_un += fr[i] * y[i]; w_un += fr[i];
        }
        mu = s_un / w_un;
        double sq = 0.0;
        for (size_t i = 0; i < N; ++i) if (!cn[i]) {
            const double d = y[i] - mu; sq += fr[i] * d * d;
        }
        sd = std::sqrt(std::max(sq / std::max(w_un - 1.0, 1.0), 1e-12));
    }
    const int    maxIter = 200;
    const double tolFun  = 1e-10;
    for (int it = 0; it < maxIter; ++it) {
        double sumfy = 0.0, sumfy2 = 0.0;
        for (size_t i = 0; i < N; ++i) {
            const double yi = y[i], fi = fr[i];
            if (!cn[i]) {
                sumfy  += fi * yi;
                sumfy2 += fi * yi * yi;
            } else {
                const double a   = (yi - mu) / sd;
                const double lam = inv_mills(a);
                const double Ey  = mu + sd * lam;
                const double Eym2 = sd * sd * (1.0 + a * lam);
                const double Ey2  = Eym2 + 2.0 * mu * sd * lam + mu * mu;
                sumfy  += fi * Ey;
                sumfy2 += fi * Ey2;
            }
        }
        const double mu_new  = sumfy / sumf;
        const double var_new = std::max(sumfy2 / sumf - mu_new * mu_new, 1e-300);
        const double sd_new  = std::sqrt(var_new);
        const double delta   = std::fabs(mu_new - mu) + std::fabs(sd_new - sd);
        mu = mu_new; sd = sd_new;
        if (delta < tolFun) break;
    }

    // Analytic observed Fisher info at the MLE.
    double Imm = 0.0, Ims = 0.0, Iss = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double fi = fr[i];
        const double a  = (y[i] - mu) / sd;
        if (!cn[i]) {
            Imm +=  fi / (sd * sd);
            Ims +=  2.0 * fi * (y[i] - mu) / (sd * sd * sd);
            Iss += -fi / (sd * sd) + 3.0 * fi * (y[i] - mu) * (y[i] - mu) / (sd * sd * sd * sd);
        } else {
            const double m  = inv_mills(a);
            const double mp = m * (m - a);
            Imm += fi * mp / (sd * sd);
            Ims += fi * (a * mp + m) / (sd * sd);
            Iss += fi * a * (a * mp + 2.0 * m) / (sd * sd);
        }
    }
    const double det = Imm * Iss - Ims * Ims;
    double SEmu = nan, SEsigma = nan;
    if (det > 0.0) {
        SEmu    = std::sqrt(Iss / det);
        SEsigma = std::sqrt(Imm / det);
    }
    const double z = zNorm(alpha);

    R.mu = mu; R.sd = sd;
    R.mu_lo = mu - z * SEmu;
    R.mu_hi = mu + z * SEmu;
    const double SElogS = (sd > 0.0) ? SEsigma / sd : nan;
    R.sd_lo = sd * std::exp(-z * SElogS);
    R.sd_hi = sd * std::exp( z * SElogS);
    R.ok = true;
    return R;
}

} // anonymous
namespace {
double betainv_scalar2(double p, double a, double b, std::pmr::memory_resource *mr)
{
    if (a <= 0.0 || b <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    if (p <= 0.0) return 0.0;
    if (p >= 1.0) return 1.0;
    return betainv(Value::scalar(p, mr), a, b, mr).toScalar();
}
}
namespace {
constexpr double kLog2Pi = 1.8378770664093454835606594728112352;
}

// MLE negative-log-likelihood workers (def in fit.cpp, external). lognlike_full/
// explike_full fill an optional 2x2 asymptotic-covariance via double *avarOut.
double lognlike_full(double mu, double sigma, const Value &x,
                     const Value &cens, const Value &freq, double *avarOut);
double explike_full(double mu, const Value &x,
                    const Value &cens, const Value &freq, double *avarOut);
double wbllike_full(double scale, double shape, const Value &x,
                    const Value &cens, const Value &freq,
                    std::pmr::memory_resource *mr);
double evlike_full(double mu, double sigma, const Value &x,
                   const Value &cens, const Value &freq,
                   std::pmr::memory_resource *mr);

} // namespace numkit::stats
