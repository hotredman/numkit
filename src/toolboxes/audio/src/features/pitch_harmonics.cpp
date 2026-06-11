// toolboxes/audio/src/features/pitch_harmonics.cpp
// Audio Cycle E: pitch + harmonicRatio.
// pitch (NCF method, MATLAB R2025b default):
//   For each frame, compute autocorrelation R[k]; normalize via
//   R[k] / sqrt(totalPower * partialPower(k)); search peak in valid
//   lag range [fs/maxF, fs/minF] where Range = [50, 400] Hz default.
//   f0 = fs / peakLag.
//   Defaults: Window = hamming(round(0.052*fs)) (~52 ms),
//             Overlap = round(0.042*fs)            (~42 ms),
//             Range   = [50, 400] Hz.
// harmonicRatio:
//   Same autocorrelation + normalization as pitch's NCF, but the
//   metric is the MAX of the normalized correlation in the valid lag
//   range (rather than the lag of the peak). Range [0, 1] roughly.
//   Defaults: Window = hamming(round(0.03*fs)), Overlap = round(0.02*fs).
// PMR HARD RULE.
// KNOWN GAPs (post cycles E-F-K-K2-K3):
//   * pitch shipped methods: NCF (default, cycle E), CEP (cycle K),
//     PEF (cycle K-2), LHS (cycle K-3). SRH still deferred (uses LPC
//     residual + Blackman-windowed framing — separate algorithm).
//   * Default median filter (MedianFilterLength=1) — applied with len=1
//     it's a no-op so we omit it.
//   * harmonicRatio: full MATLAB R2025b parity — auto low-edge from first
//     sign change of R[k] + Smith's parabolic peak interpolation. Tiny
//     ~2e-4 mean diff vs MATLAB on pure tones is FP-ordering noise.

#include <numkit/audio/features/pitch_harmonics.hpp>
#include <numkit/signal/transforms/fft.hpp>
#include <numkit/signal/spectral_analysis/signal_modeling.hpp>  // lpc
#include <numkit/signal/digital_filtering/filter.hpp>           // filter
#include <numkit/signal/windows/windows.hpp>                    // hann, blackman

