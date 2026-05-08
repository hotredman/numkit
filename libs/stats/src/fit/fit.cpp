// libs/stats/src/fit/fit.cpp
//
// Distribution MLE fitters with confidence intervals.

#include <numkit/stats/fit/fit.hpp>

#include <numkit/stats/distributions/chi2.hpp>
#include <numkit/stats/distributions/students_t.hpp>
#include <numkit/stats/distributions/beta.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory_resource>

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

Value rowCI(std::pmr::memory_resource *mr, double lo, double hi) {
    Value v = Value::matrix(2, 1, ValueType::DOUBLE, mr);
    double *d = v.doubleDataMut();
    d[0] = lo;
    d[1] = hi;
    return v;
}

double tinv_scalar(std::pmr::memory_resource *mr, double p, double nu) {
    Value pv = Value::scalar(p, mr);
    return tinv(mr, pv, nu).toScalar();
}

double chi2inv_scalar(std::pmr::memory_resource *mr, double p, double k) {
    Value pv = Value::scalar(p, mr);
    return chi2inv(mr, pv, k).toScalar();
}

} // anonymous

std::tuple<Value, Value, Value, Value>
normfit(std::pmr::memory_resource *mr, const Value &x, double alpha)
{
    double mean = 0, var = 0;
    size_t N = 0;
    mean_var(x, mean, var, N);
    const double sd = std::sqrt(var);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (N < 2) {
        return {Value::scalar(N == 1 ? mean : nan, mr),
                Value::scalar(N == 1 ? 0.0 : nan, mr),
                rowCI(mr, nan, nan), rowCI(mr, nan, nan)};
    }
    const double t = tinv_scalar(mr, 1.0 - alpha / 2.0, double(N - 1));
    const double sem = sd / std::sqrt(double(N));
    const double mu_lo = mean - t * sem;
    const double mu_hi = mean + t * sem;
    // sigma CI from chi² on (N-1)*var.
    const double chiU = chi2inv_scalar(mr, 1.0 - alpha / 2.0, double(N - 1));
    const double chiL = chi2inv_scalar(mr,       alpha / 2.0, double(N - 1));
    const double s_lo = std::sqrt(double(N - 1) * var / chiU);
    const double s_hi = std::sqrt(double(N - 1) * var / chiL);
    return {Value::scalar(mean, mr),
            Value::scalar(sd,   mr),
            rowCI(mr, mu_lo, mu_hi),
            rowCI(mr, s_lo,  s_hi)};
}

std::tuple<Value, Value>
poissfit(std::pmr::memory_resource *mr, const Value &x, double alpha)
{
    const size_t N = x.numel();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (N == 0) return {Value::scalar(nan, mr), rowCI(mr, nan, nan)};
    double S = 0.0;
    for (size_t i = 0; i < N; ++i) S += x.elemAsDouble(i);
    const double lambda = S / double(N);
    // Exact CI via chi² inversion (Garwood).
    const double lo = (S == 0.0) ? 0.0
                                 : chi2inv_scalar(mr, alpha / 2.0,       2.0 * S)       / (2.0 * N);
    const double hi = chi2inv_scalar(mr, 1.0 - alpha / 2.0, 2.0 * (S + 1.0)) / (2.0 * N);
    return {Value::scalar(lambda, mr), rowCI(mr, lo, hi)};
}

std::tuple<Value, Value>
expfit(std::pmr::memory_resource *mr, const Value &x, double alpha,
       const Value *cens, const Value *freq)
{
    const size_t N = x.numel();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (N == 0) return {Value::scalar(nan, mr), rowCI(mr, nan, nan)};
    // Validate optional vector lengths.
    if (cens && cens->numel() != 0 && cens->numel() != N)
        return {Value::scalar(nan, mr), rowCI(mr, nan, nan)};
    if (freq && freq->numel() != 0 && freq->numel() != N)
        return {Value::scalar(nan, mr), rowCI(mr, nan, nan)};
    const bool has_cens = (cens && cens->numel() == N);
    const bool has_freq = (freq && freq->numel() == N);
    // Total observation time T = Σ(freq · x); event count D = Σ(freq · (1-cens)).
    double T = 0.0, D = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double xi = x.elemAsDouble(i);
        const double fi = has_freq ? freq->elemAsDouble(i) : 1.0;
        const double ci = has_cens ? cens->elemAsDouble(i) : 0.0;
        T += fi * xi;
        D += fi * (1.0 - ci);
    }
    if (!(D > 0.0))
        return {Value::scalar(nan, mr), rowCI(mr, nan, nan)};
    const double mu = T / D;
    // Exact CI via χ²(2D): 2T/μ̂ ~ χ²(2D); CI[μ] = [2T/χ²₁₋α/₂, 2T/χ²_α/₂].
    const double dof  = 2.0 * D;
    const double chiU = chi2inv_scalar(mr, 1.0 - alpha / 2.0, dof);
    const double chiL = chi2inv_scalar(mr,       alpha / 2.0, dof);
    const double lo = 2.0 * T / chiU;
    const double hi = 2.0 * T / chiL;
    return {Value::scalar(mu, mr), rowCI(mr, lo, hi)};
}

std::tuple<Value, Value, Value, Value>
unifit(std::pmr::memory_resource *mr, const Value &x, double alpha)
{
    const size_t N = x.numel();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (N == 0) {
        return {Value::scalar(nan, mr), Value::scalar(nan, mr),
                rowCI(mr, nan, nan), rowCI(mr, nan, nan)};
    }
    double mn = x.elemAsDouble(0), mx = mn;
    for (size_t i = 1; i < N; ++i) {
        const double v = x.elemAsDouble(i);
        if (v < mn) mn = v;
        if (v > mx) mx = v;
    }
    const double range = mx - mn;
    const double delta = range * (std::pow(alpha, -1.0 / double(N)) - 1.0);
    return {Value::scalar(mn, mr),
            Value::scalar(mx, mr),
            rowCI(mr, mn - delta, mn),
            rowCI(mr, mx, mx + delta)};
}

// ── lognfit ───────────────────────────────────────────────────────────

namespace {

constexpr double kPi = 3.14159265358979323846;

// Inverse-Mills ratio λ(α) = φ(α)/Φ(-α). For right-censored normal MLE.
inline double inv_mills(double a) {
    const double pdf  = std::exp(-0.5 * a * a) / std::sqrt(2.0 * kPi);
    const double surv = 0.5 * std::erfc(a / std::sqrt(2.0));  // Φ(-a)
    return (surv > 1e-300) ? pdf / surv : 0.0;
}

// Per-observation log-likelihood of a normal model on `y` with optional
// right-censoring (cens=1 ⇒ Y > y observed) and freq weights.
double normal_logL(const ScratchVec<double> &y,
                   const ScratchVec<double> &fr,
                   const ScratchVec<uint8_t> &cn,
                   double mu, double sd)
{
    const double log2pi = std::log(2.0 * kPi);
    double L = 0.0;
    for (size_t i = 0; i < y.size(); ++i) {
        const double a = (y[i] - mu) / sd;
        if (!cn[i]) {
            L += fr[i] * (-0.5 * log2pi - std::log(sd) - 0.5 * a * a);
        } else {
            const double surv = 0.5 * std::erfc(a / std::sqrt(2.0));
            L += fr[i] * std::log(std::max(surv, 1e-300));
        }
    }
    return L;
}

} // anonymous

