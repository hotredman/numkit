// libs/signal/src/spectral_analysis/ar.cpp
//
// Parametric AR-model PSD estimators: pyulear and pburg. Both fit
// an AR(p) model to the input and evaluate
//
//   Pxx(f) = σ² / |1 + Σ a_k · e^{−jωk}|²
//
// on a one-sided ω ∈ [0, π] grid. They differ only in how the AR
// coefficients are estimated:
//
//   pyulear : Yule-Walker autocorrelation → Levinson-Durbin recursion
//   pburg   : minimise Σ (forward² + backward²) prediction error,
//             building a, σ², and the lattice reflection coefficients
//             jointly per Burg's update rule.
//
// Both give similar PSDs on stationary input; pburg is preferred on
// short signals because it doesn't bias σ² downward.

#include <numkit/signal/spectral_analysis/periodogram_pwelch.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::signal {

namespace {

void requireValidArgs(const Value &x, int p, const char *fn) {
    if (p < 1)
        throw Error(std::string(fn) + ": order p must be ≥ 1",
                    0, 0, fn, "", "m:ar:order");
    if (static_cast<size_t>(p) >= x.numel())
        throw Error(std::string(fn) +
                    ": order p must be < length(x)",
                    0, 0, fn, "", "m:ar:order");
}

// Evaluate Pxx(f) = σ² / |A(e^{jω})|² on a one-sided grid of nOut bins.
// `a` holds AR coefficients (a_1 … a_p), and `sigma2` the prediction-
// error variance. Returns (Pxx, F) Values sized nOut × 1.
std::tuple<Value, Value>
arSpectrum(std::pmr::memory_resource *mr,
           const std::vector<double> &a, double sigma2, size_t nfft)
{
    const size_t nOut = nfft / 2 + 1;
    Value Pxx = Value::matrix(nOut, 1, ValueType::DOUBLE, mr);
    Value F   = Value::matrix(nOut, 1, ValueType::DOUBLE, mr);
    double *pd = Pxx.doubleDataMut();
    double *fd = F.doubleDataMut();
    for (size_t k = 0; k < nOut; ++k) {
        const double w = M_PI * static_cast<double>(k) /
                                   static_cast<double>(nOut - 1);
        std::complex<double> A(1.0, 0.0);
        for (size_t i = 0; i < a.size(); ++i) {
            const std::complex<double> z =
                std::polar(1.0, -w * static_cast<double>(i + 1));
            A += a[i] * z;
        }
        const double mag2 = std::norm(A);
        pd[k] = (mag2 > 0.0) ? sigma2 / mag2 : 0.0;
        fd[k] = w;
    }
    return std::make_tuple(std::move(Pxx), std::move(F));
}

} // anonymous

std::tuple<Value, Value>
pyulear(std::pmr::memory_resource *mr, const Value &x, int p, size_t nfft)
{
    requireValidArgs(x, p, "pyulear");
    const size_t N = x.numel();
    const size_t pp = static_cast<size_t>(p);
    if (nfft == 0) nfft = (N >= 256) ? 256 : 256;

    // Biased autocorrelation r[k] = (1/N) Σ x[n]·x[n+k] for k=0..p.
    std::vector<double> r(pp + 1, 0.0);
    for (size_t k = 0; k <= pp; ++k) {
        double s = 0.0;
        for (size_t n = 0; n + k < N; ++n)
            s += x.elemAsDouble(n) * x.elemAsDouble(n + k);
        r[k] = s / static_cast<double>(N);
    }

    // Levinson-Durbin: solve r * a = -r[1..p], return (a[1..p], σ²).
    std::vector<double> a(pp, 0.0);
    double sigma2 = r[0];
    if (pp >= 1) {
        std::vector<double> aPrev(pp, 0.0);
        const double k1 = -r[1] / r[0];
        a[0] = k1;
        sigma2 = r[0] * (1.0 - k1 * k1);
        for (size_t m = 1; m < pp; ++m) {
            // kappa = -(r[m+1] + Σ_{k=0..m-1} a[k]·r[m-k]) / sigma2
            double num = r[m + 1];
            for (size_t k = 0; k < m; ++k) num += a[k] * r[m - k];
            const double kappa = -num / sigma2;
            // a' [k] = a[k] + kappa * a[m-1-k]   for k=0..m-1
            // a' [m] = kappa
            for (size_t k = 0; k < m; ++k) aPrev[k] = a[k];
            for (size_t k = 0; k < m; ++k)
                a[k] = aPrev[k] + kappa * aPrev[m - 1 - k];
            a[m] = kappa;
            sigma2 *= (1.0 - kappa * kappa);
        }
    }
    return arSpectrum(mr, a, sigma2, nfft);
}

