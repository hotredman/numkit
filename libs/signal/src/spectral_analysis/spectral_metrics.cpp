// libs/signal/src/spectral_analysis/spectral_metrics.cpp
//
// Spectral measurement functions on power spectral densities. All take
// either a time-series (PSD computed via periodogram with rectangular
// window) or, in select cases, a pre-computed (Pxx, F).
//
// Frequency convention: when fs is omitted, frequencies run [0, π] and
// power-band integrations yield rad/s. When fs is supplied, freqs run
// [0, fs/2] and integrations yield Hz.

#include <numkit/signal/spectral_analysis/spectral_metrics.hpp>
#include <numkit/signal/spectral_analysis/periodogram_pwelch.hpp>
#include <numkit/signal/transforms/hilbert.hpp>
#include <numkit/signal/windows/windows.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::signal {

namespace {

double scalarOr(const Value *v, double dflt)
{
    return (v && !v->isEmpty()) ? v->toScalar() : dflt;
}

// Compute single-segment periodogram of x, returning (Pxx, F) where F
// is in physical units: [0, fs/2] when fs > 0, else [0, π] when fs <= 0.
struct PsdPair {
    std::vector<double> Pxx;
    std::vector<double> F;
};

PsdPair computePsd(std::pmr::memory_resource *mr, const Value &x, double fs)
{
    // The existing `periodogram` returns Pxx normalised by 1/(N·nfft)
    // and F on [0, π]. That convention is convenient for plotting but
    // does NOT obey Parseval: ∫ Pxx df ≠ mean(x²). To produce a
    // canonical Hz-PSD where ∫ Pxx df = mean(x²) (matching MATLAB's
    // periodogram(x, [], [], fs)), we renormalise here.
    //
    // For a real input of length N with rectangular window:
    //   Pxx_existing[k] = (2 if inner else 1) * |X[k]|² / (N * nfft)
    //   F_rad[k]        = π·k / (nOut - 1),   nOut = nfft/2 + 1
    //
    // Canonical MATLAB Pxx_Hz with df_Hz = fs / nfft satisfies
    //   sum(Pxx_Hz) * df_Hz = mean(x²) = sum_one_sided(|X|²) / N²
    // so Pxx_Hz[k] = (2 if inner else 1) * |X[k]|² / (N · fs).
    //
    // Conversion factor: Pxx_Hz = Pxx_existing * (nfft / fs).
    // (Equivalently, Pxx_rad_canonical = Pxx_existing * nfft / (2π).)
    Value win;  // empty → rectangular
    auto [Pxx, F] = periodogram(mr, x, win, /*nfft=*/0);
    const size_t n = Pxx.numel();
    PsdPair p;
    p.Pxx.resize(n);
    p.F.resize(n);

    // Recover nfft from nOut (= nfft/2 + 1 → nfft = 2*(n-1)).
    const size_t nfft = (n > 1) ? 2 * (n - 1) : 1;

    if (fs > 0.0) {
        const double fScale = fs / (2.0 * M_PI);            // F: rad → Hz
        const double pScale = static_cast<double>(nfft) / fs; // Pxx: → power/Hz
        const double *fd = F.doubleData();
        const double *pd = Pxx.doubleData();
        for (size_t i = 0; i < n; ++i) {
            p.F[i]   = fd[i] * fScale;
            p.Pxx[i] = pd[i] * pScale;
        }
    } else {
        // No fs: keep [0, π] but renormalise so ∫ P df_rad = mean(x²).
        // Pxx_rad = Pxx_existing * nfft / (2π).
        const double pScale = static_cast<double>(nfft) / (2.0 * M_PI);
        const double *fd = F.doubleData();
        const double *pd = Pxx.doubleData();
        for (size_t i = 0; i < n; ++i) {
            p.F[i]   = fd[i];
            p.Pxx[i] = pd[i] * pScale;
        }
    }
    return p;
}

// Trapezoidal integration of P over the frequency grid F, with a band
// restriction to [fLo, fHi] (inclusive). Endpoints are linearly
// interpolated when they fall inside an interval.
double integrate(const std::vector<double> &P, const std::vector<double> &F,
                 double fLo, double fHi)
{
    const size_t n = P.size();
    if (n < 2) return 0.0;
    if (fLo > fHi) std::swap(fLo, fHi);
    double s = 0.0;
    for (size_t i = 0; i + 1 < n; ++i) {
        double a = F[i], b = F[i + 1];
        if (b <= fLo) continue;
        if (a >= fHi) break;
        double pa = P[i], pb = P[i + 1];
        // Clip interval to band.
        if (a < fLo) {
            const double t = (fLo - a) / (b - a);
            pa = pa + t * (pb - pa);
            a = fLo;
        }
        if (b > fHi) {
            const double t = (fHi - a) / (F[i + 1] - F[i]);
            pb = P[i] + t * (P[i + 1] - P[i]);
            b = fHi;
        }
        s += 0.5 * (pa + pb) * (b - a);
    }
    return s;
}

} // anonymous