std::tuple<Value, Value>
lognfit(std::pmr::memory_resource *mr, const Value &x, double alpha,
        const Value *cens, const Value *freq)
{
    const size_t N = x.numel();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    Value parm = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    Value pci  = Value::matrix(2, 2, ValueType::DOUBLE, mr);
    double *pd = parm.doubleDataMut();
    double *cd = pci.doubleDataMut();
    auto fail = [&]() {
        for (int i = 0; i < 2; ++i) pd[i] = nan;
        for (int i = 0; i < 4; ++i) cd[i] = nan;
        return std::make_tuple(std::move(parm), std::move(pci));
    };
    if (N < 2) return fail();

    // Validate optional vector lengths.
    const bool has_cens = (cens && cens->numel() == N);
    const bool has_freq = (freq && freq->numel() == N);
    if (cens && cens->numel() != 0 && cens->numel() != N) return fail();
    if (freq && freq->numel() != 0 && freq->numel() != N) return fail();

    ScratchArena scratch(mr);
    ScratchVec<double>  y(N, &scratch);     // log(x)
    ScratchVec<double>  fr(N, 1.0, &scratch);
    ScratchVec<uint8_t> cn(N, 0, &scratch);
    double sumf = 0.0;
    int nuncens = 0;
    for (size_t i = 0; i < N; ++i) {
        const double xi = x.elemAsDouble(i);
        if (!(xi > 0.0)) return fail();
        y[i] = std::log(xi);
        if (has_freq) fr[i] = freq->elemAsDouble(i);
        if (has_cens) cn[i] = cens->elemAsDouble(i) > 0.5 ? 1 : 0;
        sumf += fr[i];
        if (!cn[i]) ++nuncens;
    }
    if (sumf < 2.0 || nuncens < 1) return fail();

    double mu = 0.0, sd = 0.0;

    if (!has_cens || nuncens == (int)N) {
        // No censoring — closed-form weighted moments on log(x).
        double s = 0.0;
        for (size_t i = 0; i < N; ++i) s += fr[i] * y[i];
        mu = s / sumf;
        double sq = 0.0;
        for (size_t i = 0; i < N; ++i) {
            const double d = y[i] - mu;
            sq += fr[i] * d * d;
        }
        sd = std::sqrt(sq / (sumf - 1.0));

        // CI via t (mu) and chi² (sigma).
        const double dof = sumf - 1.0;
        const double t   = tinv_scalar(mr, 1.0 - alpha / 2.0, dof);
        const double sem = sd / std::sqrt(sumf);
        const double mu_lo = mu - t * sem;
        const double mu_hi = mu + t * sem;
        const double chiU = chi2inv_scalar(mr, 1.0 - alpha / 2.0, dof);
        const double chiL = chi2inv_scalar(mr,       alpha / 2.0, dof);
        const double s_lo = std::sqrt(dof * sd * sd / chiU);
        const double s_hi = std::sqrt(dof * sd * sd / chiL);
        pd[0] = mu;  pd[1] = sd;
        cd[0] = mu_lo;  cd[1] = mu_hi;
        cd[2] = s_lo;   cd[3] = s_hi;
        return {std::move(parm), std::move(pci)};
    }

    // Censored MLE via EM iteration on the truncated-normal moments.
    // Init from uncensored data only.
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
        // E-step: compute E[y] and E[y²] under each observation given the
        // current θ_old.
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
                // E[(y-μ)²|y>y_c] = σ²(1 + α·λ); E[y²] = E[(y-μ)²] + 2μ·E[y-μ] + μ²
                const double Eym2 = sd * sd * (1.0 + a * lam);
                const double Ey2  = Eym2 + 2.0 * mu * sd * lam + mu * mu;
                sumfy  += fi * Ey;
                sumfy2 += fi * Ey2;
            }
        }
        // M-step: MLE → divide by Σf.
        const double mu_new = sumfy / sumf;
        const double var_new = std::max(sumfy2 / sumf - mu_new * mu_new, 1e-300);
        const double sd_new = std::sqrt(var_new);
        const double delta  = std::fabs(mu_new - mu) + std::fabs(sd_new - sd);
        mu = mu_new; sd = sd_new;
        if (delta < tolFun) break;
    }

    // Analytic observed Fisher info (negative Hessian) at the MLE.
    // Per-obs contributions for normal under right-censoring with freq:
    //   uncens i:
    //     -∂²L/∂μ²  = f_i / σ²
    //     -∂²L/∂μ∂σ = 2 f_i (y_i-μ) / σ³
    //     -∂²L/∂σ²  = -f_i/σ² + 3 f_i (y_i-μ)²/σ⁴
    //   cens at y_c, α=(y_c-μ)/σ, m=φ(α)/Φ(-α), m'=m·(m-α):
    //     -∂²L/∂μ²  = f_i · m·(m-α) / σ²
    //     -∂²L/∂μ∂σ = f_i · {α·m'+m} / σ²
    //     -∂²L/∂σ²  = f_i · α · {α·m'+2m} / σ²
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
            const double mp = m * (m - a);          // m'(α) = m·(m-α)
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
    // Asymptotic Wald CI uses standard normal quantile (MATLAB matches):
    // z = -norminv(α/2) = √2 · erf⁻¹(1 - α). Compute via std::erf
    // inverse — for α=0.05 this gives 1.959964...
    const double z = std::sqrt(2.0) * [&]{
        // Inverse erf via Newton refinement of a rational approximation.
        // For α=0.05 z=1.96 exactly to 6 digits; this code path runs once.
        const double y_target = 1.0 - alpha;
        // Beasley-Springer-Moro is overkill — std::erfc + Newton on std::erf.
        double e = y_target;
        for (int it = 0; it < 50; ++it) {
            const double f  = std::erf(e) - y_target;
            const double fp = (2.0 / std::sqrt(kPi)) * std::exp(-e * e);
            e -= f / fp;
        }
        return e;
    }();

    pd[0] = mu;  pd[1] = sd;
    cd[0] = mu - z * SEmu;          // mu_lo
    cd[1] = mu + z * SEmu;          // mu_hi
    // Asymmetric CI on sigma via log-transform: Var(log σ) ≈ Var(σ)/σ².
    const double SElogS = (sd > 0.0) ? SEsigma / sd : nan;
    cd[2] = sd * std::exp(-z * SElogS);  // sigma_lo
    cd[3] = sd * std::exp( z * SElogS);  // sigma_hi
    return {std::move(parm), std::move(pci)};
}

// Backward-compat 2-arg form.
std::tuple<Value, Value>
lognfit(std::pmr::memory_resource *mr, const Value &x, double alpha)
{
    return lognfit(mr, x, alpha, nullptr, nullptr);
}

// ── binofit ───────────────────────────────────────────────────────────