// Compute-only TU: Value substrate + Error, no engine. The pitch /
// harmonicRatio builtins (CallContext wrappers) live in
// features/pitch_harmonics_reg.cpp.
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::audio {

namespace {

// Periodic Hamming window: w[n] = 0.54 - 0.46 * cos(2π·n/N).
void hammingPeriodic(double *w, size_t N)
{
    for (size_t n = 0; n < N; ++n)
        w[n] = 0.54 - 0.46 * std::cos(2.0 * M_PI * static_cast<double>(n)
                                        / static_cast<double>(N));
}

// Compute autocorrelation R[k] = Σ x[n]·x[n+k] for k = 0..maxLag-1.
// O(L * maxLag) — adequate for typical winLen ≤ 1024 and maxLag ≤ 320.
void autocorr(const double *x, size_t L, size_t maxLag, double *R)
{
    for (size_t k = 0; k < maxLag; ++k) {
        double s = 0.0;
        for (size_t n = 0; n + k < L; ++n) s += x[n] * x[n + k];
        R[k] = s;
    }
}

// Partial power Σ x[n+k]² for n = 0..L-k-1, k = 0..maxLag-1.
void partialPower(const double *x, size_t L, size_t maxLag, double *P)
{
    for (size_t k = 0; k < maxLag; ++k) {
        double s = 0.0;
        for (size_t n = 0; n + k < L; ++n) s += x[n + k] * x[n + k];
        P[k] = s;
    }
}

struct FrameSpec { size_t winLen, overlap, hop, numFrames; };

FrameSpec frameSpec(size_t N, double fs, double winSec, double ovSec)
{
    FrameSpec s;
    s.winLen  = static_cast<size_t>(std::round(fs * winSec));
    s.overlap = static_cast<size_t>(std::round(fs * ovSec));
    if (s.overlap >= s.winLen && s.winLen > 0) s.overlap = s.winLen - 1;
    s.hop = (s.winLen > s.overlap) ? (s.winLen - s.overlap) : 1;
    s.numFrames = (N >= s.winLen) ? ((N - s.winLen) / s.hop + 1) : 0;
    return s;
}

// 2^nextpow2(x) — smallest power of 2 >= x.
size_t nextPow2(size_t x)
{
    if (x <= 1) return 1;
    size_t p = 1;
    while (p < x) p <<= 1;
    return p;
}

} // anon

// ── pitch CEP method ──────────────────────────────────────────────────
// Cepstrum-based fundamental-frequency (pitch) estimation.
// Reference: A. M. Noll, "Cepstrum Pitch Determination", Journal of the
// Acoustical Society of America 41(2):293-309, 1967.
// Clean-room reimplementation. Per
// frame: window, zero-pad, DFT, log power spectrum, inverse DFT back to
// the quefrency domain; the cepstral peak inside the quefrency band for
// [minF, maxF] gives f0. The frame is zero-padded to nextPow2(2*winLen-1)
// so the cepstrum is free of time-domain aliasing.
// Compatibility: MATLAB's CEP reports the period from a 1-based
// quefrency index, so a cepstrum sample at 0-based array index q
// corresponds to a period of (q+1) samples; hence f0 = fs/(q+1) and the
// search band is shifted down by one index.
Value pitchCEP(const Value &x, double fs, double minF, double maxF,
               std::pmr::memory_resource *mr)
{
    // Framing — MATLAB pitch defaults: 52 ms window, 42 ms overlap.
    const std::size_t N = x.numel();
    const FrameSpec fr = frameSpec(N, fs, 0.052, 0.042);
    const std::size_t winLen    = fr.winLen;
    const std::size_t hop       = fr.hop;
    const std::size_t numFrames = fr.numFrames;
    if (numFrames == 0 || winLen == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // Zero-pad to a power of two >= 2*winLen-1 (anti-aliasing the cepstrum).
    const std::size_t NFFT = nextPow2(2 * winLen - 1);

    ScratchArena arena(mr);
    ScratchVec<double> win(winLen, &arena);
    hammingPeriodic(win.data(), winLen);

    // Column f holds the f-th windowed, zero-padded frame.
    Value frames = Value::matrix(NFFT, numFrames, ValueType::DOUBLE, &arena);
    double *fd = frames.doubleDataMut();
    std::fill(fd, fd + NFFT * numFrames, 0.0);
    for (std::size_t f = 0; f < numFrames; ++f) {
        const std::size_t base = f * hop;
        double *col = fd + f * NFFT;
        for (std::size_t n = 0; n < winLen; ++n) {
            const std::size_t idx = base + n;
            col[n] = (idx < N) ? x.elemAsDouble(idx) * win[n] : 0.0;
        }
    }

    // Forward DFT of every frame, then the log power spectrum.
    Value Y = signal::fft(frames, static_cast<int>(NFFT), 1, &arena);
    Value logspec = Value::matrix(NFFT, numFrames, ValueType::DOUBLE, &arena);
    double *ld = logspec.doubleDataMut();
    const double kLogFloor = std::log(std::numeric_limits<double>::min());
    const std::size_t total = NFFT * numFrames;
    if (Y.type() == ValueType::COMPLEX) {
        const Complex *yd = Y.complexData();
        for (std::size_t i = 0; i < total; ++i) {
            const double re = yd[i].real(), im = yd[i].imag();
            const double p2 = re * re + im * im;
            ld[i] = (p2 > 0.0) ? std::log(p2) : kLogFloor;
        }
    } else {
        const double *yd = Y.doubleData();
        for (std::size_t i = 0; i < total; ++i) {
            const double p2 = yd[i] * yd[i];
            ld[i] = (p2 > 0.0) ? std::log(p2) : kLogFloor;
        }
    }

    // Real cepstrum c[q] = real(IDFT(logspec)).
    Value C = signal::ifft(logspec, static_cast<int>(NFFT), 1, &arena);
    const bool cIsComplex = (C.type() == ValueType::COMPLEX);
    const Complex *cc = cIsComplex ? C.complexData() : nullptr;
    const double  *cr = cIsComplex ? nullptr        : C.doubleData();

    // Quefrency search band. A period of p samples gives f0 = fs/p, so the
    // pitch range [minF, maxF] maps to p in [fs/maxF, fs/minF]. With the
    // 1-based quefrency convention (period = array index + 1) the array
    // index range is [round(fs/maxF)-1, round(fs/minF)-1].
    Value out = Value::matrix(numFrames, 1, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();

    long qLo = 0, qHi = -1;
    if (minF > 0.0 && maxF > 0.0) {
        qLo = static_cast<long>(std::lround(fs / maxF)) - 1;
        qHi = static_cast<long>(std::lround(fs / minF)) - 1;
    }
    if (qLo < 0) qLo = 0;
    if (qHi > static_cast<long>(NFFT) - 1) qHi = static_cast<long>(NFFT) - 1;
    const bool rangeValid = (qLo <= qHi);

    for (std::size_t f = 0; f < numFrames; ++f) {
        if (!rangeValid) { od[f] = 0.0; continue; }
        const std::size_t colOff = f * NFFT;
        long   qBest = qLo;
        double cBest = -std::numeric_limits<double>::infinity();
        for (long q = qLo; q <= qHi; ++q) {
            const std::size_t idx = colOff + static_cast<std::size_t>(q);
            const double cq = cIsComplex ? cc[idx].real() : cr[idx];
            if (cq > cBest) { cBest = cq; qBest = q; }
        }
        // 1-based quefrency convention: period = qBest + 1 samples.
        od[f] = fs / static_cast<double>(qBest + 1);
    }
    return out;
}

// ──────────────────────────────────────────────────────────────────────
// pitchPEF — Pitch-Estimation-Filter fundamental-frequency estimation.
// Public reference:
//   S. Gonzalez and M. Brookes, "A Pitch Estimation Filter robust to
//   high levels of noise (PEFAC)", Proc. EUSIPCO 2011, pp. 451-455.
// This is the "PEF" variant of the published method — PEFAC without the
// amplitude-compression stage (the paper's Fig. 6 "algorithm without the
// amplitude compression stage").
// Idea: on a natural-log frequency axis q = ln f, the harmonics of a
// tone at f0 sit at q = ln f0 + ln k (k = 1..K) — a comb pattern whose
// spacing is independent of f0. Correlating the log-frequency power
// spectrum with a fixed comb filter h(q) whose teeth are at q = ln k
// therefore sums the harmonic energy and peaks at q = ln f0. The comb
// is built from the paper's Eq. 4:
//     g(q) = ln(γ − cos(2π·e^q)),   h(q) = β − g(q)
// with β chosen so Σh = 0, which makes h reject white / smoothly-varying
// noise. γ controls tooth width.
// Per analysis frame: window (periodic Hamming) → zero-pad to NFFT and
// FFT → one-sided power spectrum P → for each candidate f0 on a uniform
// log-frequency grid, score(f0) = Σ_j h_j · P_interp(f0·e^{q'_j}); the
// candidate with the largest score is the frame's f0, clipped to
// [minF, maxF].
// Clean-room reimplementation. This is
// a faithful implementation of the paper's filter (Eq. 4); MATLAB's PEF
// uses a different comb formula, so this is not bit-matched to MATLAB.
// ──────────────────────────────────────────────────────────────────────
Value pitchPEF(const Value &x, double fs, double minF, double maxF,
               std::pmr::memory_resource *mr)
{
    // PEF algorithm constants (paper §2.1, §3, §2).
    constexpr double kGamma = 1.5;            // comb tooth-width parameter γ
    constexpr int    kK     = 10;             // number of comb teeth K
    const double     kDelta = std::log(1.0058); // log-grid step Δ (0.58 %)

    // ── 1. Framing (numkit default pitch framing — one f0 per frame). ──
    const size_t N = x.numel();
    const FrameSpec fs_spec = frameSpec(N, fs, 0.052, 0.042);
    const size_t winLen   = fs_spec.winLen;
    const size_t hop      = fs_spec.hop;
    const size_t numFrames = fs_spec.numFrames;

    if (numFrames == 0 || winLen == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // ── 2. Zero-pad length: NFFT = nextPow2(4 · winLen). ───────────────
    const size_t NFFT = nextPow2(4 * winLen);
    const size_t half = NFFT / 2;            // P spans bins 0 .. NFFT/2

    ScratchArena arena(mr);

    // Periodic Hamming window over the analysis window length.
    ScratchVec<double> win(winLen, &arena);
    hammingPeriodic(win.data(), winLen);

    // Windowed-and-zero-padded frame matrix (NFFT × numFrames), column-
    // major: frame f occupies column f. signal::fft along dim = 1 then
    // transforms every frame in one call.
    Value frames = Value::matrix(NFFT, numFrames, ValueType::DOUBLE, &arena);
    double *fd = frames.doubleDataMut();
    std::fill(fd, fd + NFFT * numFrames, 0.0);   // zero-pad region
    for (size_t f = 0; f < numFrames; ++f) {
        const size_t base   = f * NFFT;       // column start
        const size_t offset = f * hop;        // frame start sample in x
        for (size_t i = 0; i < winLen; ++i)
            fd[base + i] = x.elemAsDouble(offset + i) * win[i];
    }

    // ── 2. (cont.) DFT and one-sided power spectrum P[k] = |Y[k]|². ────
    Value spec = signal::fft(frames, static_cast<int>(NFFT), 1, &arena);

    const size_t pbins = half + 1;
    ScratchVec<double> P(pbins * numFrames, &arena);
    if (spec.type() == ValueType::COMPLEX) {
        const Complex *yd = spec.complexData();
        for (size_t f = 0; f < numFrames; ++f) {
            const size_t ybase = f * NFFT;
            const size_t pbase = f * pbins;
            for (size_t k = 0; k <= half; ++k) {
                const Complex c = yd[ybase + k];
                P[pbase + k] = c.real() * c.real() + c.imag() * c.imag();
            }
        }
    } else {
        const double *yd = spec.doubleData();
        for (size_t f = 0; f < numFrames; ++f) {
            const size_t ybase = f * NFFT;
            const size_t pbase = f * pbins;
            for (size_t k = 0; k <= half; ++k) {
                const double r = yd[ybase + k];
                P[pbase + k] = r * r;
            }
        }
    }

    // Linear frequency of bin k: f_k = k · fs / NFFT  ⇒  bin = f / df.
    const double df = fs / static_cast<double>(NFFT);

    // ── Comb filter h_j over its log-frequency support (paper Eq. 4):
    //    j ∈ [ceil(ln(0.5)/Δ), floor(ln(K+0.5)/Δ)], q'_j = j·Δ. ─────────
    const long jLo = static_cast<long>(std::ceil (std::log(0.5)      / kDelta));
    const long jHi = static_cast<long>(std::floor(std::log(kK + 0.5) / kDelta));
    const size_t taps = (jHi >= jLo)
                        ? static_cast<size_t>(jHi - jLo + 1)
                        : 0;

    if (taps == 0) {
        Value out = Value::matrix(numFrames, 1, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        const double mid = std::min(std::max(0.5 * (minF + maxF), minF), maxF);
        for (size_t f = 0; f < numFrames; ++f)
            od[f] = mid;
        return out;
    }

    // q'_j and h_j = β − g_j, g_j = ln(γ − cos(2π·e^{q'_j})), β = mean(g).
    ScratchVec<double> rTap(taps, &arena);    // e^{q'_j}  (tap freq ratio)
    ScratchVec<double> gTap(taps, &arena);    // g_j
    double gSum = 0.0;
    for (size_t t = 0; t < taps; ++t) {
        const double q = static_cast<double>(jLo + static_cast<long>(t)) * kDelta;
        const double r = std::exp(q);
        const double g = std::log(kGamma - std::cos(2.0 * M_PI * r));
        rTap[t] = r;
        gTap[t] = g;
        gSum   += g;
    }
    const double beta = gSum / static_cast<double>(taps);
    ScratchVec<double> hTap(taps, &arena);    // h_j = β − g_j
    for (size_t t = 0; t < taps; ++t)
        hTap[t] = beta - gTap[t];

    // ── Candidate log-frequencies q0 = m·Δ, q0 ∈ [ln(minF), ln(maxF)]. ─
    const double q0Lo = std::log(minF);
    const double q0Hi = std::log(maxF);
    const long   mLo  = static_cast<long>(std::ceil (q0Lo / kDelta));
    const long   mHi  = static_cast<long>(std::floor(q0Hi / kDelta));

    Value out = Value::matrix(numFrames, 1, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();

    // ── Score every candidate against every frame's spectrum. ─────────
    for (size_t f = 0; f < numFrames; ++f) {
        const double *Pf = &P[f * pbins];     // this frame's power spectrum

        double bestScore = -std::numeric_limits<double>::infinity();
        double bestF0    = 0.5 * (minF + maxF);

        for (long m = mLo; m <= mHi; ++m) {
            const double f0 = std::exp(static_cast<double>(m) * kDelta);

            // score(f0) = Σ_j h_j · P_interp(f0 · e^{q'_j}).
            double score = 0.0;
            for (size_t t = 0; t < taps; ++t) {
                const double ftap = f0 * rTap[t];   // linear freq of tap j

                double pVal = 0.0;
                if (ftap >= 0.0 && ftap <= 0.5 * fs) {
                    const double pos = ftap / df;   // fractional bin index
                    const long   k0  = static_cast<long>(std::floor(pos));
                    if (k0 >= 0 && static_cast<size_t>(k0) < half) {
                        const double frac = pos - static_cast<double>(k0);
                        pVal = Pf[k0] * (1.0 - frac) + Pf[k0 + 1] * frac;
                    } else if (k0 >= 0 &&
                               static_cast<size_t>(k0) == half) {
                        pVal = Pf[half];
                    }
                }
                score += hTap[t] * pVal;
            }

            if (score > bestScore) {
                bestScore = score;
                bestF0    = f0;
            }
        }

        od[f] = std::min(std::max(bestF0, minF), maxF);
    }

    return out;
}

// ── pitch LHS method ──────────────────────────────────────────────────
// pitchLHS — fundamental-frequency estimation by log harmonic summation.
// Principle (Hermes, "Measurement of pitch by subharmonic summation",
// JASA 83(1):257-264, 1988): a voiced signal whose fundamental is f has
// spectral energy concentrated at the integer harmonics f, 2f, 3f, ...
// Therefore, if the log-magnitude spectrum is sampled at integer multiples
// of a candidate fundamental and those samples are summed, the resulting
// score is maximised at the true f0. Hermes used subharmonic summation
// (compressing toward a virtual-pitch percept); here we use the direct
// harmonic-summation form ("log harmonic summation", LHS), which sums the
// log-magnitude spectrum over the first H harmonics of each candidate.
// Clean-room reimplementation.
// Per analysis frame:
//   1. Window the frame with a periodic Hamming window.
//   2. Zero-pad to NFFT = round(fs) and take the DFT; with NFFT = fs each
//      bin spans exactly 1 Hz, so a bin index equals a frequency in Hz and
//      harmonic indexing j*m is a plain integer multiply.
//   3. Form the log-magnitude spectrum S[k] = log(|Y[k]|), flooring a zero
//      magnitude at log(smallest positive normal double).
//   4. domain[j] = sum_{m=1..H} S[j*m] is the harmonic-sum score for a
//      fundamental of j Hz.
//   5. f0 = argmax over the search range; clip to [minF, maxF].
// Compatibility: MATLAB's LHS reports f0 from a 1-based bin index, so a
// harmonic-sum peak at 0-based bin j is reported as f0 = j + 1 Hz.
Value pitchLHS(const Value &x, double fs, double minF, double maxF,
               std::pmr::memory_resource *mr)
{
    // --- Method constants (spec §3) -----------------------------------------
    constexpr int    kH         = 5;     // harmonics summed by LHS
    constexpr double kWinSec    = 0.052; // analysis window length (s)
    constexpr double kOverlapS  = 0.042; // analysis overlap (s)

    // --- Framing (spec §3) --------------------------------------------------
    const size_t N  = x.numel();
    const FrameSpec fs_spec = frameSpec(N, fs, kWinSec, kOverlapS);
    const size_t winLen   = fs_spec.winLen;
    const size_t hop      = fs_spec.hop;
    const size_t numFrames = fs_spec.numFrames;

    if (numFrames == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // --- DFT length and harmonic-coverage check (spec §3) -------------------
    // NFFT = round(fs) gives exactly 1-Hz bins (Δf = fs/NFFT = 1).
    const int NFFT = static_cast<int>(std::llround(fs));

    // Highest bin any candidate touches: harmonic H of the highest
    // searchable fundamental, floor(maxF). If it exceeds NFFT the search
    // range cannot be served by this spectrum.
    const long long K = static_cast<long long>(kH) *
                        static_cast<long long>(std::floor(maxF));
    if (NFFT <= 0 || K > static_cast<long long>(NFFT))
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // --- 0-based search range over harmonic-sum candidate bins (spec §3) ----
    // domain[j] scores a fundamental of j Hz; MATLAB reports f0 = j + 1,
    // so to search fundamentals in [minF, maxF] we scan
    // j ∈ [ceil(minF) − 1, floor(maxF) − 1], clamped to valid indices.
    long long jLoRaw = static_cast<long long>(std::ceil(minF)) - 1;
    long long jHiRaw = static_cast<long long>(std::floor(maxF)) - 1;
    if (jLoRaw < 0)
        jLoRaw = 0;
    // A candidate at bin j needs bin j*H present, i.e. j*H ≤ NFFT-1.
    const long long jMaxByCoverage = (NFFT - 1) / kH;
    if (jHiRaw > jMaxByCoverage)
        jHiRaw = jMaxByCoverage;
    if (jHiRaw < jLoRaw)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    const size_t jLo = static_cast<size_t>(jLoRaw);
    const size_t jHi = static_cast<size_t>(jHiRaw);

    // --- Scratch (PMR HARD RULE: ScratchArena + ScratchVec only) -----------
    ScratchArena arena(mr);

    // Periodic Hamming analysis window, computed once.
    ScratchVec<double> window(winLen, &arena);
    hammingPeriodic(window.data(), winLen);

    // Batched real input: NFFT rows × numFrames columns. Each column holds
    // one windowed, zero-padded frame; signal::fft(..., NFFT, 1, ...) then
    // transforms every column in a single call.
    Value frames = Value::matrix(static_cast<size_t>(NFFT), numFrames,
                                 ValueType::DOUBLE, &arena);
    double *col0 = frames.doubleDataMut(); // column-major writable buffer

    for (size_t f = 0; f < numFrames; ++f) {
        double *col = col0 + f * static_cast<size_t>(NFFT);
        const size_t base = f * hop;
        for (size_t n = 0; n < static_cast<size_t>(NFFT); ++n) {
            double s = 0.0;
            if (n < winLen) {
                const size_t idx = base + n;
                if (idx < N)
                    s = x.elemAsDouble(idx) * window[n];
            }
            col[n] = s;
        }
    }

    // --- Forward DFT of every frame (dim == 1 → transform each column) ------
    Value spec = signal::fft(frames, NFFT, 1, &arena);
    const Complex *Y = spec.complexData();

    // log-magnitude floor: smallest positive normal double.
    const double kMagFloor = std::log(std::numeric_limits<double>::min());

    // Bins of the log-magnitude spectrum we actually need: 0 .. K-1.
    const size_t Kbins = static_cast<size_t>(K);

    // --- Per-frame harmonic-sum estimation ---------------------------------
    Value out = Value::matrix(numFrames, 1, ValueType::DOUBLE, mr);
    double *outData = out.doubleDataMut();

    ScratchVec<double> logMag(Kbins, &arena);  // S[k] for current frame

    for (size_t f = 0; f < numFrames; ++f) {
        const Complex *Yf = Y + f * static_cast<size_t>(NFFT);

        // S[k] = log(|Y[k]|), floored at log(min normal double).
        for (size_t k = 0; k < Kbins; ++k) {
            const double mag = std::abs(Yf[k]);
            logMag[k] = (mag > 0.0) ? std::log(mag) : kMagFloor;
        }

        // domain[j] = Σ_{m=1..H} S[j·m]; pick the maximising bin.
        size_t bestJ   = jLo;
        double bestSum = -std::numeric_limits<double>::infinity();
        for (size_t j = jLo; j <= jHi; ++j) {
            double sum = 0.0;
            for (int m = 1; m <= kH; ++m)
                sum += logMag[j * static_cast<size_t>(m)];
            if (sum > bestSum) {
                bestSum = sum;
                bestJ   = j;
            }
        }

        // Compatibility convention: a 0-based peak at bin j → f0 = j + 1 Hz.
        double f0 = static_cast<double>(bestJ) + 1.0;

        // Clip to the requested search range.
        if (f0 < minF)
            f0 = minF;
        else if (f0 > maxF)
            f0 = maxF;

        outData[f] = f0;
    }

    return out;
}

// ─────────────────────────────────────────────────────────────────────
// pitchSRH — Summation-of-Residual-Harmonics fundamental-frequency
//            estimation, one f0 per analysis frame.
// Reference:
//   T. Drugman and A. Alwan, "Joint Robust Voicing Detection and Pitch
//   Estimation Based on Residual Harmonics", Interspeech 2011,
//   pp. 1973-1976.
// Algorithm (paper-faithful, NOT bit-matched to MATLAB pitch(...,'SRH')):
//   The signal is cut into overlapping frames. Each frame is windowed
//   with a periodic Hann window, then an order-P linear-prediction model
//   is fitted and the frame is inverse-filtered to obtain the LPC
//   residual r[n] — this whitens the spectral envelope and leaves the
//   harmonic structure of the voiced excitation. The residual amplitude
//   spectrum E[k] = |DFT(r)| is computed with an NFFT chosen so each bin
//   spans exactly 1 Hz, so E is indexed directly in Hz. For every integer
//   candidate fundamental f the SRH criterion (paper Eq. 1)
//       SRH(f) = E[f] + Σ_{k=2..Nharm} ( E[k·f] − E[round((k−½)·f)] )
//   sums the harmonic energy while the half-harmonic subtraction
//   suppresses spurious maxima at f0/2 and at even harmonics. The f0 of
//   a frame is the candidate maximising SRH. A two-step range refinement
//   first estimates a global mean f0 over all frames, then re-runs the
//   search per frame with the candidate range narrowed around that mean.
// Clean-room reimplementation. This is
// a faithful implementation of the published SRH method; it does not
// replicate MATLAB's SRH pipeline and is not bit-matched to MATLAB.
// ─────────────────────────────────────────────────────────────────────
Value pitchSRH(const Value &x, double fs, double minF, double maxF,
               std::pmr::memory_resource *mr)
{
    // ── Method parameters (paper §3.2) ──────────────────────────────
    constexpr int    kLpcOrder = 12;   // P  — LPC order
    constexpr int    kNharm    = 5;    // Nharm — harmonics summed
    const     long   nfft      = std::lround(fs);   // 1-Hz bins

    // ── Framing — standard pitch framing (52 ms win, 42 ms overlap) ─
    const size_t N = x.numel();
    const FrameSpec fr = frameSpec(N, fs, 0.052, 0.042);
    const size_t winLen    = fr.winLen;
    const size_t hop       = fr.hop;
    const size_t numFrames = fr.numFrames;

    // No frames → MATLAB-style empty result.
    if (numFrames == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // Candidate-range / NFFT feasibility (§3): the highest spectral bin
    // ever touched is Nharm · floor(maxF). If that exceeds NFFT the
    // 1-Hz-bin spectrum cannot serve the requested range.
    const long maxBin = static_cast<long>(kNharm)
                      * static_cast<long>(std::floor(maxF));
    if (nfft <= 0 || maxBin >= nfft || maxF <= 0.0 || minF <= 0.0
        || maxF < minF)
        return Value::matrix(numFrames, 1, ValueType::DOUBLE, mr);

    // ── Per-call scratch arena (PMR HARD RULE) ──────────────────────
    ScratchArena arena(mr);

    // Periodic Hann window: w[n] = 0.5·(1 − cos(2π·n / N)), n = 0..N−1.
    ScratchVec<double> hann(winLen, &arena);
    for (size_t n = 0; n < winLen; ++n)
        hann[n] = 0.5 * (1.0 - std::cos(2.0 * M_PI
                         * static_cast<double>(n)
                         / static_cast<double>(winLen)));

    // Residual amplitude spectrum E for every frame, packed row-major:
    // E[frame * nfft + k]. Each E[k] is the magnitude of DFT bin k,
    // indexed directly in Hz because every bin spans 1 Hz.
    const size_t specLen = static_cast<size_t>(nfft);
    ScratchVec<double> E(numFrames * specLen, &arena);

    // Per-frame buffers reused across the loop.
    ScratchVec<double> windowed(winLen, &arena);

    for (size_t f = 0; f < numFrames; ++f)
    {
        const size_t base = f * hop;

        // Window the frame (zero-pad implicitly if the frame runs past
        // the end of the signal — defensive, framing usually prevents).
        for (size_t n = 0; n < winLen; ++n)
        {
            const size_t idx = base + n;
            const double s   = (idx < N) ? x.elemAsDouble(idx) : 0.0;
            windowed[n]      = s * hann[n];
        }

        // LPC residual: fit an order-P model, then inverse-filter the
        // windowed frame with the LPC polynomial a = [1, a₁, …, a_P]:
        //   r[n] = filter(a, 1, windowedFrame).
        Value winFrame = Value::matrix(winLen, 1, ValueType::DOUBLE,
                                       &arena);
        std::copy(windowed.data(), windowed.data() + winLen,
                  winFrame.doubleDataMut());
        auto [lpcA, lpcGain] = signal::lpc(winFrame, kLpcOrder, &arena);
        (void) lpcGain;  // gain not used by SRH

        Value one     = Value::scalar(1.0, &arena);
        Value residual = signal::filter(lpcA, one, winFrame, &arena);

        // Amplitude spectrum E[k] = |DFT_NFFT(residual)|. fft zero-pads
        // / truncates the residual to NFFT samples; result is COMPLEX.
        Value spec = signal::fft(residual, static_cast<int>(nfft), 0,
                                 &arena);

        double *Erow = E.data() + f * specLen;
        if (spec.isComplex())
        {
            const Complex *sd = spec.complexData();
            for (size_t k = 0; k < specLen; ++k)
                Erow[k] = std::abs(sd[k]);
        }
        else
        {
            // fft downgrades to DOUBLE when the result is real within
            // tolerance — magnitude is then |real value|.
            const double *sd = spec.doubleData();
            for (size_t k = 0; k < specLen; ++k)
                Erow[k] = std::abs(sd[k]);
        }
    }

    // SRH score for candidate fundamental `cand` (Hz) on frame `f`,
    // paper Eq. 1:
    //   SRH(f) = E[f] + Σ_{k=2..Nharm} ( E[k·f] − E[round((k−½)·f)] ).
    auto srhScore = [&](size_t f, long cand) -> double
    {
        const double *Erow = E.data() + f * specLen;
        double score = Erow[cand];
        for (int k = 2; k <= kNharm; ++k)
        {
            const long hi  = static_cast<long>(k) * cand;
            const long mid = std::lround((static_cast<double>(k) - 0.5)
                                         * static_cast<double>(cand));
            const double eHi  = (hi  >= 0 && hi  < nfft)
                                    ? Erow[hi]  : 0.0;
            const double eMid = (mid >= 0 && mid < nfft)
                                    ? Erow[mid] : 0.0;
            score += eHi - eMid;
        }
        return score;
    };

    // Find the candidate in [lo, hi] (Hz, inclusive) maximising SRH for
    // frame f. Returns the integer Hz of the best candidate.
    auto bestCandidate = [&](size_t f, long lo, long hi) -> long
    {
        long   bestF     = lo;
        double bestScore = -std::numeric_limits<double>::infinity();
        for (long cand = lo; cand <= hi; ++cand)
        {
            const double sc = srhScore(f, cand);
            if (sc > bestScore)
            {
                bestScore = sc;
                bestF     = cand;
            }
        }
        return bestF;
    };

    // ── Pass 1 — full candidate range [ceil(minF), floor(maxF)] ──────
    const long loFull = static_cast<long>(std::ceil(minF));
    const long hiFull = static_cast<long>(std::floor(maxF));

    ScratchVec<double> f0(numFrames, &arena);
    double sumPass1 = 0.0;
    for (size_t f = 0; f < numFrames; ++f)
    {
        f0[f]     = static_cast<double>(bestCandidate(f, loFull, hiFull));
        sumPass1 += f0[f];
    }
    const double F0mean = sumPass1 / static_cast<double>(numFrames);

    // ── Pass 2 — narrow the range around F0mean, re-run per frame ────
    if (std::isfinite(F0mean) && F0mean > 0.0)
    {
        const long loRefined = std::max<long>(
            loFull, std::lround(0.5 * F0mean));
        const long hiRefined = std::min<long>(
            hiFull, std::lround(2.0 * F0mean));
        if (loRefined <= hiRefined)
        {
            for (size_t f = 0; f < numFrames; ++f)
                f0[f] = static_cast<double>(
                    bestCandidate(f, loRefined, hiRefined));
        }
    }

    // ── Clip each f0 to [minF, maxF] and emit the result ────────────
    Value result = Value::matrix(numFrames, 1, ValueType::DOUBLE, mr);
    double *out   = result.doubleDataMut();
    for (size_t f = 0; f < numFrames; ++f)
    {
        double v = f0[f];
        if (v < minF) v = minF;
        if (v > maxF) v = maxF;
        out[f] = v;
    }
    return result;
}

// ── pitch ─────────────────────────────────────────────────────────────
Value pitch(const Value &x, double fs, double minF, double maxF, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    const FrameSpec sp = frameSpec(N, fs, 0.052, 0.042);
    Value out = Value::matrix(sp.numFrames, sp.numFrames == 0 ? 0 : 1,
                              ValueType::DOUBLE, mr);
    if (sp.numFrames == 0) return out;

    const size_t minLag = static_cast<size_t>(std::floor(fs / maxF));
    const size_t maxLag = std::min<size_t>(sp.winLen - 1,
                                           static_cast<size_t>(std::ceil(fs / minF)));
    if (maxLag <= minLag) return out;

    ScratchArena scratch(mr);
    ScratchVec<double> win(sp.winLen, &scratch);
    hammingPeriodic(win.data(), sp.winLen);
    ScratchVec<double> frame(sp.winLen, &scratch);
    ScratchVec<double> R(maxLag + 1, &scratch);
    ScratchVec<double> P(maxLag + 1, &scratch);

    double *od = out.doubleDataMut();
    for (size_t f = 0; f < sp.numFrames; ++f) {
        const size_t start = f * sp.hop;
        for (size_t i = 0; i < sp.winLen; ++i)
            frame[i] = x.elemAsDouble(start + i) * win[i];
        autocorr(frame.data(), sp.winLen, maxLag + 1, R.data());
        partialPower(frame.data(), sp.winLen, maxLag + 1, P.data());

        const double total = R[0];
        // NCF: γ[k] = R[k] / sqrt(total * P[k]).
        double bestVal = -1.0;
        size_t bestLag = minLag;
        for (size_t k = minLag; k <= maxLag; ++k) {
            const double denom = std::sqrt(total * P[k]) + 1e-300;
            const double g = R[k] / denom;
            if (g > bestVal) { bestVal = g; bestLag = k; }
        }
        // f0 = fs / lag (with parabolic interpolation for sub-sample lag).
        double lag = static_cast<double>(bestLag);
        if (bestLag > minLag && bestLag < maxLag) {
            const double a = R[bestLag - 1] / (std::sqrt(total * P[bestLag - 1]) + 1e-300);
            const double b = bestVal;
            const double c = R[bestLag + 1] / (std::sqrt(total * P[bestLag + 1]) + 1e-300);
            const double denom = 2.0 * (2.0 * b - c - a);
            if (std::abs(denom) > 1e-12)
                lag = static_cast<double>(bestLag) - (a - c) / denom;
        }
        od[f] = (lag > 0.0) ? fs / lag : 0.0;
    }
    return out;
}

// ── harmonicRatio ─────────────────────────────────────────────────────
// Harmonic ratio (MPEG-7 / Peeters 2004), normalized-autocorrelation
// form. Per frame:
//   1. Auto low-edge: first sign change of R[k] for k >= 1.
//   2. Search peak γ in [lowEdge, highEdge=winLen-1].
//   3. Parabolic interpolation around peak (Smith's quadratic peak).
//   4. Clip to [0, 1].
Value harmonicRatio(const Value &x, double fs, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    const FrameSpec sp = frameSpec(N, fs, 0.03, 0.02);
    Value out = Value::matrix(sp.numFrames, sp.numFrames == 0 ? 0 : 1,
                              ValueType::DOUBLE, mr);
    if (sp.numFrames == 0) return out;

    const size_t maxLag = sp.winLen - 1;  // highEdge = winLen-1 per MATLAB

    ScratchArena scratch(mr);
    ScratchVec<double> win(sp.winLen, &scratch);
    hammingPeriodic(win.data(), sp.winLen);
    ScratchVec<double> frame(sp.winLen, &scratch);
    const size_t mLag = std::max<size_t>(maxLag + 1, 2);
    ScratchVec<double> R(mLag, &scratch);
    ScratchVec<double> P(mLag, &scratch);
    ScratchVec<double> gamma(mLag, &scratch);

    double *od = out.doubleDataMut();
    for (size_t f = 0; f < sp.numFrames; ++f) {
        const size_t start = f * sp.hop;
        for (size_t i = 0; i < sp.winLen; ++i)
            frame[i] = x.elemAsDouble(start + i) * win[i];
        autocorr(frame.data(), sp.winLen, mLag, R.data());
        partialPower(frame.data(), sp.winLen, mLag, P.data());

        const double total = R[0];
        // Build full normalized correlation γ[k] = R[k] / sqrt(total*P[k]).
        for (size_t k = 0; k < mLag; ++k) {
            const double denom = std::sqrt(total * P[k])
                                  + std::sqrt(std::numeric_limits<double>::epsilon());
            gamma[k] = R[k] / denom;
        }
        // Auto low-edge: first sign change of R[k] for k >= 1.
        // (i.e. first k where sign(R[k]) != sign(R[k-1])).
        size_t lowEdge = maxLag;  // sentinel = no sign change → use maxLag
        const double r0 = R[0];
        int prev_sign = (r0 > 0.0) ? 1 : (r0 < 0.0 ? -1 : 0);
        for (size_t k = 1; k < mLag; ++k) {
            const int s = (R[k] > 0.0) ? 1 : (R[k] < 0.0 ? -1 : 0);
            if (s != prev_sign) { lowEdge = k + 1; break; }
        }
        if (lowEdge < 1) lowEdge = 1;
        // Zero the normalized correlation below the low edge (0..lowEdge-1).
        for (size_t k = 0; k < lowEdge && k < mLag; ++k) gamma[k] = 0.0;

        // Find peak.
        size_t peakIdx = lowEdge;
        double peakVal = 0.0;
        for (size_t k = lowEdge; k < mLag; ++k) {
            if (gamma[k] > peakVal) { peakVal = gamma[k]; peakIdx = k; }
        }
        // Parabolic interpolation (Smith's quadratic-peak formula):
        //   refined = b - 0.25 * (a - c) * s
        //   where s = (c - a) / (2*(2*b - c - a))
        double refined = peakVal;
        if (peakIdx > 0 && peakIdx + 1 < mLag) {
            const double a = gamma[peakIdx - 1];
            const double b = gamma[peakIdx];
            const double c = gamma[peakIdx + 1];
            const double denomS = 2.0 * (2.0 * b - c - a);
            if (std::abs(denomS) > 1e-12) {
                const double s = (c - a) / denomS;
                refined = b - 0.25 * (a - c) * s;
            }
        }
        if (refined < 0.0) refined = 0.0;
        if (refined > 1.0) refined = 1.0;
        od[f] = refined;
    }
    return out;
}

} // namespace numkit::audio