// ── bandpower ──────────────────────────────────────────────────────

Value bandpower(std::pmr::memory_resource *mr, const Value &x,
                const Value *fs, const Value *freqrange)
{
    // 1-arg: total signal power = mean(|x|^2). Cheaper than going
    // through PSD (Parseval's theorem makes them equal anyway).
    if (!fs || fs->isEmpty()) {
        const size_t n = x.numel();
        if (n == 0) return Value::scalar(0.0, mr);
        double s = 0.0;
        for (size_t i = 0; i < n; ++i) {
            const double v = x.elemAsDouble(i);
            s += v * v;
        }
        return Value::scalar(s / n, mr);
    }
    const double fsv = fs->toScalar();
    auto p = computePsd(mr, x, fsv);
    double fLo = p.F.front();
    double fHi = p.F.back();
    if (freqrange && !freqrange->isEmpty() && freqrange->numel() >= 2) {
        fLo = freqrange->elemAsDouble(0);
        fHi = freqrange->elemAsDouble(1);
    }
    return Value::scalar(integrate(p.Pxx, p.F, fLo, fHi), mr);
}

// ── meanfreq / medfreq ────────────────────────────────────────────

Value meanfreq(std::pmr::memory_resource *mr, const Value &x, const Value *fs)
{
    const double fsv = scalarOr(fs, 0.0);
    auto p = computePsd(mr, x, fsv);
    double num = 0.0, den = 0.0;
    for (size_t i = 0; i + 1 < p.F.size(); ++i) {
        const double df = p.F[i + 1] - p.F[i];
        const double pmid = 0.5 * (p.Pxx[i] + p.Pxx[i + 1]);
        const double fmid = 0.5 * (p.F[i] + p.F[i + 1]);
        num += fmid * pmid * df;
        den += pmid * df;
    }
    return Value::scalar(den > 0 ? num / den : 0.0, mr);
}

Value medfreq(std::pmr::memory_resource *mr, const Value &x, const Value *fs)
{
    const double fsv = scalarOr(fs, 0.0);
    auto p = computePsd(mr, x, fsv);
    if (p.F.size() < 2) return Value::scalar(0.0, mr);
    // Cumulative trapezoid of P over F.
    std::vector<double> cum(p.F.size(), 0.0);
    for (size_t i = 1; i < p.F.size(); ++i) {
        cum[i] = cum[i - 1]
               + 0.5 * (p.Pxx[i - 1] + p.Pxx[i]) * (p.F[i] - p.F[i - 1]);
    }
    const double half = 0.5 * cum.back();
    if (cum.back() <= 0.0) return Value::scalar(0.0, mr);
    // Linear-interp the frequency where cum == half.
    for (size_t i = 1; i < cum.size(); ++i) {
        if (cum[i] >= half) {
            const double t = (half - cum[i - 1]) / (cum[i] - cum[i - 1]);
            return Value::scalar(p.F[i - 1] + t * (p.F[i] - p.F[i - 1]), mr);
        }
    }
    return Value::scalar(p.F.back(), mr);
}