namespace {
double betainv_scalar2(std::pmr::memory_resource *mr,
                       double p, double a, double b)
{
    if (a <= 0.0 || b <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    if (p <= 0.0) return 0.0;
    if (p >= 1.0) return 1.0;
    return betainv(mr, Value::scalar(p, mr), a, b).toScalar();
}
}

std::tuple<Value, Value>
binofit(std::pmr::memory_resource *mr, const Value &x, const Value &n,
        double alpha)
{
    const size_t Nx = x.numel();
    const size_t Nn = n.numel();
    const bool scalarN = (Nn == 1);
    if (!scalarN && Nn != Nx)
        throw Error("binofit: x and n must be the same length",
                    0, 0, "binofit", "", "m:binofit:size");

    Value phat = Value::matrix(Nx, 1, ValueType::DOUBLE, mr);
    Value pci  = Value::matrix(Nx, 2, ValueType::DOUBLE, mr);
    double *pd = phat.doubleDataMut();
    double *cd = pci.doubleDataMut();

    for (size_t i = 0; i < Nx; ++i) {
        const double k = x.elemAsDouble(i);
        const double N = scalarN ? n.elemAsDouble(0) : n.elemAsDouble(i);
        if (!(N > 0.0)) {
            const double nan = std::numeric_limits<double>::quiet_NaN();
            pd[i] = nan;
            cd[i]      = nan;
            cd[i + Nx] = nan;
            continue;
        }
        const double p = k / N;
        pd[i] = p;
        // Clopper-Pearson exact CI.
        const double lo = (k == 0.0) ? 0.0
                                     : betainv_scalar2(mr, alpha / 2.0, k, N - k + 1.0);
        const double hi = (k == N)   ? 1.0
                                     : betainv_scalar2(mr, 1.0 - alpha / 2.0, k + 1.0, N - k);
        // Column-major: pci(:,1) = lower, pci(:,2) = upper.
        cd[i]       = lo;
        cd[i + Nx]  = hi;
    }
    if (Nx == 1) {
        // Collapse phat to a true scalar to match MATLAB's [phat,pci] = binofit(x,n).
        phat = Value::scalar(pd[0], mr);
    }
    return {std::move(phat), std::move(pci)};
}

// ── raylfit ───────────────────────────────────────────────────────────

std::tuple<Value, Value>
raylfit(std::pmr::memory_resource *mr, const Value &x, double alpha)
{
    const size_t N = x.numel();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (N == 0)
        return {Value::scalar(nan, mr), rowCI(mr, nan, nan)};
    double s2 = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double xi = x.elemAsDouble(i);
        s2 += xi * xi;
    }
    const double sigma = std::sqrt(s2 / (2.0 * double(N)));
    const double chiU = chi2inv_scalar(mr, 1.0 - alpha / 2.0, 2.0 * double(N));
    const double chiL = chi2inv_scalar(mr,       alpha / 2.0, 2.0 * double(N));
    const double lo = sigma * std::sqrt(2.0 * double(N) / chiU);
    const double hi = sigma * std::sqrt(2.0 * double(N) / chiL);
    return {Value::scalar(sigma, mr), rowCI(mr, lo, hi)};
}

// ── Negative log-likelihoods ──────────────────────────────────────────

namespace {
constexpr double kLog2Pi = 1.8378770664093454835606594728112352;
}

double normlike(std::pmr::memory_resource * /*mr*/, double mu, double sigma,
                const Value &x, const Value &cens, const Value &freq)
{
    const size_t N = x.numel();
    if (N == 0) return 0.0;
    if (!(sigma > 0.0)) return std::numeric_limits<double>::quiet_NaN();
    const bool useC = cens.numel() > 0;
    const bool useF = freq.numel() > 0;
    if (useC && cens.numel() != N)
        throw Error("normlike: censoring must match the data length",
                    0, 0, "normlike", "", "m:normlike:cens");
    if (useF && freq.numel() != N)
        throw Error("normlike: freq must match the data length",
                    0, 0, "normlike", "", "m:normlike:freq");

    const double inv_s   = 1.0 / sigma;
    const double inv_2s2 = 0.5 * inv_s * inv_s;
    const double logS    = std::log(sigma);
    const double halfLog2pi = 0.5 * kLog2Pi;
    const double sqrt2_inv  = 1.0 / std::sqrt(2.0);
    double nL = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double w = useF ? freq.elemAsDouble(i) : 1.0;
        if (w == 0.0) continue;                                  // MATLAB drops zero-freq rows
        const double xi = x.elemAsDouble(i);
        if (std::isnan(xi)) return std::numeric_limits<double>::quiet_NaN();
        const double d = xi - mu;
        const double z = d * inv_s;
        const bool censored = useC && (cens.elemAsDouble(i) != 0.0);
        if (censored) {
            // -log(S(z)) = -log(0.5·erfc(z/sqrt(2)))
            const double s = 0.5 * std::erfc(z * sqrt2_inv);
            nL += w * (-std::log(s));
        } else {
            // -log(f(z)) = log(σ) + 0.5·log(2π) + 0.5·z²
            nL += w * (logS + halfLog2pi + d * d * inv_2s2);
        }
    }
    return nL;
}

