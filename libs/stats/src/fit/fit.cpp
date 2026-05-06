// libs/stats/src/fit/fit.cpp
//
// Distribution MLE fitters with confidence intervals.

#include <numkit/stats/fit/fit.hpp>

#include <numkit/stats/distributions/chi2.hpp>
#include <numkit/stats/distributions/students_t.hpp>
#include <numkit/stats/distributions/beta.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

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
expfit(std::pmr::memory_resource *mr, const Value &x, double alpha)
{
    const size_t N = x.numel();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (N == 0) return {Value::scalar(nan, mr), rowCI(mr, nan, nan)};
    double S = 0.0;
    for (size_t i = 0; i < N; ++i) S += x.elemAsDouble(i);
    const double mu = S / double(N);
    // Exact CI: 2·N·muhat ~ μ·χ²(2N).
    const double chiU = chi2inv_scalar(mr, 1.0 - alpha / 2.0, 2.0 * double(N));
    const double chiL = chi2inv_scalar(mr,       alpha / 2.0, 2.0 * double(N));
    const double lo = 2.0 * double(N) * mu / chiU;
    const double hi = 2.0 * double(N) * mu / chiL;
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

std::tuple<Value, Value>
lognfit(std::pmr::memory_resource *mr, const Value &x, double alpha)
{
    const size_t N = x.numel();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    Value parm = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    Value pci  = Value::matrix(2, 2, ValueType::DOUBLE, mr);
    double *pd = parm.doubleDataMut();
    double *cd = pci.doubleDataMut();
    if (N < 2) {
        for (int i = 0; i < 2; ++i) pd[i] = nan;
        for (int i = 0; i < 4; ++i) cd[i] = nan;
        return {std::move(parm), std::move(pci)};
    }
    // Compute on log(x). Reject non-positive x by NaN.
    double s = 0.0;
    std::vector<double> lx(N);
    for (size_t i = 0; i < N; ++i) {
        const double xi = x.elemAsDouble(i);
        if (!(xi > 0.0)) {
            for (int j = 0; j < 2; ++j) pd[j] = nan;
            for (int j = 0; j < 4; ++j) cd[j] = nan;
            return {std::move(parm), std::move(pci)};
        }
        lx[i] = std::log(xi);
        s += lx[i];
    }
    const double mu = s / double(N);
    double sq = 0.0;
    for (double v : lx) { const double d = v - mu; sq += d * d; }
    const double var = sq / double(N - 1);
    const double sd  = std::sqrt(var);
    pd[0] = mu;  pd[1] = sd;

    const double t = tinv_scalar(mr, 1.0 - alpha / 2.0, double(N - 1));
    const double sem = sd / std::sqrt(double(N));
    const double mu_lo = mu - t * sem;
    const double mu_hi = mu + t * sem;
    const double chiU = chi2inv_scalar(mr, 1.0 - alpha / 2.0, double(N - 1));
    const double chiL = chi2inv_scalar(mr,       alpha / 2.0, double(N - 1));
    const double s_lo = std::sqrt(double(N - 1) * var / chiU);
    const double s_hi = std::sqrt(double(N - 1) * var / chiL);

    // pci is column-major: [mu_lo, mu_hi; sigma_lo, sigma_hi] is stored as
    // pci(1,1)=mu_lo pci(2,1)=mu_hi pci(1,2)=sigma_lo pci(2,2)=sigma_hi.
    cd[0] = mu_lo;  // (1,1)
    cd[1] = mu_hi;  // (2,1)
    cd[2] = s_lo;   // (1,2)
    cd[3] = s_hi;   // (2,2)
    return {std::move(parm), std::move(pci)};
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
    if (N == 0 || scale <= 0.0 || shape <= 0.0)
        return std::numeric_limits<double>::infinity();
    double sumLogX = 0.0, sumPow = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double xi = x.elemAsDouble(i);
        if (xi <= 0.0) return std::numeric_limits<double>::infinity();
        sumLogX += std::log(xi);
        sumPow  += std::pow(xi / scale, shape);
    }
    return -double(N) * std::log(shape) + double(N) * shape * std::log(scale)
         - (shape - 1.0) * sumLogX + sumPow;
}

double evlike(std::pmr::memory_resource * /*mr*/, double mu, double sigma,
              const Value &x)
{
    const size_t N = x.numel();
    if (N == 0 || sigma <= 0.0) return std::numeric_limits<double>::infinity();
    double sLin = 0.0, sExp = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double t = (x.elemAsDouble(i) - mu) / sigma;
        sLin += t;
        sExp += std::exp(t);
    }
    return double(N) * std::log(sigma) - sLin + sExp;
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
        throw Error("expfit: requires X[, alpha]",
                    0, 0, "expfit", "", "m:expfit:nargin");
    const double alpha = parse_alpha_arg(args, 1, 0.05);
    auto [mu, ci] = expfit(ctx.engine->resource(), args[0], alpha);
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
        throw Error("lognfit: requires X[, alpha]",
                    0, 0, "lognfit", "", "m:lognfit:nargin");
    const double alpha = parse_alpha_arg(args, 1, 0.05);
    auto [parm, pci] = lognfit(ctx.engine->resource(), args[0], alpha);
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
{ like2_reg("wbllike", &wbllike, args, outs, ctx); }

void evlike_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{ like2_reg("evlike", &evlike, args, outs, ctx); }

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
