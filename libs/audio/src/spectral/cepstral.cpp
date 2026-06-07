// libs/audio/src/spectral/cepstral.cpp
// Audio Cycle D + G: cepstral coefficients (cepstralCoefficients / mfcc / gtcc).
// cepstralCoefficients pipeline:
//   S (L bands × M frames) → log10 rectification → DCT-II (unitary)
//                          → keep first NumCoeffs → permute to M × NumCoeffs
// DCT-II unitary matrix (Ahmed, Natarajan & Rao, IEEE TC, 1974):
//   N = NumCoeffs, K = NumFilters
//   matrix(1, k)   = sqrt(1/K)  (DC row)
//   matrix(n, k)   = sqrt(2/K) * cos(π·(n-1)·(k-0.5)/K)  for n = 2..N
// mfcc pipeline (standard MFCC; Davis & Mermelstein, IEEE TASSP, 1980):
//   1. y = buffer(x, winLen=round(0.03*fs), hop=winLen-round(0.02*fs))
//   2. logE = log(sum(y.^2))   ← natural log of UNWINDOWED frame energy
//   3. winCast = hamming(winLen, 'periodic')
//   4. Z = abs(fft(y .* win, fftLen))   ← MAGNITUDE, full-length FFT
//   5. Slaney-style mel filterbank with 'Bandwidth' normalization
//   6. melMag = filterBank.' * Z   (numBands × numFrames)
//   7. coeffs = cepstralCoefficients(melMag, NumCoeffs=13, Rectification='log')
//   8. out = [logE.', coeffs]   (numFrames × (NumCoeffs+1))
// Slaney mel band edges (Slaney, "Auditory Toolbox", Apple TR #45, 1998):
//   factor = 133.33333333333333
//   bE[1..13] = factor + (factor/2)*(i-1)        (linear up to 866.66 Hz)
//   bE[14..42] = bE[i-1] * 1.0711703             (log-spaced, ~27 bands/octave)
// PMR HARD RULE.

#include <numkit/audio/spectral/cepstral.hpp>
#include <numkit/audio/spectral/melspec_delta.hpp>

// Compute-only TU: Value substrate + Error, no engine. The
// cepstralCoefficients / mfcc / gtcc builtins (CallContext wrappers) live
// in spectral/cepstral_reg.cpp.
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>

#include "fft_one_sided.hpp"

#include <algorithm>
#include <cmath>
#include <complex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::audio {