// ── enbw ──────────────────────────────────────────────────────────

Value enbw(std::pmr::memory_resource *mr, const Value &window, const Value *fs)
{
    const size_t n = window.numel();
    if (n == 0) return Value::scalar(0.0, mr);
    double sumsq = 0.0, sm = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double w = window.elemAsDouble(i);
        sumsq += w * w;
        sm    += w;
    }
    const double bin = (sm * sm > 0) ? n * sumsq / (sm * sm) : 0.0;
    const double fsv = scalarOr(fs, static_cast<double>(n));
    return Value::scalar(bin * fsv / n, mr);
}

// ── obw ───────────────────────────────────────────────────────────

Value obw(std::pmr::memory_resource *mr, const Value &x,
          const Value *fs, double p)
{
    const double fsv = scalarOr(fs, 0.0);
    auto psd = computePsd(mr, x, fsv);
    if (psd.F.size() < 2) return Value::scalar(0.0, mr);
    std::vector<double> cum(psd.F.size(), 0.0);
    for (size_t i = 1; i < psd.F.size(); ++i) {
        cum[i] = cum[i - 1]
               + 0.5 * (psd.Pxx[i - 1] + psd.Pxx[i]) * (psd.F[i] - psd.F[i - 1]);
    }
    const double total = cum.back();
    if (total <= 0.0) return Value::scalar(0.0, mr);
    const double pLow = (1.0 - p) * 0.5 * total;
    const double pHigh = (1.0 + p) * 0.5 * total;
    auto findF = [&](double target) {
        for (size_t i = 1; i < cum.size(); ++i) {
            if (cum[i] >= target) {
                const double t = (target - cum[i - 1])
                               / std::max(cum[i] - cum[i - 1], 1e-300);
                return psd.F[i - 1] + t * (psd.F[i] - psd.F[i - 1]);
            }
        }
        return psd.F.back();
    };
    return Value::scalar(findF(pHigh) - findF(pLow), mr);
}

// ── powerbw ───────────────────────────────────────────────────────

Value powerbw(std::pmr::memory_resource *mr, const Value &x, const Value *fs)
{
    const double fsv = scalarOr(fs, 0.0);
    auto p = computePsd(mr, x, fsv);
    if (p.F.size() < 2) return Value::scalar(0.0, mr);
    // Find dominant peak.
    size_t pk = 0;
    double pkV = p.Pxx[0];
    for (size_t i = 1; i < p.Pxx.size(); ++i)
        if (p.Pxx[i] > pkV) { pkV = p.Pxx[i]; pk = i; }
    const double thresh = pkV * 0.5;
    // Walk left and right from pk to find the half-power crossings.
    auto interpCross = [&](size_t i, size_t j) {
        const double a = p.Pxx[i], b = p.Pxx[j];
        if (a == b) return p.F[i];
        const double t = (thresh - a) / (b - a);
        return p.F[i] + t * (p.F[j] - p.F[i]);
    };
    double fLo = p.F.front(), fHi = p.F.back();
    for (size_t i = pk; i > 0; --i)
        if (p.Pxx[i - 1] < thresh) { fLo = interpCross(i - 1, i); break; }
    for (size_t i = pk; i + 1 < p.Pxx.size(); ++i)
        if (p.Pxx[i + 1] < thresh) { fHi = interpCross(i, i + 1); break; }
    return Value::scalar(fHi - fLo, mr);
}

// ── spectral shape statistics ─────────────────────────────────────
// All of these treat the normalised PSD as a probability distribution
// over frequency.

namespace {

struct PsdMoments {
    double sumP;
    double mean;
    double var;
    double m3;   // third central moment
    double m4;   // fourth central moment
};

PsdMoments computePsdMoments(const std::vector<double> &P,
                             const std::vector<double> &F)
{
    double sumP = 0.0;
    for (double v : P) sumP += v;
    if (sumP <= 0.0) return {0.0, 0.0, 0.0, 0.0, 0.0};
    double mean = 0.0;
    for (size_t i = 0; i < P.size(); ++i) mean += F[i] * P[i];
    mean /= sumP;
    double var = 0.0, m3 = 0.0, m4 = 0.0;
    for (size_t i = 0; i < P.size(); ++i) {
        const double d = F[i] - mean;
        var += d * d * P[i];
        m3  += d * d * d * P[i];
        m4  += d * d * d * d * P[i];
    }
    var /= sumP;
    m3  /= sumP;
    m4  /= sumP;
    return {sumP, mean, var, m3, m4};
}

} // anonymous