double explike(std::pmr::memory_resource * /*mr*/, double mu, const Value &x)
{
    // Two-arg form: empty data ⇒ 0 (matches MATLAB R2025b),
    //               mu <= 0   ⇒ NaN.
    const size_t N = x.numel();
    if (N == 0) return 0.0;
    if (mu <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    double sx = 0.0;
    for (size_t i = 0; i < N; ++i) sx += x.elemAsDouble(i);
    return double(N) * std::log(mu) + sx / mu;
}

// Extended form for the adapter: cens + freq + scalar avar.
// Returns nL via the function value; if `avarOut` is non-null, fills
// it with the inverse observed Fisher info (1/I).
//
// Uncensored row, weight w:  contributes w·(log μ + x/μ)
//   ∂²nL/∂μ² += w · (-1/μ² + 2 x / μ³)
// Right-censored row, weight w: contributes w·(x/μ)
//   ∂²nL/∂μ² += w · (2 x / μ³)
// Empty (after freq=0 drops) ⇒ 0.
static double explike_full(double mu, const Value &x,
                           const Value &cens, const Value &freq,
                           double *avarOut)
{
    const size_t N = x.numel();
    if (N == 0) {
        if (avarOut) *avarOut = std::numeric_limits<double>::quiet_NaN();
        return 0.0;
    }
    if (mu <= 0.0) {
        if (avarOut) *avarOut = std::numeric_limits<double>::quiet_NaN();
        return std::numeric_limits<double>::quiet_NaN();
    }
    const bool useC = cens.numel() > 0;
    const bool useF = freq.numel() > 0;
    if (useC && cens.numel() != N)
        throw Error("explike: censoring must match the data length",
                    0, 0, "explike", "", "m:explike:cens");
    if (useF && freq.numel() != N)
        throw Error("explike: freq must match the data length",
                    0, 0, "explike", "", "m:explike:freq");

    const double inv_mu  = 1.0 / mu;
    const double inv_mu2 = inv_mu * inv_mu;
    const double inv_mu3 = inv_mu2 * inv_mu;
    const double logMu   = std::log(mu);

    double nL = 0.0, I = 0.0;
    bool any = false;
    for (size_t i = 0; i < N; ++i) {
        const double w = useF ? freq.elemAsDouble(i) : 1.0;
        if (w == 0.0) continue;
        any = true;
        const double xi = x.elemAsDouble(i);
        const bool censored = useC && (cens.elemAsDouble(i) != 0.0);
        if (censored) {
            nL += w * (xi * inv_mu);
            I  += w * (2.0 * xi * inv_mu3);
        } else {
            nL += w * (logMu + xi * inv_mu);
            I  += w * (-inv_mu2 + 2.0 * xi * inv_mu3);
        }
    }
    if (avarOut) {
        if (!any || I == 0.0 || !std::isfinite(I))
            *avarOut = std::numeric_limits<double>::quiet_NaN();
        else
            *avarOut = 1.0 / I;
    }
    return any ? nL : 0.0;
}

double lognlike(std::pmr::memory_resource * /*mr*/, double mu, double sigma,
                const Value &x)
{
    // Two-arg form. Edges (matching MATLAB R2025b):
    //   empty data ⇒ 0;  sigma <= 0 ⇒ NaN;  any x[i] <= 0 ⇒ NaN.
    const size_t N = x.numel();
    if (N == 0) return 0.0;
    if (sigma <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    double sumLogX = 0.0, ss = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double xi = x.elemAsDouble(i);
        if (xi <= 0.0) return std::numeric_limits<double>::quiet_NaN();
        const double lx = std::log(xi);
        sumLogX += lx;
        const double d = lx - mu;
        ss += d * d;
    }
    return sumLogX + double(N) * std::log(sigma)
         + 0.5 * double(N) * kLog2Pi + ss / (2.0 * sigma * sigma);
}

// Extended lognlike: cens + freq + 2×2 aVar (inverse observed-Fisher).
// Hessian wrt (mu, sigma) is structurally identical to the normal
// Hessian on y = log x (the per-row `log x_i` baseline is a constant
// in (mu, sigma)). Same uncensored / right-censored split as normlike.
//
// `avarOut` (4 doubles, column-major 2×2) filled iff non-null.
static double lognlike_full(double mu, double sigma, const Value &x,
                            const Value &cens, const Value &freq,
                            double *avarOut)
{
    const double NaNd = std::numeric_limits<double>::quiet_NaN();
    auto setAvarNaN = [&]() {
        if (avarOut) { for (int i = 0; i < 4; ++i) avarOut[i] = NaNd; }
    };
    const size_t N = x.numel();
    if (N == 0) { setAvarNaN(); return 0.0; }
    if (sigma <= 0.0) { setAvarNaN(); return NaNd; }
    const bool useC = cens.numel() > 0;
    const bool useF = freq.numel() > 0;
    if (useC && cens.numel() != N)
        throw Error("lognlike: censoring must match the data length",
                    0, 0, "lognlike", "", "m:lognlike:cens");
    if (useF && freq.numel() != N)
        throw Error("lognlike: freq must match the data length",
                    0, 0, "lognlike", "", "m:lognlike:freq");

    const double inv_s   = 1.0 / sigma;
    const double inv_s2  = inv_s * inv_s;
    const double inv_s3  = inv_s2 * inv_s;
    const double inv_s4  = inv_s2 * inv_s2;
    const double inv_2s2 = 0.5 * inv_s2;
    const double logS    = std::log(sigma);
    const double halfL2pi = 0.5 * kLog2Pi;
    const double sqrt2_inv  = 1.0 / std::sqrt(2.0);
    const double sqrt2pi_inv = 1.0 / std::sqrt(2.0 * 3.14159265358979323846);

    double nL = 0.0;
    double I00 = 0.0, I01 = 0.0, I11 = 0.0;
    bool any = false;
    for (size_t i = 0; i < N; ++i) {
        const double w = useF ? freq.elemAsDouble(i) : 1.0;
        if (w == 0.0) continue;
        const double xi = x.elemAsDouble(i);
        if (xi <= 0.0) { setAvarNaN(); return NaNd; }
        any = true;
        const double lx = std::log(xi);
        const double d  = lx - mu;
        const double z  = d * inv_s;
        const bool censored = useC && (cens.elemAsDouble(i) != 0.0);
        if (!censored) {
            // -log f = log x + log σ + 0.5·log 2π + d²/(2σ²)
            nL  += w * (lx + logS + halfL2pi + d * d * inv_2s2);
            I00 += w * inv_s2;
            I11 += w * (-inv_s2 + 3.0 * d * d * inv_s4);
            I01 += w * 2.0 * d * inv_s3;
        } else {
            // -log S(z) = -log(0.5·erfc(z/√2)). For Hessian use the
            // same formulas as right-censored normal:
            //   I_μμ += w · h'/σ²
            //   I_σσ += w · (2 z h + z² h')/σ²
            //   I_μσ += w · (z h' + h)/σ²
            //   h = φ(z)/S(z), h' = h(h - z)
            const double S = 0.5 * std::erfc(z * sqrt2_inv);
            nL += w * (-std::log(S));
            const double phi = sqrt2pi_inv * std::exp(-0.5 * z * z);
            const double h   = phi / S;
            const double hp  = h * (h - z);
            I00 += w * hp * inv_s2;
            I11 += w * (2.0 * z * h + z * z * hp) * inv_s2;
            I01 += w * (z * hp + h) * inv_s2;
        }
    }
    if (avarOut) {
        if (!any) {
            setAvarNaN();
        } else {
            const double det = I00 * I11 - I01 * I01;
            if (det == 0.0 || !std::isfinite(det)) {
                setAvarNaN();
            } else {
                const double inv = 1.0 / det;
                avarOut[0] =  I11 * inv;
                avarOut[1] = -I01 * inv;
                avarOut[2] = -I01 * inv;
                avarOut[3] =  I00 * inv;
            }
        }
    }
    return any ? nL : 0.0;
}

double gamlike(std::pmr::memory_resource * /*mr*/, double a, double b,
               const Value &x)
{
    const size_t N = x.numel();
    if (N == 0) return std::numeric_limits<double>::infinity();
    if (a <= 0.0 || b <= 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    double sumLogX = 0.0, sx = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double xi = x.elemAsDouble(i);
        if (xi <= 0.0) return std::numeric_limits<double>::infinity();
        sumLogX += std::log(xi);
        sx += xi;
    }
    return -(a - 1.0) * sumLogX + sx / b
         + double(N) * a * std::log(b) + double(N) * std::lgamma(a);
}

double betalike(std::pmr::memory_resource * /*mr*/, double a, double b,
                const Value &x)
{
    const size_t N = x.numel();
    if (N == 0) return std::numeric_limits<double>::infinity();
    if (a <= 0.0 || b <= 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    double sumLog = 0.0, sumLog1m = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double xi = x.elemAsDouble(i);
        if (xi <= 0.0 || xi >= 1.0)
            return std::numeric_limits<double>::quiet_NaN();
        sumLog   += std::log(xi);
        sumLog1m += std::log1p(-xi);
    }
    const double logBeta = std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);
    return -(a - 1.0) * sumLog - (b - 1.0) * sumLog1m + double(N) * logBeta;
}

double wbllike(std::pmr::memory_resource * /*mr*/, double scale, double shape,
               const Value &x)
{
    const size_t N = x.numel();
    if (N == 0) return 0.0;
    if (!(scale > 0.0) || !(shape > 0.0))
        return std::numeric_limits<double>::quiet_NaN();
    double sumLogX = 0.0, sumPow = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double xi = x.elemAsDouble(i);
        if (xi <= 0.0) return std::numeric_limits<double>::quiet_NaN();
        sumLogX += std::log(xi);
        sumPow  += std::pow(xi / scale, shape);
    }
    return -double(N) * std::log(shape) + double(N) * shape * std::log(scale)
         - (shape - 1.0) * sumLogX + sumPow;
}