namespace {

// Build the N × K unitary DCT-II matrix (Ahmed, Natarajan & Rao 1974).
void buildDctMatrix(double *D, size_t N, size_t K)
{
    if (K == 0 || N == 0) return;
    const double A = std::sqrt(1.0 / static_cast<double>(K));
    const double B = std::sqrt(2.0 / static_cast<double>(K));
    // First row = A everywhere.
    for (size_t k = 0; k < K; ++k) D[0 + k * N] = A;
    // Remaining rows: B * cos(π·(n-1)·(k-0.5)/K)
    for (size_t k = 0; k < K; ++k) {
        const double kc = static_cast<double>(k) + 0.5;
        for (size_t n = 1; n < N; ++n) {
            D[n + k * N] = B * std::cos(M_PI * static_cast<double>(n) * kc
                                          / static_cast<double>(K));
        }
    }
}

// Periodic Hamming window: w[n] = 0.54 - 0.46 * cos(2π·n / N).
void hammingPeriodic(double *w, size_t N)
{
    for (size_t n = 0; n < N; ++n)
        w[n] = 0.54 - 0.46 * std::cos(2.0 * M_PI * static_cast<double>(n)
                                        / static_cast<double>(N));
}

// Slaney mel band edges (42 entries; Slaney, "Auditory Toolbox", 1998).
// factor=133.33...; first 13 linear at factor/2 step; rest log-spaced.
void slaneyBandEdges(double *edges, size_t numEdges = 42)
{
    if (numEdges < 1) return;
    const double factor = 133.33333333333333;
    const size_t linEnd = std::min<size_t>(13, numEdges);
    for (size_t i = 0; i < linEnd; ++i)
        edges[i] = factor + (factor * 0.5) * static_cast<double>(i);
    for (size_t i = linEnd; i < numEdges; ++i)
        edges[i] = edges[i - 1] * 1.0711703;
}

// ---------------------------------------------------------------------------
// designMelFilterBankSlaney — Slaney-style triangular mel filterbank.
// Each band k places one triangular filter spanning three consecutive mel
// band edges edges[k] < edges[k+1] < edges[k+2]: the filter rises linearly
// from 0 at edges[k] to 1 at edges[k+1], then falls linearly back to 0 at
// edges[k+2] (overlapping-triangle filterbank of Davis & Mermelstein 1980).
// The filter is sampled on the one-sided FFT frequency grid, bin j having
// frequency f_j = j * fs / NFFT. Weights are bandwidth-normalised: every
// weight of band k is divided by half its base width, (edges[k+2]-edges[k])/2,
// giving each triangle unit area (Slaney, Auditory Toolbox v2, 1998 —
// Normalization='Bandwidth'). Output FB is column-major, length H*(numEdges-2).
// Clean-room reimplementation.
// References:
//   S. B. Davis and P. Mermelstein, "Comparison of Parametric Representations
//   for Monosyllabic Word Recognition in Continuously Spoken Sentences",
//   IEEE Trans. ASSP 28(4):357-366, 1980.
//   M. Slaney, "Auditory Toolbox, Version 2", Interval Research Corp.
//   Technical Report #1998-010, 1998.
// ---------------------------------------------------------------------------
void designMelFilterBankSlaney(double *FB, double fs, std::size_t NFFT,
                               const double *edges, std::size_t numEdges,
                               std::size_t H /* = NFFT/2 + 1 */)
{
    // Early return when there is nothing to construct.
    if (numEdges < 3 || H == 0 || NFFT == 0) {
        return;
    }

    const std::size_t numBands = numEdges - 2;

    // Zero the whole output buffer first, then fill it.
    for (std::size_t i = 0; i < H * numBands; ++i) {
        FB[i] = 0.0;
    }

    // Hz per FFT bin on the one-sided frequency grid.
    const double binToHz = fs / static_cast<double>(NFFT);

    for (std::size_t k = 0; k < numBands; ++k) {
        const double left = edges[k];
        const double centre = edges[k + 1];
        const double right = edges[k + 2];

        const double riseWidth = centre - left;   // base of the rising side
        const double fallWidth = right - centre;  // base of the falling side
        const double baseWidth = right - left;    // full triangle base

        // Degenerate guards: a zero/negative side width or non-positive base
        // width leaves this band's column at zero.
        if (riseWidth <= 0.0 || fallWidth <= 0.0 || baseWidth <= 0.0) {
            continue;
        }

        // Bandwidth normalisation: a unit-height triangle of base baseWidth
        // has area baseWidth/2; dividing by that yields unit area.
        const double norm = 2.0 / baseWidth;

        double *column = FB + k * H;
        for (std::size_t j = 0; j < H; ++j) {
            const double f = static_cast<double>(j) * binToHz;
            const double rise = (f - left) / riseWidth;
            const double fall = (right - f) / fallWidth;
            const double w = std::max(0.0, std::min(rise, fall));
            column[j] = w * norm;
        }
    }
}

// ERB scale factor (Glasberg & Moore 1990): log(10)*1000/(24.673*4.368)
// — matches libs/audio/src/scale/freq_scales.cpp erbScale().
inline double erbScale()
{
    return std::log(10.0) * 1000.0 / (24.673 * 4.368);
}
inline double hz2erbVal(double hz) { return erbScale() * std::log10(1.0 + 0.004368 * hz); }
inline double erb2hzVal(double e)  { return (std::pow(10.0, e / erbScale()) - 1.0) / 0.004368; }

// Compute Patterson-Holdsworth gammatone filterbank frequency response
// magnitude (one-sided H × NumBands). Filter coefficients per Slaney,
// "An Efficient Implementation of the Patterson-Holdsworth Auditory
// Filter Bank", Apple Tech. Report #35, 1993.
// For each band i with center frequency Fc[i] (Hz):
//   ERB[i] = Fc[i]/9.26449 + 24.7
//   B      = 1.019 * 2π * ERB[i]
//   T      = 1/fs
//   B1     = -2*cos(2π Fc T)/exp(B T)
//   B2     = exp(-2 B T)
//   A0     = T;  A2 = 0
//   A11..A14 = -(2T cos(2π Fc T)/exp(B T) ± 2 sqrt(3 ± 2^(3/2)) T sin(2π Fc T)/exp(B T))/2
//   gain   = (complex polynomial — see Slaney 1993, eq. 4.6)
// Each band is a CASCADE of 4 biquads:
//   sec1: [A0/gain, A11/gain, A2/gain]/[B0, B1, B2]   (gain applied here)
//   sec2: [A0,      A12,      A2]    /[B0, B1, B2]
//   sec3: [A0,      A13,      A2]    /[B0, B1, B2]
//   sec4: [A0,      A14,      A2]    /[B0, B1, B2]
// freqz(...,'whole') evaluates H at ω = 2π k/N for k=0..N-1.
// We compute one-sided (k=0..N/2) since H is conjugate-symmetric for
// real coefficients.
void computeGammatoneFreqRespOneSided(double fs, const double *Fc, size_t numBands,
                                       size_t NFFT, size_t H, double *bank)
{
    const double T = 1.0 / fs;
    const double EarQ = 9.26449;
    const double minBW = 24.7;
    const double s32a = std::sqrt(3.0 + std::pow(2.0, 1.5));  // sqrt(3 + 2√2)
    const double s32b = std::sqrt(3.0 - std::pow(2.0, 1.5));  // sqrt(3 - 2√2)

    using cd = std::complex<double>;
    for (size_t i = 0; i < numBands; ++i) {
        const double cf = Fc[i];
        const double ERB = cf / EarQ + minBW;
        const double B = 1.019 * 2.0 * M_PI * ERB;

        const double cosArg = 2.0 * cf * M_PI * T;
        const double cosT = std::cos(cosArg);
        const double sinT = std::sin(cosArg);
        const double expBT = std::exp(B * T);
        const double expM2BT = std::exp(-2.0 * B * T);

        const double A0 = T;
        const double A11 = -(2.0 * T * cosT / expBT + 2.0 * s32a * T * sinT / expBT) * 0.5;
        const double A12 = -(2.0 * T * cosT / expBT - 2.0 * s32a * T * sinT / expBT) * 0.5;
        const double A13 = -(2.0 * T * cosT / expBT + 2.0 * s32b * T * sinT / expBT) * 0.5;
        const double A14 = -(2.0 * T * cosT / expBT - 2.0 * s32b * T * sinT / expBT) * 0.5;
        const double A2 = 0.0;
        const double B0 = 1.0;
        const double B1 = -2.0 * cosT / expBT;
        const double B2 = expM2BT;

        // Compute gain (Slaney equation 4.6, complex):
        //   exp4j = exp(4·j·π·cf·T) = exp(2j·cosArg)
        //   exp_BT_2j = exp(-B·T + 2j·π·cf·T) = exp(-B·T) · exp(j·cosArg)
        //   common term: -2·exp4j·T + 2·exp_BT_2j·T·(cos ± sX·sin)
        //   gain = abs(prod_4_terms / denom^4)
        const cd j(0.0, 1.0);
        const cd exp4j  = std::exp(2.0 * j * cosArg);
        const cd exp_2j = std::exp(j * cosArg) / expBT;  // exp(-B T + j cosArg)
        const cd term1 = -2.0 * exp4j * T + 2.0 * exp_2j * T * (cosT - s32b * sinT);
        const cd term2 = -2.0 * exp4j * T + 2.0 * exp_2j * T * (cosT + s32b * sinT);
        const cd term3 = -2.0 * exp4j * T + 2.0 * exp_2j * T * (cosT - s32a * sinT);
        const cd term4 = -2.0 * exp4j * T + 2.0 * exp_2j * T * (cosT + s32a * sinT);
        const cd denomSimple = -2.0 / std::exp(2.0 * B * T) - 2.0 * exp4j
                                + 2.0 * (1.0 + exp4j) / expBT;
        const cd denom4 = std::pow(denomSimple, 4);
        const double gain = std::abs((term1 * term2 * term3 * term4) / denom4);

        // Section coefficients: [b0, b1, b2, a0, a1, a2]
        // sec1 numerator divided by gain.
        const double b0_1 = A0 / gain, b1_1 = A11 / gain, b2_1 = A2 / gain;
        const double b0_2 = A0,        b1_2 = A12,        b2_2 = A2;
        const double b0_3 = A0,        b1_3 = A13,        b2_3 = A2;
        const double b0_4 = A0,        b1_4 = A14,        b2_4 = A2;
        // Common denom for all 4 sections: [B0, B1, B2]

        // Compute |H(ω_k)| for k = 0..H-1.
        double *bankRow = bank + i * H;  // row i, H columns (column-major over H)
        for (size_t k = 0; k < H; ++k) {
            const double w = -2.0 * M_PI * static_cast<double>(k) / static_cast<double>(NFFT);
            const cd zinv  = std::exp(j * w);
            const cd zinv2 = zinv * zinv;
            // Same denom for all 4 sections.
            const cd den = B0 + B1 * zinv + B2 * zinv2;
            const cd num1 = b0_1 + b1_1 * zinv + b2_1 * zinv2;
            const cd num2 = b0_2 + b1_2 * zinv + b2_2 * zinv2;
            const cd num3 = b0_3 + b1_3 * zinv + b2_3 * zinv2;
            const cd num4 = b0_4 + b1_4 * zinv + b2_4 * zinv2;
            const cd Htot = (num1 * num2 * num3 * num4) / (den * den * den * den);
            bankRow[k] = std::abs(Htot);
        }
    }
}

} // anon

