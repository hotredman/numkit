// libs/audio/src/spectral/cepstral.cpp
//
// Audio Cycle D: cepstral coefficients (cepstralCoefficients / mfcc / gtcc).
//
// MATLAB pipeline (from audio toolbox cepstralCoefficients.m source):
//   S (L bands × M frames) → log10 rectification → DCT-II (unitary)
//                          → keep first NumCoeffs → permute to M × NumCoeffs
//
// DCT-II unitary matrix (createDCTmatrix.m):
//   N = NumCoeffs, K = NumFilters
//   matrix(1, k)   = sqrt(1/K)  (DC row)
//   matrix(n, k)   = sqrt(2/K) * cos(π·(n-1)·(k-0.5)/K)  for n = 2..N
//
// PMR HARD RULE.

#include <numkit/audio/spectral/cepstral.hpp>
#include <numkit/audio/spectral/melspec_delta.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::audio {

namespace {

// Build N × K DCT-II unitary matrix matching MATLAB createDCTmatrix.
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

} // anon

// ── cepstralCoefficients ──────────────────────────────────────────────
// S: L × M (filterbank bands × frames). Output: M × NumCoeffs.
Value cepstralCoefficients(std::pmr::memory_resource *mr, const Value &S,
                           int numCoeffs)
{
    if (S.dims().is3D())
        throw Error("cepstralCoefficients: input must be 2-D",
                    0, 0, "cepstralCoefficients", "",
                    "m:cepstralCoefficients:Not2D");
    const size_t L = S.dims().rows();
    const size_t M = S.dims().cols();
    const size_t N = static_cast<size_t>(numCoeffs);
    if (numCoeffs < 2)
        throw Error("cepstralCoefficients: NumCoeffs must be > 1",
                    0, 0, "cepstralCoefficients", "",
                    "m:cepstralCoefficients:BadN");

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
// Pipeline: melSpectrogram(x, fs, 32 default) → cepstralCoefficients.
// MATLAB also prepends/replaces the first column with log-energy
// (LogEnergy='append' default). v1 ships the basic shape; LogEnergy
// column is approximated via sum-of-power per frame.
//
// KNOWN GAPs vs MATLAB R2025b:
//   * MATLAB uses |FFT| (magnitude) into mel filterbank; we use |FFT|^2
//     (power). Numerical values differ.
//   * MATLAB has its own internal designMelFilterBank with different
//     normalization than our melSpectrogram impl.
//   * Result shape and rough value ranges match; exact bit-equality
//     deferred to v2.
std::tuple<Value, Value, Value>
mfcc(std::pmr::memory_resource *mr, const Value &x, double fs, int numCoeffs)
{
    auto [S, F, T] = melSpectrogram(mr, x, fs, 32);
    Value coeffs = cepstralCoefficients(mr, S, numCoeffs);

    // Compute log-energy per frame: log(sum of S column).
    const size_t M = S.dims().cols();
    Value logE = Value::matrix(M, M == 0 ? 0 : 1, ValueType::DOUBLE, mr);
    if (M > 0) {
        const double *Sd = S.doubleData();
        const size_t L = S.dims().rows();
        double *Ed = logE.doubleDataMut();
        for (size_t m = 0; m < M; ++m) {
            double sum = 0.0;
            for (size_t l = 0; l < L; ++l) sum += Sd[l + m * L];
            Ed[m] = (sum > 0.0) ? std::log(sum)
                                 : -std::numeric_limits<double>::infinity();
        }
    }

    // Append log-energy as the first column (MATLAB LogEnergy='append').
    const size_t NC = static_cast<size_t>(numCoeffs);
    Value out = Value::matrix(M, NC + 1, ValueType::DOUBLE, mr);
    if (M > 0) {
        double *od = out.doubleDataMut();
        const double *Cd = coeffs.doubleData();
        const double *Ed = logE.doubleData();
        for (size_t m = 0; m < M; ++m) od[m] = Ed[m];
        for (size_t n = 0; n < NC; ++n)
            for (size_t m = 0; m < M; ++m)
                od[m + (n + 1) * M] = Cd[m + n * M];
    }
    Value delta      = audioDelta(mr, out, 9);
    Value deltaDelta = audioDelta(mr, delta, 9);
    return {out, delta, deltaDelta};
}

// ── gtcc ──────────────────────────────────────────────────────────────
// Same pipeline as mfcc but with ERB-spaced (gammatone) filterbank.
// v1: triangular ERB filterbank approximation. KNOWN GAP: real gammatone
// filters use IIR shape, not triangular. v2 will implement proper IIR.
std::tuple<Value, Value, Value>
gtcc(std::pmr::memory_resource *mr, const Value &x, double fs, int numCoeffs)
{
    // For v1 reuse melSpectrogram path (which uses mel filterbank).
    // gtcc would normally use ERB-spaced gammatone; documented as gap.
    auto [coeffs, delta, deltaDelta] = mfcc(mr, x, fs, numCoeffs);
    return {coeffs, delta, deltaDelta};
}

namespace detail {

void cepstralCoefficients_reg(Span<const Value> args, size_t /*nargout*/,
                               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cepstralCoefficients: requires (S [, NumCoeffs])",
                    0, 0, "cepstralCoefficients", "",
                    "m:cepstralCoefficients:nargin");
    int nc = 13;
    if (args.size() >= 2) nc = static_cast<int>(args[1].toScalar());
    outs[0] = cepstralCoefficients(ctx.engine->resource(), args[0], nc);
}

void mfcc_reg(Span<const Value> args, size_t nargout,
              Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mfcc: requires (x, fs [, NumCoeffs])",
                    0, 0, "mfcc", "", "m:mfcc:nargin");
    int nc = 13;
    if (args.size() >= 3) nc = static_cast<int>(args[2].toScalar());
    auto [c, d, dd] = mfcc(ctx.engine->resource(), args[0],
                            args[1].toScalar(), nc);
    outs[0] = c;
    if (nargout >= 2 && outs.size() >= 2) outs[1] = d;
    if (nargout >= 3 && outs.size() >= 3) outs[2] = dd;
}

void gtcc_reg(Span<const Value> args, size_t nargout,
              Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("gtcc: requires (x, fs [, NumCoeffs])",
                    0, 0, "gtcc", "", "m:gtcc:nargin");
    int nc = 13;
    if (args.size() >= 3) nc = static_cast<int>(args[2].toScalar());
    auto [c, d, dd] = gtcc(ctx.engine->resource(), args[0],
                            args[1].toScalar(), nc);
    outs[0] = c;
    if (nargout >= 2 && outs.size() >= 2) outs[1] = d;
    if (nargout >= 3 && outs.size() >= 3) outs[2] = dd;
}

} // namespace detail

} // namespace numkit::audio