// Extended form: cens + freq. Weibull(scale, shape).
//   pdf:      f(x) = (shape/scale)·(x/scale)^(shape-1)·exp(-(x/scale)^shape)
//   survival: S(x) = exp(-(x/scale)^shape)
//   -log f:   -log(shape) + shape·log(scale) - (shape-1)·log(x) + (x/scale)^shape
//   -log S:   (x/scale)^shape
double wbllike_full(std::pmr::memory_resource * /*mr*/, double scale, double shape,
                    const Value &x, const Value &cens, const Value &freq)
{
    const size_t N = x.numel();
    if (N == 0) return 0.0;
    if (!(scale > 0.0) || !(shape > 0.0))
        return std::numeric_limits<double>::quiet_NaN();
    const bool useC = cens.numel() > 0;
    const bool useF = freq.numel() > 0;
    if (useC && cens.numel() != N)
        throw Error("wbllike: censoring must match the data length",
                    0, 0, "wbllike", "", "m:wbllike:cens");
    if (useF && freq.numel() != N)
        throw Error("wbllike: freq must match the data length",
                    0, 0, "wbllike", "", "m:wbllike:freq");
    const double logA = std::log(scale);
    const double logK = std::log(shape);
    double nL = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double w = useF ? freq.elemAsDouble(i) : 1.0;
        if (w == 0.0) continue;
        const double xi = x.elemAsDouble(i);
        if (!(xi > 0.0)) return std::numeric_limits<double>::quiet_NaN();
        const double pow_term = std::pow(xi / scale, shape);
        const bool censored = useC && (cens.elemAsDouble(i) != 0.0);
        if (censored) nL += w * pow_term;
        else          nL += w * (-logK + shape * logA - (shape - 1.0) * std::log(xi) + pow_term);
    }
    return nL;
}

double evlike(std::pmr::memory_resource * /*mr*/, double mu, double sigma,
              const Value &x)
{
    const size_t N = x.numel();
    if (N == 0) return 0.0;                                  // matches MATLAB convention
    if (!(sigma > 0.0)) return std::numeric_limits<double>::quiet_NaN();
    double sLin = 0.0, sExp = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double t = (x.elemAsDouble(i) - mu) / sigma;
        sLin += t;
        sExp += std::exp(t);
    }
    return double(N) * std::log(sigma) - sLin + sExp;
}

// Extended form: cens + freq.
//   Gumbel-min (Type-I extreme value): density f(z) = (1/σ)·exp(z)·exp(-exp(z)).
//   Uncensored contribution: log(σ) - z + exp(z), weight w.
//   Right-censored contribution: -log S(z) = exp(z), weight w.
double evlike_full(std::pmr::memory_resource * /*mr*/, double mu, double sigma,
                   const Value &x, const Value &cens, const Value &freq)
{
    const size_t N = x.numel();
    if (N == 0) return 0.0;
    if (!(sigma > 0.0)) return std::numeric_limits<double>::quiet_NaN();
    const bool useC = cens.numel() > 0;
    const bool useF = freq.numel() > 0;
    if (useC && cens.numel() != N)
        throw Error("evlike: censoring must match the data length",
                    0, 0, "evlike", "", "m:evlike:cens");
    if (useF && freq.numel() != N)
        throw Error("evlike: freq must match the data length",
                    0, 0, "evlike", "", "m:evlike:freq");
    const double inv_s = 1.0 / sigma;
    const double logS  = std::log(sigma);
    double nL = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double w = useF ? freq.elemAsDouble(i) : 1.0;
        if (w == 0.0) continue;
        const double xi = x.elemAsDouble(i);
        if (std::isnan(xi)) return std::numeric_limits<double>::quiet_NaN();
        const double z  = (xi - mu) * inv_s;
        const double ez = std::exp(z);
        const bool censored = useC && (cens.elemAsDouble(i) != 0.0);
        if (censored)  nL += w * ez;                           // -log S(z)
        else           nL += w * (logS - z + ez);              // -log f(z)
    }
    return nL;
}

double gevlike(std::pmr::memory_resource * /*mr*/, double k, double sigma,
               double mu, const Value &x)
{
    const size_t N = x.numel();
    if (N == 0) return std::numeric_limits<double>::infinity();
    if (sigma <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    if (k == 0.0) {
        double sZ = 0.0, sExp = 0.0;
        for (size_t i = 0; i < N; ++i) {
            const double z = (x.elemAsDouble(i) - mu) / sigma;
            sZ += z;
            sExp += std::exp(-z);
        }
        return double(N) * std::log(sigma) + sZ + sExp;
    }
    double sLogT = 0.0, sTinvk = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double z = (x.elemAsDouble(i) - mu) / sigma;
        const double t = 1.0 + k * z;
        if (t <= 0.0) return std::numeric_limits<double>::quiet_NaN();
        sLogT  += std::log(t);
        sTinvk += std::pow(t, -1.0 / k);
    }
    return double(N) * std::log(sigma) + (1.0 / k + 1.0) * sLogT + sTinvk;
}

double gplike(std::pmr::memory_resource * /*mr*/, double k, double sigma,
              const Value &x)
{
    // MATLAB's gplike does NOT enforce x >= 0 — it only requires the
    // per-point support condition `1 + k*x/sigma > 0`. With k>0 that
    // permits negative x close enough to 0; with k=0 (exponential
    // limit) the formula admits any finite x. Edges (matching MATLAB
    // R2025b probe — note the asymmetry vs gevlike):
    //   sigma == 0 → NaN
    //   sigma  < 0 → -Inf
    //   per-point support violation → +Inf (not NaN)
    const size_t N = x.numel();
    if (N == 0) return std::numeric_limits<double>::infinity();
    if (sigma == 0.0) return std::numeric_limits<double>::quiet_NaN();
    if (sigma  < 0.0) return -std::numeric_limits<double>::infinity();
    if (k == 0.0) {
        double sX = 0.0;
        for (size_t i = 0; i < N; ++i) sX += x.elemAsDouble(i);
        return double(N) * std::log(sigma) + sX / sigma;
    }
    double sLogT = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double xi = x.elemAsDouble(i);
        const double t = 1.0 + k * xi / sigma;
        if (t <= 0.0) return std::numeric_limits<double>::infinity();
        sLogT += std::log(t);
    }
    return double(N) * std::log(sigma) + (1.0 / k + 1.0) * sLogT;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

static double parse_alpha_arg(Span<const Value> args, size_t pos, double def) {
    if (pos >= args.size() || args[pos].isEmpty()) return def;
    return args[pos].toScalar();
}

void normfit_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("normfit: requires X[, alpha]",
                    0, 0, "normfit", "", "m:normfit:nargin");
    const double alpha = parse_alpha_arg(args, 1, 0.05);
    auto [mu, sd, muci, sdci] = normfit(ctx.engine->resource(), args[0], alpha);
    outs[0] = std::move(mu);
    if (nargout > 1) outs[1] = std::move(sd);
    if (nargout > 2) outs[2] = std::move(muci);
    if (nargout > 3) outs[3] = std::move(sdci);
}

void poissfit_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("poissfit: requires X[, alpha]",
                    0, 0, "poissfit", "", "m:poissfit:nargin");
    const double alpha = parse_alpha_arg(args, 1, 0.05);
    auto [lam, ci] = poissfit(ctx.engine->resource(), args[0], alpha);
    outs[0] = std::move(lam);
    if (nargout > 1) outs[1] = std::move(ci);
}