// ── cepstralCoefficients ──────────────────────────────────────────────
// S: L × M (filterbank bands × frames). Output: M × NumCoeffs.
Value cepstralCoefficients(const Value &S, int numCoeffs, std::pmr::memory_resource *mr)
{
    if (S.dims().is3D())
        throw Error("cepstralCoefficients: input must be 2-D",
                    0, 0, "cepstralCoefficients", "",
                    "numkit:cepstralCoefficients:Not2D");
    const size_t L = S.dims().rows();
    const size_t M = S.dims().cols();
    const size_t N = static_cast<size_t>(numCoeffs);
    if (numCoeffs < 2)
        throw Error("cepstralCoefficients: NumCoeffs must be > 1",
                    0, 0, "cepstralCoefficients", "",
                    "numkit:cepstralCoefficients:BadN");

    Value out = Value::matrix(M, N, ValueType::DOUBLE, mr);
    if (L == 0 || M == 0) return out;

    ScratchArena scratch(mr);
    ScratchVec<double> logS(L * M, &scratch);
    const double *Sd = S.doubleData();
    for (size_t i = 0; i < L * M; ++i) {
        const double v = Sd[i];
        logS[i] = (v > 0.0) ? std::log10(v) : -std::numeric_limits<double>::infinity();
    }

    // DCT matrix N × L.
    ScratchVec<double> D(N * L, &scratch);
    buildDctMatrix(D.data(), N, L);

    // coeffs (N × M) = D (N × L) * logS (L × M); then transpose → M × N.
    double *od = out.doubleDataMut();
    for (size_t m = 0; m < M; ++m) {
        for (size_t n = 0; n < N; ++n) {
            double s = 0.0;
            for (size_t l = 0; l < L; ++l)
                s += D[n + l * N] * logS[l + m * L];
            // Permute (n, m) → out(m, n) in M × N column-major: idx = m + n*M.
            od[m + n * M] = s;
        }
    }
    return out;
}