std::tuple<Value, Value>
pburg(std::pmr::memory_resource *mr, const Value &x, int p, size_t nfft)
{
    requireValidArgs(x, p, "pburg");
    const size_t N = x.numel();
    const size_t pp = static_cast<size_t>(p);
    if (nfft == 0) nfft = (N >= 256) ? 256 : 256;

    // Forward / backward prediction error sequences (start = signal).
    std::vector<double> f(N), b(N);
    for (size_t n = 0; n < N; ++n) {
        const double v = x.elemAsDouble(n);
        f[n] = v;
        b[n] = v;
    }

    std::vector<double> a(pp, 0.0);
    std::vector<double> aPrev(pp, 0.0);

    // Initial prediction-error variance σ² = (1/N) Σ x²
    double sigma2 = 0.0;
    for (size_t n = 0; n < N; ++n) {
        const double v = x.elemAsDouble(n);
        sigma2 += v * v;
    }
    sigma2 /= static_cast<double>(N);

    for (size_t m = 0; m < pp; ++m) {
        // kappa = -2 · Σ f[n+1] · b[n]   /   Σ (f[n+1]² + b[n]²)
        // n ranges over 0..(N - m - 2).
        double num = 0.0, den = 0.0;
        for (size_t n = 0; n + 1 < N - m; ++n) {
            num += f[n + 1] * b[n];
            den += f[n + 1] * f[n + 1] + b[n] * b[n];
        }
        const double kappa = (den > 0.0) ? (-2.0 * num / den) : 0.0;

        // AR coefficient update: a' [k] = a[k] + kappa · a[m-1-k];
        // a' [m] = kappa.
        for (size_t k = 0; k < m; ++k) aPrev[k] = a[k];
        for (size_t k = 0; k < m; ++k)
            a[k] = aPrev[k] + kappa * aPrev[m - 1 - k];
        a[m] = kappa;
        sigma2 *= (1.0 - kappa * kappa);

        // Update forward / backward prediction errors in place.
        // f_new[n] = f[n+1] + kappa · b[n]
        // b_new[n] = b[n]   + kappa · f[n+1]
        // for n = 0..(N - m - 2). After this the active length is
        // N - m - 1.
        const size_t L = N - m - 1;
        std::vector<double> fNew(L), bNew(L);
        for (size_t n = 0; n < L; ++n) {
            fNew[n] = f[n + 1] + kappa * b[n];
            bNew[n] = b[n]     + kappa * f[n + 1];
        }
        f = std::move(fNew);
        b = std::move(bNew);
    }
    return arSpectrum(mr, a, sigma2, nfft);
}

namespace detail {

void pyulear_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pyulear: requires (x, p[, nfft])",
                    0, 0, "pyulear", "", "m:pyulear:nargin");
    const int p     = static_cast<int>(args[1].toScalar());
    const size_t nf = (args.size() >= 3 && !args[2].isEmpty())
                      ? static_cast<size_t>(args[2].toScalar()) : 0;
    auto [Pxx, F] = pyulear(ctx.engine->resource(), args[0], p, nf);
    outs[0] = std::move(Pxx);
    if (nargout > 1) outs[1] = std::move(F);
}

void pburg_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pburg: requires (x, p[, nfft])",
                    0, 0, "pburg", "", "m:pburg:nargin");
    const int p     = static_cast<int>(args[1].toScalar());
    const size_t nf = (args.size() >= 3 && !args[2].isEmpty())
                      ? static_cast<size_t>(args[2].toScalar()) : 0;
    auto [Pxx, F] = pburg(ctx.engine->resource(), args[0], p, nf);
    outs[0] = std::move(Pxx);
    if (nargout > 1) outs[1] = std::move(F);
}

} // namespace detail

} // namespace numkit::signal