void expfit_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("expfit: requires X[, alpha[, censoring[, freq]]]",
                    0, 0, "expfit", "", "m:expfit:nargin");
    const double alpha = parse_alpha_arg(args, 1, 0.05);
    const Value *cens = (args.size() > 2) ? &args[2] : nullptr;
    const Value *freq = (args.size() > 3) ? &args[3] : nullptr;
    auto [mu, ci] = expfit(ctx.engine->resource(), args[0], alpha, cens, freq);
    outs[0] = std::move(mu);
    if (nargout > 1) outs[1] = std::move(ci);
}

void unifit_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("unifit: requires X[, alpha]",
                    0, 0, "unifit", "", "m:unifit:nargin");
    const double alpha = parse_alpha_arg(args, 1, 0.05);
    auto [a, b, aci, bci] = unifit(ctx.engine->resource(), args[0], alpha);
    outs[0] = std::move(a);
    if (nargout > 1) outs[1] = std::move(b);
    if (nargout > 2) outs[2] = std::move(aci);
    if (nargout > 3) outs[3] = std::move(bci);
}

void lognfit_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("lognfit: requires X[, alpha[, censoring[, freq[, options]]]]",
                    0, 0, "lognfit", "", "m:lognfit:nargin");
    const double alpha = parse_alpha_arg(args, 1, 0.05);
    // 3rd arg = censoring (may be empty []), 4th = freq (may be empty),
    // 5th = options struct (silently ignored — we use fixed 200 / 1e-10).
    const Value *cens = (args.size() > 2 && !args[2].isEmpty()) ? &args[2] : nullptr;
    const Value *freq = (args.size() > 3 && !args[3].isEmpty()) ? &args[3] : nullptr;
    auto [parm, pci] = lognfit(ctx.engine->resource(), args[0], alpha,
                                cens, freq);
    outs[0] = std::move(parm);
    if (nargout > 1) outs[1] = std::move(pci);
}

void binofit_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("binofit: requires (X, N[, alpha])",
                    0, 0, "binofit", "", "m:binofit:nargin");
    const double alpha = parse_alpha_arg(args, 2, 0.05);
    auto [phat, pci] = binofit(ctx.engine->resource(), args[0], args[1], alpha);
    outs[0] = std::move(phat);
    if (nargout > 1) outs[1] = std::move(pci);
}

void raylfit_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("raylfit: requires X[, alpha]",
                    0, 0, "raylfit", "", "m:raylfit:nargin");
    const double alpha = parse_alpha_arg(args, 1, 0.05);
    auto [shat, sci] = raylfit(ctx.engine->resource(), args[0], alpha);
    outs[0] = std::move(shat);
    if (nargout > 1) outs[1] = std::move(sci);
}

// ─── *like adapters ───────────────────────────────────────────────────

// Digamma ψ(z) for z > 0 — recurrence to z>=8 then asymptotic series.
static double digamma(double z)
{
    double r = 0.0;
    while (z < 8.0) { r -= 1.0 / z; z += 1.0; }
    const double inv  = 1.0 / z;
    const double inv2 = inv * inv;
    r += std::log(z) - 0.5 * inv;
    r -= inv2 * (1.0/12.0 - inv2 * (1.0/120.0 - inv2 * 1.0/252.0));
    return r;
}

// Fill a 2×2 inverse observed-Fisher matrix `p` (column-major,
// parameter order [p0, p1]) for a 2-parameter likelihood. Uses central
// differences (no in-tree trigamma); step h ≈ eps^(1/4) ≈ 1e-4 is the
// optimal balance between truncation O(h²) and roundoff O(eps/h²).
// Caller must have already verified that `nL` is finite — we do not
// re-validate inputs.
template <class Eval>
static void fill_fd_avar2(double *p, double p0, double p1,
                          double nL, Eval eval_nL)
{
    const double NaNd = std::numeric_limits<double>::quiet_NaN();
    if (!std::isfinite(nL)) {
        p[0] = NaNd; p[1] = NaNd; p[2] = NaNd; p[3] = NaNd;
        return;
    }
    const double h0 = std::max(1e-4, 1e-4 * std::abs(p0));
    const double h1 = std::max(1e-4, 1e-4 * std::abs(p1));
    const double f_p0 = eval_nL(p0 + h0, p1);
    const double f_m0 = eval_nL(p0 - h0, p1);
    const double f_p1 = eval_nL(p0, p1 + h1);
    const double f_m1 = eval_nL(p0, p1 - h1);
    const double f_pp = eval_nL(p0 + h0, p1 + h1);
    const double f_pm = eval_nL(p0 + h0, p1 - h1);
    const double f_mp = eval_nL(p0 - h0, p1 + h1);
    const double f_mm = eval_nL(p0 - h0, p1 - h1);
    const double I00 = (f_p0 - 2.0 * nL + f_m0) / (h0 * h0);
    const double I11 = (f_p1 - 2.0 * nL + f_m1) / (h1 * h1);
    const double I01 = (f_pp - f_pm - f_mp + f_mm) / (4.0 * h0 * h1);
    const double det = I00 * I11 - I01 * I01;
    if (det == 0.0 || !std::isfinite(det)) {
        p[0] = NaNd; p[1] = NaNd; p[2] = NaNd; p[3] = NaNd;
    } else {
        const double inv = 1.0 / det;
        p[0] =  I11 * inv;
        p[1] = -I01 * inv;
        p[2] = -I01 * inv;
        p[3] =  I00 * inv;
    }
}

// 3-parameter analogue of `fill_fd_avar2`. Fills a 3×3 column-major
// inverse observed-Fisher matrix at `(p0, p1, p2)`. 18 nL evaluations.
template <class Eval>
static void fill_fd_avar3(double *p, double p0, double p1, double p2,
                          double nL, Eval eval_nL)
{
    const double NaNd = std::numeric_limits<double>::quiet_NaN();
    auto setNaN = [&]() {
        for (int i = 0; i < 9; ++i) p[i] = NaNd;
    };
    if (!std::isfinite(nL)) { setNaN(); return; }

    const double h0 = std::max(1e-4, 1e-4 * std::abs(p0));
    const double h1 = std::max(1e-4, 1e-4 * std::abs(p1));
    const double h2 = std::max(1e-4, 1e-4 * std::abs(p2));
    auto e = [&](double q0, double q1, double q2) { return eval_nL(q0, q1, q2); };
    // Diagonal entries.
    const double H00 = (e(p0+h0,p1,p2) - 2.0*nL + e(p0-h0,p1,p2)) / (h0*h0);
    const double H11 = (e(p0,p1+h1,p2) - 2.0*nL + e(p0,p1-h1,p2)) / (h1*h1);
    const double H22 = (e(p0,p1,p2+h2) - 2.0*nL + e(p0,p1,p2-h2)) / (h2*h2);
    // Off-diagonals via 4-point stencil.
    auto cross = [&](int a, int b) {
        double da[3] = {0.0, 0.0, 0.0}, db[3] = {0.0, 0.0, 0.0};
        const double ha = (a == 0 ? h0 : (a == 1 ? h1 : h2));
        const double hb = (b == 0 ? h0 : (b == 1 ? h1 : h2));
        da[a] = ha; db[b] = hb;
        const double pp = e(p0+da[0]+db[0], p1+da[1]+db[1], p2+da[2]+db[2]);
        const double pm = e(p0+da[0]-db[0], p1+da[1]-db[1], p2+da[2]-db[2]);
        const double mp = e(p0-da[0]+db[0], p1-da[1]+db[1], p2-da[2]+db[2]);
        const double mm = e(p0-da[0]-db[0], p1-da[1]-db[1], p2-da[2]-db[2]);
        return (pp - pm - mp + mm) / (4.0 * ha * hb);
    };
    const double H01 = cross(0, 1);
    const double H02 = cross(0, 2);
    const double H12 = cross(1, 2);
    // 3×3 cofactor inversion. Symmetric: H10=H01, H20=H02, H21=H12.
    const double C00 = H11*H22 - H12*H12;
    const double C01 = -(H01*H22 - H12*H02);
    const double C02 = H01*H12 - H11*H02;
    const double C11 = H00*H22 - H02*H02;
    const double C12 = -(H00*H12 - H01*H02);
    const double C22 = H00*H11 - H01*H01;
    const double det = H00 * C00 + H01 * C01 + H02 * C02;
    if (det == 0.0 || !std::isfinite(det)) { setNaN(); return; }
    const double inv = 1.0 / det;
    // Column-major 3×3, parameter order [p0, p1, p2].
    p[0] = C00 * inv; p[1] = C01 * inv; p[2] = C02 * inv;  // col 0
    p[3] = C01 * inv; p[4] = C11 * inv; p[5] = C12 * inv;  // col 1
    p[6] = C02 * inv; p[7] = C12 * inv; p[8] = C22 * inv;  // col 2
}