Value spectralcrest(std::pmr::memory_resource *mr, const Value &x, const Value *fs)
{
    const double fsv = scalarOr(fs, 0.0);
    auto p = computePsd(mr, x, fsv);
    if (p.Pxx.empty()) return Value::scalar(0.0, mr);
    double mx = p.Pxx[0], sm = 0.0;
    for (double v : p.Pxx) { if (v > mx) mx = v; sm += v; }
    const double mean = sm / p.Pxx.size();
    return Value::scalar(mean > 0 ? mx / mean : 0.0, mr);
}

Value spectralflatness(std::pmr::memory_resource *mr, const Value &x, const Value *fs)
{
    const double fsv = scalarOr(fs, 0.0);
    auto p = computePsd(mr, x, fsv);
    if (p.Pxx.empty()) return Value::scalar(0.0, mr);
    double logSum = 0.0, sm = 0.0;
    size_t valid = 0;
    for (double v : p.Pxx) {
        sm += v;
        if (v > 0) { logSum += std::log(v); ++valid; }
    }
    if (valid == 0 || sm <= 0.0) return Value::scalar(0.0, mr);
    const double geom = std::exp(logSum / valid);
    const double arith = sm / p.Pxx.size();
    return Value::scalar(geom / arith, mr);
}

Value spectralentropy(std::pmr::memory_resource *mr, const Value &x, const Value *fs)
{
    const double fsv = scalarOr(fs, 0.0);
    auto p = computePsd(mr, x, fsv);
    if (p.Pxx.empty()) return Value::scalar(0.0, mr);
    double sm = 0.0;
    for (double v : p.Pxx) sm += v;
    if (sm <= 0.0) return Value::scalar(0.0, mr);
    double H = 0.0;
    for (double v : p.Pxx) {
        const double q = v / sm;
        if (q > 0) H -= q * std::log(q);
    }
    // Normalise by log(N) so the result lies in [0, 1].
    return Value::scalar(H / std::log(static_cast<double>(p.Pxx.size())), mr);
}

Value spectralkurtosis(std::pmr::memory_resource *mr, const Value &x, const Value *fs)
{
    const double fsv = scalarOr(fs, 0.0);
    auto p = computePsd(mr, x, fsv);
    auto m = computePsdMoments(p.Pxx, p.F);
    if (m.var <= 0.0) return Value::scalar(0.0, mr);
    return Value::scalar(m.m4 / (m.var * m.var), mr);
}

Value spectralskewness(std::pmr::memory_resource *mr, const Value &x, const Value *fs)
{
    const double fsv = scalarOr(fs, 0.0);
    auto p = computePsd(mr, x, fsv);
    auto m = computePsdMoments(p.Pxx, p.F);
    if (m.var <= 0.0) return Value::scalar(0.0, mr);
    return Value::scalar(m.m3 / std::pow(m.var, 1.5), mr);
}

// ════════════════════════════════════════════════════════════════════
// Harmonic / SNR family
// ════════════════════════════════════════════════════════════════════
//
// Strategy: compute a Kaiser-windowed periodogram (matches MATLAB's
// default snr: Kaiser(N, 38) gives near-zero sidelobes so leakage
// stays below realistic noise floors), find the dominant peak skipping
// a few DC bins, then sum a "skirt" of `kSkirtBins` bins on each side
// as the fundamental power. Repeat for harmonics. Noise = remainder.