// ── mfcc ──────────────────────────────────────────────────────────────
// Standard MFCC pipeline (Davis & Mermelstein 1980). Steps:
//   1. Buffer x into winLen × numFrames frames (winLen=round(0.03*fs),
//      hop=winLen-round(0.02*fs)).
//   2. Per-frame logE = log(sum(frame^2)) (natural log, UNWINDOWED).
//   3. Apply hamming(winLen,'periodic'); compute |FFT|; one-sided half.
//   4. Slaney mel filterbank with 'Bandwidth' normalization (40 bands at
//      default 42 Slaney edges).
//   5. cepstralCoefficients (log10 + DCT-II unitary, NumCoeffs=13).
//   6. Prepend logE column → numFrames × (NumCoeffs+1).
std::tuple<Value, Value, Value>
mfcc(const Value &x, double fs, int numCoeffs, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    const size_t winLen  = static_cast<size_t>(std::round(fs * 0.03));
    const size_t overlap = static_cast<size_t>(std::round(fs * 0.02));
    const size_t hop = (winLen > overlap) ? (winLen - overlap) : 1;
    const size_t fftLen = winLen;
    const size_t H = fftLen / 2 + 1;
    const size_t numFrames = (N >= winLen) ? ((N - winLen) / hop + 1) : 0;
    const size_t NC = static_cast<size_t>(numCoeffs);

    // Slaney edges → 42 entries → numBands = 40 (validNumBands depends on fs).
    constexpr size_t numEdges = 42;

    // Output: numFrames × (NumCoeffs+1)  with LogEnergy='append'.
    Value out = Value::matrix(numFrames, numFrames == 0 ? 0 : (NC + 1),
                              ValueType::DOUBLE, mr);
    Value zero = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (numFrames == 0 || winLen == 0)
        return {out, zero, zero};

    ScratchArena scratch(mr);

    // Window
    ScratchVec<double> win(winLen, &scratch);
    hammingPeriodic(win.data(), winLen);

    // Slaney edges + filter bank (H × numBands)
    ScratchVec<double> edges(numEdges, &scratch);
    slaneyBandEdges(edges.data(), numEdges);
    const size_t numBands = numEdges - 2;
    ScratchVec<double> FB(H * numBands, &scratch);
    designMelFilterBankSlaney(FB.data(), fs, fftLen, edges.data(), numEdges, H);

    // Per-frame buffers
    ScratchVec<double> frame(winLen, &scratch);
    ScratchVec<double> mag(H, &scratch);
    ScratchVec<double> melMag(numBands * numFrames, &scratch);
    ScratchVec<double> logE(numFrames, &scratch);

    for (size_t f = 0; f < numFrames; ++f) {
        const size_t start = f * hop;
        // Energy on the UNWINDOWED frame (standard MFCC log-energy term).
        double E = 0.0;
        for (size_t i = 0; i < winLen; ++i) {
            const double xi = x.elemAsDouble(start + i);
            E += xi * xi;
        }
        if (E == 0.0) E = std::numeric_limits<double>::min();
        logE[f] = std::log(E);  // natural log

        // Window then FFT magnitude
        for (size_t i = 0; i < winLen; ++i)
            frame[i] = x.elemAsDouble(start + i) * win[i];
        detail::fftMagHalf(mr, frame.data(), fftLen, mag.data());

        // Apply filterbank: melMag(b, f) = Σ_j FB(j, b) * mag(j)
        for (size_t b = 0; b < numBands; ++b) {
            double s = 0.0;
            for (size_t j = 0; j < H; ++j) s += FB[j + b * H] * mag[j];
            melMag[b + f * numBands] = s;
        }
    }

    // Wrap melMag into a Value for cepstralCoefficients.
    Value melMagV = Value::matrix(numBands, numFrames, ValueType::DOUBLE, mr);
    {
        double *md = melMagV.doubleDataMut();
        std::copy(melMag.data(), melMag.data() + numBands * numFrames, md);
    }
    Value coeffs = cepstralCoefficients(melMagV, static_cast<int>(NC), mr);

    // Assemble out: [logE, coeffs] (numFrames × (NC+1)).
    double *od = out.doubleDataMut();
    const double *Cd = coeffs.doubleData();
    for (size_t f = 0; f < numFrames; ++f) od[f] = logE[f];
    for (size_t n = 0; n < NC; ++n)
        for (size_t f = 0; f < numFrames; ++f)
            od[f + (n + 1) * numFrames] = Cd[f + n * numFrames];

    Value delta      = audioDelta(out, 9, mr);
    Value deltaDelta = audioDelta(delta, 9, mr);
    return {out, delta, deltaDelta};
}