static void like2_reg(const char *fn,
                      double (*impl)(std::pmr::memory_resource *,
                                     double, double, const Value &),
                      Span<const Value> args, Span<Value> outs,
                      CallContext &ctx)
{
    if (args.size() < 2 || args[0].numel() < 2)
        throw Error(std::string(fn) + ": requires (params[2], data)",
                    0, 0, fn, "", "m:like:nargin");
    const double p0 = args[0].elemAsDouble(0);
    const double p1 = args[0].elemAsDouble(1);
    const double nL = impl(ctx.engine->resource(), p0, p1, args[1]);
    outs[0] = Value::scalar(nL, ctx.engine->resource());
}

void normlike_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args[0].numel() < 2)
        throw Error("normlike: requires (params=[mu sigma], data[, cens, freq])",
                    0, 0, "normlike", "", "m:normlike:nargin");
    const double mu    = args[0].elemAsDouble(0);
    const double sigma = args[0].elemAsDouble(1);
    Value emptyVal = Value::matrix(0, 0, ValueType::DOUBLE, ctx.engine->resource());
    const Value &cens = (args.size() >= 3) ? args[2] : emptyVal;
    const Value &freq = (args.size() >= 4) ? args[3] : emptyVal;
    const double nL = normlike(ctx.engine->resource(), mu, sigma, args[1],
                               cens, freq);
    outs[0] = Value::scalar(nL, ctx.engine->resource());

    // Second output: aVar = inv(observed Fisher information).
    // Order [mu, sigma]; symmetric 2×2.
    if (nargout >= 2) {
        const Value &x = args[1];
        const size_t N = x.numel();
        const bool useC = cens.numel() > 0;
        const bool useF = freq.numel() > 0;
        // Observed information I (= positive Hessian of nL):
        //   uncensored row, weight w:
        //     I_μμ += w / σ²
        //     I_σσ += w · (-1/σ² + 3·d²/σ⁴)
        //     I_μσ += w · 2·d/σ³            (d = x-μ)
        //   right-censored row, weight w (h=φ(z)/S(z), h'=h(h-z)):
        //     I_μμ += w · h'/σ²
        //     I_σσ += w · (2z·h + z²·h')/σ²
        //     I_μσ += w · (z·h' + h)/σ²
        const double inv_s   = 1.0 / sigma;
        const double inv_s2  = inv_s * inv_s;
        const double inv_s3  = inv_s2 * inv_s;
        const double inv_s4  = inv_s2 * inv_s2;
        const double sqrt2pi_inv = 1.0 / std::sqrt(2.0 * 3.14159265358979323846);
        const double sqrt2_inv   = 1.0 / std::sqrt(2.0);
        double I00 = 0.0, I01 = 0.0, I11 = 0.0;
        bool nanSeen = false;
        for (size_t i = 0; i < N; ++i) {
            const double w = useF ? freq.elemAsDouble(i) : 1.0;
            if (w == 0.0) continue;
            const double xi = x.elemAsDouble(i);
            if (std::isnan(xi)) { nanSeen = true; break; }
            const double d = xi - mu;
            const double z = d * inv_s;
            const bool censored = useC && (cens.elemAsDouble(i) != 0.0);
            if (!censored) {
                I00 += w * inv_s2;
                I11 += w * (-inv_s2 + 3.0 * d * d * inv_s4);
                I01 += w * 2.0 * d * inv_s3;
            } else {
                const double phi = sqrt2pi_inv * std::exp(-0.5 * z * z);
                const double S   = 0.5 * std::erfc(z * sqrt2_inv);
                const double h   = phi / S;
                const double hp  = h * (h - z);
                I00 += w * hp * inv_s2;
                I11 += w * (2.0 * z * h + z * z * hp) * inv_s2;
                I01 += w * (z * hp + h) * inv_s2;
            }
        }
        const double NaNd = std::numeric_limits<double>::quiet_NaN();
        Value av = Value::matrix(2, 2, ValueType::DOUBLE, ctx.engine->resource());
        double *p = av.doubleDataMut();
        if (nanSeen || N == 0 || !(sigma > 0.0)) {
            p[0] = NaNd; p[1] = NaNd; p[2] = NaNd; p[3] = NaNd;
        } else {
            const double det = I00 * I11 - I01 * I01;
            if (det == 0.0 || !std::isfinite(det)) {
                p[0] = NaNd; p[1] = NaNd; p[2] = NaNd; p[3] = NaNd;
            } else {
                const double inv = 1.0 / det;
                // column-major 2×2: stored [a, b, c, d] = [(1,1), (2,1), (1,2), (2,2)]
                p[0] =  I11 * inv;
                p[1] = -I01 * inv;
                p[2] = -I01 * inv;
                p[3] =  I00 * inv;
            }
        }
        outs[1] = std::move(av);
    }
}

void lognlike_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args[0].numel() < 2)
        throw Error("lognlike: requires (params=[mu sigma], data[, cens, freq])",
                    0, 0, "lognlike", "", "m:lognlike:nargin");
    auto *mr = ctx.engine->resource();
    const double mu    = args[0].elemAsDouble(0);
    const double sigma = args[0].elemAsDouble(1);
    Value emptyVal = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    const Value &cens = (args.size() >= 3) ? args[2] : emptyVal;
    const Value &freq = (args.size() >= 4) ? args[3] : emptyVal;
    Value av = Value::matrix(2, 2, ValueType::DOUBLE, mr);
    const double nL = lognlike_full(mu, sigma, args[1], cens, freq,
                                    nargout >= 2 ? av.doubleDataMut() : nullptr);
    outs[0] = Value::scalar(nL, mr);
    if (nargout >= 2) outs[1] = std::move(av);
}