namespace {

PsdPair computePsdKaiser(std::pmr::memory_resource *mr, const Value &x, double fs)
{
    // Build Kaiser(beta=38) window of length numel(x).
    Value win = kaiser(x.numel(), 38.0, mr);
    auto [Pxx, F] = periodogram(mr, x, win, /*nfft=*/0);
    const size_t n = Pxx.numel();
    PsdPair p;
    p.Pxx.resize(n);
    p.F.resize(n);
    const size_t nfft = (n > 1) ? 2 * (n - 1) : 1;
    if (fs > 0.0) {
        const double fScale = fs / (2.0 * M_PI);
        const double pScale = static_cast<double>(nfft) / fs;
        for (size_t i = 0; i < n; ++i) {
            p.F[i]   = F.doubleData()[i] * fScale;
            p.Pxx[i] = Pxx.doubleData()[i] * pScale;
        }
    } else {
        const double pScale = static_cast<double>(nfft) / (2.0 * M_PI);
        for (size_t i = 0; i < n; ++i) {
            p.F[i]   = F.doubleData()[i];
            p.Pxx[i] = Pxx.doubleData()[i] * pScale;
        }
    }
    return p;
}

constexpr int kSkirtBins  = 8;   // half-width of fundamental skirt
constexpr int kHarmonics  = 6;   // number of harmonics to track
constexpr int kDcBins     = 8;   // DC bins to ignore when finding peak

// Sum P over [lo, hi) inclusive of lo, exclusive of hi (uses raw bin
// values; for narrow peaks this is close to the trapezoidal estimate
// times df, scaled identically across all sums so the ratio in dB is
// unchanged).
double sumBins(const std::vector<double> &P, int lo, int hi)
{
    if (lo < 0) lo = 0;
    if (hi > static_cast<int>(P.size())) hi = static_cast<int>(P.size());
    double s = 0.0;
    for (int i = lo; i < hi; ++i) s += P[i];
    return s;
}

// Find the bin of the dominant peak, ignoring the first kDcBins.
int findFundamentalBin(const std::vector<double> &P)
{
    int pk = kDcBins;
    double pkV = -1.0;
    for (int i = kDcBins; i < static_cast<int>(P.size()); ++i)
        if (P[i] > pkV) { pkV = P[i]; pk = i; }
    return pk;
}

struct HarmonicAnalysis {
    double pSig;     // fundamental power
    double pHarm;    // sum of harmonic powers
    double pNoise;   // remainder (everything else, excluding DC bins)
    double pSpurMax; // largest single spur OUTSIDE the fundamental skirt
    int    f0Bin;
};

HarmonicAnalysis analyseHarmonics(const std::vector<double> &P)
{
    const int nbins = static_cast<int>(P.size());
    HarmonicAnalysis r{0, 0, 0, 0, 0};
    if (nbins < 2) return r;

    r.f0Bin = findFundamentalBin(P);
    const int f0 = r.f0Bin;

    // Mark which bins belong to the fundamental skirt or a harmonic
    // skirt. Everything else contributes to noise (after skipping DC
    // bins at the start).
    std::vector<uint8_t> mark(nbins, 0); // 1=fund, 2=harm
    for (int j = std::max(0, f0 - kSkirtBins);
         j < std::min(nbins, f0 + kSkirtBins + 1); ++j) mark[j] = 1;
    for (int n = 2; n <= kHarmonics; ++n) {
        const int bin = n * f0;
        if (bin >= nbins) break;
        for (int j = std::max(0, bin - kSkirtBins);
             j < std::min(nbins, bin + kSkirtBins + 1); ++j) {
            if (mark[j] == 0) mark[j] = 2;
        }
    }

    for (int i = 0; i < nbins; ++i) {
        if (i < kDcBins && mark[i] == 0) continue; // ignore DC noise contrib
        if      (mark[i] == 1) r.pSig  += P[i];
        else if (mark[i] == 2) r.pHarm += P[i];
        else                   r.pNoise += P[i];
    }

    // Largest spur: scan for local maxima OUTSIDE the fundamental skirt
    // and take the biggest peak value (not its surrounding bin sum —
    // sfdr is a single-bin / peak-to-peak metric).
    for (int i = 1; i + 1 < nbins; ++i) {
        if (mark[i] == 1) continue;        // skip fundamental skirt
        if (i < kDcBins) continue;
        if (P[i] >= P[i - 1] && P[i] >= P[i + 1] && P[i] > r.pSpurMax)
            r.pSpurMax = P[i];
    }
    return r;
}

double safeDb(double num, double den)
{
    if (num <= 0.0 || den <= 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    return 10.0 * std::log10(num / den);
}

} // anonymous

Value snr(std::pmr::memory_resource *mr, const Value &x, const Value *fs)
{
    const double fsv = scalarOr(fs, 0.0);
    auto p = computePsdKaiser(mr, x, fsv);
    auto h = analyseHarmonics(p.Pxx);
    return Value::scalar(safeDb(h.pSig, h.pNoise), mr);
}

Value sinad(std::pmr::memory_resource *mr, const Value &x, const Value *fs)
{
    const double fsv = scalarOr(fs, 0.0);
    auto p = computePsdKaiser(mr, x, fsv);
    auto h = analyseHarmonics(p.Pxx);
    return Value::scalar(safeDb(h.pSig, h.pNoise + h.pHarm), mr);
}

Value thd(std::pmr::memory_resource *mr, const Value &x, const Value *fs)
{
    const double fsv = scalarOr(fs, 0.0);
    auto p = computePsdKaiser(mr, x, fsv);
    auto h = analyseHarmonics(p.Pxx);
    return Value::scalar(safeDb(h.pHarm, h.pSig), mr);
}

Value sfdr(std::pmr::memory_resource *mr, const Value &x, const Value *fs)
{
    const double fsv = scalarOr(fs, 0.0);
    auto p = computePsdKaiser(mr, x, fsv);
    auto h = analyseHarmonics(p.Pxx);
    // sfdr compares the fundamental peak amplitude to the largest
    // spurious peak. P[fundamental_bin] is the peak power.
    const double pPeak = (h.f0Bin >= 0 && h.f0Bin < (int)p.Pxx.size())
                         ? p.Pxx[h.f0Bin] : 0.0;
    return Value::scalar(safeDb(pPeak, h.pSpurMax), mr);
}

// ════════════════════════════════════════════════════════════════════
// Instantaneous frequency / bandwidth (analytic-signal-based)
// ════════════════════════════════════════════════════════════════════

namespace {

// Read complex Value (output of hilbert) into separate re/im vectors.
void readComplex(const Value &z, std::vector<double> &re, std::vector<double> &im)
{
    const size_t n = z.numel();
    re.resize(n); im.resize(n);
    if (z.type() == ValueType::COMPLEX) {
        const Complex *c = z.complexData();
        for (size_t i = 0; i < n; ++i) { re[i] = c[i].real(); im[i] = c[i].imag(); }
    } else {
        for (size_t i = 0; i < n; ++i) {
            re[i] = z.elemAsDouble(i);
            im[i] = 0.0;
        }
    }
}

} // anonymous

Value instfreq(std::pmr::memory_resource *mr, const Value &x, const Value *fs)
{
    const double fsv = scalarOr(fs, 1.0);
    Value z = hilbert(mr, x);
    std::vector<double> re, im;
    readComplex(z, re, im);
    const size_t n = re.size();
    if (n < 2) return Value::matrix(0, 1, ValueType::DOUBLE, mr);

    // Centred-difference of unwrapped phase. We avoid a full unwrap by
    // computing the phase delta between consecutive samples directly
    // and wrapping into (-π, π]; this is what `unwrap` would yield on
    // the local difference.
    //
    // numkit's hilbert produces the conjugate analytic signal pattern
    // (verified by smoke test: hilbert(cos θ) ≈ cos θ - i sin θ on the
    // signal interior). We negate the phase delta so that a positive-
    // frequency signal yields positive instantaneous frequency.
    auto out = Value::matrix(n - 1, 1, ValueType::DOUBLE, mr);
    double *d = out.doubleDataMut();
    constexpr double kPi = 3.14159265358979323846;
    for (size_t i = 0; i + 1 < n; ++i) {
        const double p1 = std::atan2(im[i + 1], re[i + 1]);
        const double p0 = std::atan2(im[i], re[i]);
        double dp = p1 - p0;
        while (dp >  kPi) dp -= 2.0 * kPi;
        while (dp <= -kPi) dp += 2.0 * kPi;
        d[i] = -dp * fsv / (2.0 * kPi);
    }
    return out;
}

Value instbw(std::pmr::memory_resource *mr, const Value &x, const Value *fs)
{
    const double fsv = scalarOr(fs, 1.0);
    Value z = hilbert(mr, x);
    std::vector<double> re, im;
    readComplex(z, re, im);
    const size_t n = re.size();
    if (n < 2) return Value::matrix(0, 1, ValueType::DOUBLE, mr);

    // Envelope A[i] = sqrt(re² + im²); inst-BW per MATLAB:
    //   B(t) = (1 / (2π)) * |dA/dt| / A(t)
    auto out = Value::matrix(n - 1, 1, ValueType::DOUBLE, mr);
    double *d = out.doubleDataMut();
    constexpr double kPi = 3.14159265358979323846;
    std::vector<double> A(n);
    for (size_t i = 0; i < n; ++i) A[i] = std::hypot(re[i], im[i]);
    for (size_t i = 0; i + 1 < n; ++i) {
        const double dA = (A[i + 1] - A[i]) * fsv;  // per-second
        const double Aavg = 0.5 * (A[i + 1] + A[i]);
        d[i] = (Aavg > 0.0) ? std::abs(dA) / (2.0 * kPi * Aavg) : 0.0;
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void bandpower_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bandpower: requires at least 1 argument",
                     0, 0, "bandpower", "", "m:bandpower:nargin");
    const Value *fs = (args.size() >= 2 && !args[1].isEmpty()) ? &args[1] : nullptr;
    const Value *fr = (args.size() >= 3 && !args[2].isEmpty()) ? &args[2] : nullptr;
    outs[0] = bandpower(ctx.engine->resource(), args[0], fs, fr);
}

void obw_reg(Span<const Value> args, size_t /*nargout*/,
             Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("obw: requires at least 1 argument",
                     0, 0, "obw", "", "m:obw:nargin");
    const Value *fs = (args.size() >= 2 && !args[1].isEmpty()) ? &args[1] : nullptr;
    double p = 0.99;
    if (args.size() >= 3 && !args[2].isEmpty()) p = args[2].toScalar();
    outs[0] = obw(ctx.engine->resource(), args[0], fs, p);
}

#define NK_SPEC1_REG(name, fn)                                                  \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires at least 1 argument",                 \
                         0, 0, #name, "", "m:" #name ":nargin");                 \
        const Value *fs = (args.size() >= 2 && !args[1].isEmpty())              \
                            ? &args[1] : nullptr;                                \
        outs[0] = fn(ctx.engine->resource(), args[0], fs);                      \
    }

NK_SPEC1_REG(meanfreq,         meanfreq)
NK_SPEC1_REG(medfreq,          medfreq)
NK_SPEC1_REG(enbw,             enbw)
NK_SPEC1_REG(powerbw,          powerbw)
NK_SPEC1_REG(spectralcrest,    spectralcrest)
NK_SPEC1_REG(spectralflatness, spectralflatness)
NK_SPEC1_REG(spectralentropy,  spectralentropy)
NK_SPEC1_REG(spectralkurtosis, spectralkurtosis)
NK_SPEC1_REG(spectralskewness, spectralskewness)
NK_SPEC1_REG(snr,              snr)
NK_SPEC1_REG(sinad,            sinad)
NK_SPEC1_REG(thd,              thd)
NK_SPEC1_REG(sfdr,             sfdr)
NK_SPEC1_REG(instfreq,         instfreq)
NK_SPEC1_REG(instbw,           instbw)

#undef NK_SPEC1_REG

} // namespace detail
} // namespace numkit::signal