// ── gtcc ──────────────────────────────────────────────────────────────
// Same STFT pipeline as mfcc but with a Patterson-Holdsworth gammatone
// filterbank in the frequency domain (Slaney 1993).
// Pipeline:
//   1. winLen=round(0.03*fs), overlap=round(0.02*fs), fftLen=winLen.
//   2. Per-frame logE = log(sum(unwindowed_frame.^2)).
//   3. hamming(winLen,'periodic'); Z = |FFT(frame .* win)|.
//   4. Gammatone filterbank parameters:
//        FrequencyScale='erb', FrequencyRange=[50, fs/2],
//        OneSided=false, Normalization='Bandwidth'.
//      NumFilters = ceil(hz2erb(fs/2) - hz2erb(50)).
//      Fc = erb2hz(linspace(lowERB, highERB, NumFilters)).
//      Filter shape per band: cascade of 4 biquads
//      (Slaney 1993 — eq. 4.6).
//      H[k] = freqz(coeffs, NFFT, 'whole') magnitude.
//      BW[i] = 1.019 · 24.7 · (0.00437·Fc[i] + 1).
//      Bandwidth norm: H[i,k] /= BW[i]/2.
//   5. melMag[i, frame] = filterBank * Z (full two-sided sum).
//      We compute one-sided H (real coeffs → conjugate symmetric)
//      and double the inner-half bins for equivalent sum.
//   6. cepstralCoefficients (log10 + DCT-II unitary).
//   7. Prepend logE column → numFrames × (NumCoeffs+1).
std::tuple<Value, Value, Value>
gtcc(const Value &x, double fs, int numCoeffs, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    const size_t winLen  = static_cast<size_t>(std::round(fs * 0.03));
    const size_t overlap = static_cast<size_t>(std::round(fs * 0.02));
    const size_t hop = (winLen > overlap) ? (winLen - overlap) : 1;
    const size_t fftLen = winLen;
    const size_t H = fftLen / 2 + 1;
    const size_t numFrames = (N >= winLen) ? ((N - winLen) / hop + 1) : 0;
    const size_t NC = static_cast<size_t>(numCoeffs);

    // ERB-spaced Fc from FrequencyRange=[50, fs/2].
    const double fLow = 50.0;
    const double fHigh = fs * 0.5;
    const double lowERB = hz2erbVal(fLow);
    const double highERB = hz2erbVal(fHigh);
    const size_t numBands = static_cast<size_t>(std::ceil(highERB - lowERB));

    Value out = Value::matrix(numFrames, numFrames == 0 ? 0 : (NC + 1),
                              ValueType::DOUBLE, mr);
    Value zero = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (numFrames == 0 || winLen == 0 || numBands == 0)
        return {out, zero, zero};

    ScratchArena scratch(mr);

    // Window
    ScratchVec<double> win(winLen, &scratch);
    hammingPeriodic(win.data(), winLen);

    // Fc = erb2hz(linspace(lowERB, highERB, numBands))
    ScratchVec<double> Fc(numBands, &scratch);
    if (numBands == 1) {
        Fc[0] = erb2hzVal(lowERB);
    } else {
        const double step = (highERB - lowERB) / static_cast<double>(numBands - 1);
        for (size_t i = 0; i < numBands; ++i)
            Fc[i] = erb2hzVal(lowERB + step * static_cast<double>(i));
    }

    // Gammatone filterbank one-sided (numBands × H).
    ScratchVec<double> bank(numBands * H, &scratch);
    computeGammatoneFreqRespOneSided(fs, Fc.data(), numBands, fftLen, H, bank.data());

    // Bandwidth normalization: BW[i] = 1.019 * 24.7 * (0.00437 * Fc[i] + 1).
    // Then bank[i, k] /= BW[i]/2.
    ScratchVec<double> BW(numBands, &scratch);
    for (size_t i = 0; i < numBands; ++i) {
        BW[i] = 1.019 * 24.7 * (0.00437 * Fc[i] + 1.0);
        const double inv = (BW[i] > 0.0) ? 2.0 / BW[i] : 0.0;
        for (size_t k = 0; k < H; ++k) bank[i * H + k] *= inv;
    }

    // Per-frame buffers
    ScratchVec<double> frame(winLen, &scratch);
    ScratchVec<double> mag(H, &scratch);
    ScratchVec<double> melMag(numBands * numFrames, &scratch);
    ScratchVec<double> logE(numFrames, &scratch);

    for (size_t f = 0; f < numFrames; ++f) {
        const size_t start = f * hop;
        // Energy on UNWINDOWED frame.
        double E = 0.0;
        for (size_t i = 0; i < winLen; ++i) {
            const double xi = x.elemAsDouble(start + i);
            E += xi * xi;
        }
        if (E == 0.0) E = std::numeric_limits<double>::min();
        logE[f] = std::log(E);

        // Window then FFT magnitude
        for (size_t i = 0; i < winLen; ++i)
            frame[i] = x.elemAsDouble(start + i) * win[i];
        detail::fftMagHalf(mr, frame.data(), fftLen, mag.data());

        // Apply two-sided gammatone bank: equivalent sum via one-sided
        // with bins 1..H-2 doubled (k=0 DC, k=H-1 Nyquist single-counted).
        // For odd NFFT, k=H-1 is NOT Nyquist — also double it.
        const bool nyquistSingle = ((fftLen % 2) == 0);
        for (size_t b = 0; b < numBands; ++b) {
            double s = 0.0;
            // k=0 (DC): single
            s += bank[b * H + 0] * mag[0];
            // k=1..H-2: doubled
            for (size_t k = 1; k + 1 < H; ++k)
                s += 2.0 * bank[b * H + k] * mag[k];
            // k=H-1: Nyquist iff fftLen even
            if (H >= 1) {
                const double last = bank[b * H + (H - 1)] * mag[H - 1];
                s += nyquistSingle ? last : 2.0 * last;
            }
            melMag[b + f * numBands] = s;
        }
    }

    // Wrap melMag → cepstralCoefficients
    Value melMagV = Value::matrix(numBands, numFrames, ValueType::DOUBLE, mr);
    {
        double *md = melMagV.doubleDataMut();
        std::copy(melMag.data(), melMag.data() + numBands * numFrames, md);
    }
    Value coeffs = cepstralCoefficients(melMagV, static_cast<int>(NC), mr);

    // Assemble out: [logE, coeffs] (numFrames × (NC+1)).
    double *od = out.doubleDataMut();
    const double *Cd = coeffs.doubleData();
    for (size_t f = 0; f < numFrames; ++f) od[f] = logE[f];
    for (size_t n = 0; n < NC; ++n)
        for (size_t f = 0; f < numFrames; ++f)
            od[f + (n + 1) * numFrames] = Cd[f + n * numFrames];

    Value delta      = audioDelta(out, 9, mr);
    Value deltaDelta = audioDelta(delta, 9, mr);
    return {out, delta, deltaDelta};
}

} // namespace numkit::audio