void gamlike_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args[0].numel() < 2)
        throw Error("gamlike: requires (params=[a b], data)",
                    0, 0, "gamlike", "", "m:gamlike:nargin");
    auto *mr = ctx.engine->resource();
    const double a  = args[0].elemAsDouble(0);
    const double b  = args[0].elemAsDouble(1);
    const Value &x  = args[1];
    const double nL = gamlike(mr, a, b, x);
    outs[0] = Value::scalar(nL, mr);

    // Second output: 2×2 inverse observed-Fisher info, parameter order
    // [a, b]. Computed via central-difference Hessian (no trigamma).
    if (nargout >= 2) {
        Value av = Value::matrix(2, 2, ValueType::DOUBLE, mr);
        fill_fd_avar2(av.doubleDataMut(), a, b, nL,
                      [&](double aa, double bb) { return gamlike(mr, aa, bb, x); });
        outs[1] = std::move(av);
    }
}

void betalike_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args[0].numel() < 2)
        throw Error("betalike: requires (params=[a b], data)",
                    0, 0, "betalike", "", "m:betalike:nargin");
    auto *mr = ctx.engine->resource();
    const double a  = args[0].elemAsDouble(0);
    const double b  = args[0].elemAsDouble(1);
    const Value &x  = args[1];
    const double nL = betalike(mr, a, b, x);
    outs[0] = Value::scalar(nL, mr);

    // Second output: 2×2 inverse Fisher info, parameter order [a, b].
    // MATLAB's betalike uses BHHH (outer-product-of-gradients) — the
    // sum of per-row score outer products — NOT the Hessian. Verified
    // by direct probe: at user-supplied params (away from MLE) the two
    // estimators differ; MATLAB / Octave both report the BHHH form.
    // Score per row:
    //   ∂log f/∂a = log x_i  - ψ(a) + ψ(a+b)
    //   ∂log f/∂b = log(1-x_i) - ψ(b) + ψ(a+b)
    if (nargout >= 2) {
        Value av = Value::matrix(2, 2, ValueType::DOUBLE, mr);
        double *p = av.doubleDataMut();
        const double NaNd = std::numeric_limits<double>::quiet_NaN();
        const size_t N = x.numel();
        if (!std::isfinite(nL) || !(a > 0.0) || !(b > 0.0) || N == 0) {
            p[0] = NaNd; p[1] = NaNd; p[2] = NaNd; p[3] = NaNd;
        } else {
            const double Ca = -digamma(a) + digamma(a + b);
            const double Cb = -digamma(b) + digamma(a + b);
            double Iaa = 0.0, Ibb = 0.0, Iab = 0.0;
            bool bad = false;
            for (size_t i = 0; i < N; ++i) {
                const double xi = x.elemAsDouble(i);
                if (xi <= 0.0 || xi >= 1.0) { bad = true; break; }
                const double sa = std::log(xi)    + Ca;
                const double sb = std::log1p(-xi) + Cb;
                Iaa += sa * sa;
                Ibb += sb * sb;
                Iab += sa * sb;
            }
            const double det = Iaa * Ibb - Iab * Iab;
            if (bad || det == 0.0 || !std::isfinite(det)) {
                p[0] = NaNd; p[1] = NaNd; p[2] = NaNd; p[3] = NaNd;
            } else {
                const double inv = 1.0 / det;
                p[0] =  Ibb * inv;
                p[1] = -Iab * inv;
                p[2] = -Iab * inv;
                p[3] =  Iaa * inv;
            }
        }
        outs[1] = std::move(av);
    }
}

void wbllike_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args[0].numel() < 2)
        throw Error("wbllike: requires (params=[scale shape], data[, cens, freq])",
                    0, 0, "wbllike", "", "m:wbllike:nargin");
    const double scale = args[0].elemAsDouble(0);
    const double shape = args[0].elemAsDouble(1);
    Value emptyVal = Value::matrix(0, 0, ValueType::DOUBLE, ctx.engine->resource());
    const Value &cens = (args.size() >= 3) ? args[2] : emptyVal;
    const Value &freq = (args.size() >= 4) ? args[3] : emptyVal;
    const double nL = wbllike_full(ctx.engine->resource(), scale, shape, args[1], cens, freq);
    outs[0] = Value::scalar(nL, ctx.engine->resource());
    // AVAR (2-output form): not yet implemented; observed Fisher info
    // for Weibull has nontrivial mixed partials. Deferred.
}

void evlike_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args[0].numel() < 2)
        throw Error("evlike: requires (params=[mu sigma], data[, cens, freq])",
                    0, 0, "evlike", "", "m:evlike:nargin");
    const double mu    = args[0].elemAsDouble(0);
    const double sigma = args[0].elemAsDouble(1);
    Value emptyVal = Value::matrix(0, 0, ValueType::DOUBLE, ctx.engine->resource());
    const Value &cens = (args.size() >= 3) ? args[2] : emptyVal;
    const Value &freq = (args.size() >= 4) ? args[3] : emptyVal;
    const double nL = evlike_full(ctx.engine->resource(), mu, sigma, args[1], cens, freq);
    outs[0] = Value::scalar(nL, ctx.engine->resource());
    // AVAR (2-output form): not yet implemented — observed Fisher info
    // for Gumbel-min has nontrivial cross-terms; deferred. See
    // audit/closed/stats/evlike.md for the partial-closure note.
}

void explike_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("explike: requires (mu, data[, cens, freq])",
                    0, 0, "explike", "", "m:explike:nargin");
    auto *mr = ctx.engine->resource();
    const double mu = args[0].toScalar();
    Value emptyVal = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    const Value &cens = (args.size() >= 3) ? args[2] : emptyVal;
    const Value &freq = (args.size() >= 4) ? args[3] : emptyVal;
    double avar = std::numeric_limits<double>::quiet_NaN();
    const double nL = explike_full(mu, args[1], cens, freq,
                                   nargout >= 2 ? &avar : nullptr);
    outs[0] = Value::scalar(nL, mr);
    if (nargout >= 2) outs[1] = Value::scalar(avar, mr);
}

void gevlike_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args[0].numel() < 3)
        throw Error("gevlike: requires (params=[k sigma mu], data)",
                    0, 0, "gevlike", "", "m:gevlike:nargin");
    auto *mr = ctx.engine->resource();
    const double k     = args[0].elemAsDouble(0);
    const double sigma = args[0].elemAsDouble(1);
    const double mu    = args[0].elemAsDouble(2);
    const Value &x     = args[1];
    const double nL = gevlike(mr, k, sigma, mu, x);
    outs[0] = Value::scalar(nL, mr);
    if (nargout >= 2) {
        Value ac = Value::matrix(3, 3, ValueType::DOUBLE, mr);
        fill_fd_avar3(ac.doubleDataMut(), k, sigma, mu, nL,
                      [&](double kk, double ss, double mm) {
                          return gevlike(mr, kk, ss, mm, x);
                      });
        outs[1] = std::move(ac);
    }
}

void gplike_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args[0].numel() < 2)
        throw Error("gplike: requires (params=[k sigma], data)",
                    0, 0, "gplike", "", "m:gplike:nargin");
    auto *mr = ctx.engine->resource();
    const double k     = args[0].elemAsDouble(0);
    const double sigma = args[0].elemAsDouble(1);
    const Value &x     = args[1];
    const double nL = gplike(mr, k, sigma, x);
    outs[0] = Value::scalar(nL, mr);
    if (nargout >= 2) {
        Value ac = Value::matrix(2, 2, ValueType::DOUBLE, mr);
        fill_fd_avar2(ac.doubleDataMut(), k, sigma, nL,
                      [&](double kk, double ss) { return gplike(mr, kk, ss, x); });
        outs[1] = std::move(ac);
    }
}

} // namespace detail
} // namespace numkit::stats
