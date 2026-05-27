// libs/signal/src/time_frequency/spectrogram.cpp
//
// Short-time Fourier transform. Split from spectral_analysis/. The
// time-collapsed power-spectrum estimators periodogram / pwelch
// live in spectral_analysis/periodogram_pwelch.cpp.

#include <numkit/signal/time_frequency/spectrogram.hpp>
#include <numkit/signal/transforms/fft.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "../dsp_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <memory_resource>
#include <string>
#include <tuple>

namespace numkit::signal {

namespace {

// Fill caller-provided buffer with Hamming window coefficients, same
// formula MATLAB uses.
void fillHammingWindow(double *w, size_t N)
{
    if (N == 1) {
        w[0] = 1.0;
        return;
    }
    for (size_t i = 0; i < N; ++i)
        w[i] = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (N - 1));
}

// Periodic Hann window (MATLAB hann(N, 'periodic') — used as the stft
// default. Note the divisor is N, NOT N-1 (that's the symmetric form).
void fillHannPeriodic(double *w, size_t N)
{
    if (N == 1) { w[0] = 1.0; return; }
    for (size_t i = 0; i < N; ++i)
        w[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / N));
}

} // anonymous namespace

std::tuple<Value, Value, Value>
spectrogram(const Value &x, const Value &window, size_t noverlap, size_t nfft, std::pmr::memory_resource *mr)
{
    const size_t nx = x.numel();
    const double *xd = x.doubleData();

    ScratchArena scratch(mr);

    size_t winLen;
    ScratchVec<double> win(&scratch);
    if (window.numel() > 0) {
        winLen = window.numel();
        win.resize(winLen);
        const double *w = window.doubleData();
        for (size_t i = 0; i < winLen; ++i)
            win[i] = w[i];
    } else {
        // MATLAB spectrogram default: 8 Hamming-windowed segments with
        // 50% overlap (winLen = floor(nx / 4.5)).
        winLen = std::max<size_t>(1,
            static_cast<size_t>(std::floor(static_cast<double>(nx) / 4.5)));
        if (winLen > nx) winLen = nx;
        win.resize(winLen);
        fillHammingWindow(win.data(), winLen);
    }

    if (winLen > nx)
        winLen = nx;

    if (noverlap == 0)
        noverlap = winLen / 2;
    if (nfft == 0)
        nfft = std::max<size_t>(256, nextPow2(winLen));

    const size_t nFreqs = nfft / 2 + 1;
    const size_t step = winLen - noverlap;

    size_t nSegments = 0;
    for (size_t start = 0; start + winLen <= nx; start += step)
        nSegments++;

    auto S = Value::complexMatrix(nFreqs, nSegments, mr);
    auto F = Value::matrix(nFreqs, 1, ValueType::DOUBLE, mr);
    auto T = Value::matrix(1, nSegments, ValueType::DOUBLE, mr);

    // Per-segment FFT buffer hoisted: see pwelch for the rationale —
    // a fresh allocation per loop iteration would grow the arena to
    // O(nSegments × nfft) instead of O(nfft).
    auto buf = ScratchVec<Complex>(nfft, &scratch);

    size_t seg = 0;
    for (size_t start = 0; start + winLen <= nx; start += step) {
        for (size_t i = 0; i < winLen; ++i)
            buf[i] = Complex(xd[start + i] * win[i], 0.0);
        for (size_t i = winLen; i < nfft; ++i)
            buf[i] = Complex(0.0, 0.0);

        fftRadix2(&scratch, buf, 1);

        for (size_t i = 0; i < nFreqs; ++i)
            S.complexDataMut()[i + seg * nFreqs] = buf[i];

        T.doubleDataMut()[seg] = static_cast<double>(start + winLen / 2);
        seg++;
    }

    for (size_t i = 0; i < nFreqs; ++i)
        F.doubleDataMut()[i] = M_PI * i / (nFreqs - 1);

    return std::make_tuple(std::move(S), std::move(F), std::move(T));
}

// ── stft / istft (MATLAB-compat) ─────────────────────────────────────
//
// MATLAB stft defaults: hann(128, 'periodic'), overlap = 96,
// fftLength = 128, range = 'twosided'. Output is FFT-bin × frame.
// MATLAB packs the columns in standard fft order; 'centered' applies
// an fftshift along the frequency axis, 'onesided' keeps bins 0..N/2.
//
// istft inverts via overlap-add with the same window — for the
// canonical COLA configurations (hann/periodic + 50%/75% overlap) the
// round-trip is bit-exact within ulp.
namespace {

void resolveWindow(const Value &win, ScratchVec<double> &out, std::pmr::memory_resource *mr)
{
    if (win.numel() > 0) {
        const size_t n = win.numel();
        out.resize(n);
        for (size_t i = 0; i < n; ++i) out[i] = win.elemAsDouble(i);
        return;
    }
    // Default: hann(128, 'periodic').
    out.resize(128);
    fillHannPeriodic(out.data(), 128);
    (void) mr;
}

} // anonymous namespace

Value stft(const Value &x, const Value &window, std::size_t overlap, std::size_t fftLength, const std::string &range, std::pmr::memory_resource *mr)
{
    using Cd = std::complex<double>;
    const size_t N = x.numel();
    if (N == 0)
        return Value::complexMatrix(0, 0, mr);

    ScratchArena scratch(mr);
    ScratchVec<double> win(&scratch);
    resolveWindow(window, win, mr);
    const size_t M = win.size();
    if (M == 0)
        throw Error("stft: window must be non-empty",
                     0, 0, "stft", "", "m:stft:badWindow");
    if (M > N)
        throw Error("stft: signal shorter than window length",
                     0, 0, "stft", "", "m:stft:shortSignal");

    const size_t OL  = (overlap == SIZE_MAX) ? (3 * M) / 4 : overlap;
    if (OL >= M)
        throw Error("stft: OverlapLength must be < window length",
                     0, 0, "stft", "", "m:stft:badOverlap");
    const size_t NFFT = (fftLength == 0) ? M : fftLength;
    if (NFFT < M)
        throw Error("stft: FFTLength must be >= window length",
                     0, 0, "stft", "", "m:stft:badNfft");

    if (range != "twosided" && range != "onesided" && range != "centered")
        throw Error("stft: FrequencyRange must be 'twosided', 'centered' "
                    "or 'onesided'",
                     0, 0, "stft", "", "m:stft:badRange");

    const size_t hop = M - OL;
    const size_t K   = (N - M) / hop + 1;
    const bool   isOneSided = (range == "onesided");
    const bool   isCentered = (range == "centered");
    const size_t outRows = isOneSided ? (NFFT / 2 + 1) : NFFT;
    // Centered rotation: MATLAB places bins as [-N/2+1, ..., N/2] for
    // even N (Nyquist at the END), or [-(N-1)/2, ..., (N-1)/2] for odd
    // N. So DC sits at index N/2-1 (even) or (N-1)/2 (odd). To produce
    //   Sd[k] = Fd[(k + cShift) mod N]
    // with Sd[DC_idx] = Fd[0], we need cShift = N - DC_idx:
    //   even N: cShift = N/2 + 1
    //   odd  N: cShift = (N + 1) / 2
    const size_t cShift = (NFFT % 2 == 0) ? (NFFT / 2 + 1)
                                          : ((NFFT + 1) / 2);

    // x as doubles. Complex input also supported via fft(complex frame).
    const bool xCplx = x.isComplex();
    const double *xr = xCplx ? nullptr : x.doubleData();
    const Cd     *xc = xCplx ? x.complexData() : nullptr;

    Value S = Value::complexMatrix(outRows, K, mr);
    Cd *Sd = S.complexDataMut();

    // Per-frame buffer reused across iterations.
    Value frameV = xCplx
        ? Value::complexMatrix(NFFT, 1, mr)
        : Value::matrix(NFFT, 1, ValueType::DOUBLE, mr);
    Cd     *frameCd = xCplx ? frameV.complexDataMut() : nullptr;
    double *frameRd = xCplx ? nullptr               : frameV.doubleDataMut();

    for (size_t k = 0; k < K; ++k) {
        const size_t start = k * hop;
        if (xCplx) {
            for (size_t i = 0; i < M; ++i)
                frameCd[i] = xc[start + i] * win[i];
            for (size_t i = M; i < NFFT; ++i) frameCd[i] = Cd(0, 0);
        } else {
            for (size_t i = 0; i < M; ++i)
                frameRd[i] = xr[start + i] * win[i];
            for (size_t i = M; i < NFFT; ++i) frameRd[i] = 0.0;
        }
        Value F = fft(frameV, static_cast<int>(NFFT), 1, mr);
        const Cd *Fd = F.complexData();
        if (isCentered) {
            for (size_t r = 0; r < outRows; ++r)
                Sd[k * outRows + r] = Fd[(r + cShift) % NFFT];
        } else {
            // twosided / onesided: copy the first outRows bins as-is.
            for (size_t r = 0; r < outRows; ++r)
                Sd[k * outRows + r] = Fd[r];
        }
    }
    return S;
}

Value istft(const Value &S, const Value &window, std::size_t overlap, std::size_t fftLength, const std::string &range, std::pmr::memory_resource *mr)
{
    using Cd = std::complex<double>;

    const auto &d = S.dims();
    const size_t inRows = d.rows();
    const size_t K      = d.cols();
    if (K == 0 || inRows == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    ScratchArena scratch(mr);
    ScratchVec<double> win(&scratch);
    resolveWindow(window, win, mr);
    const size_t M = win.size();
    if (M == 0)
        throw Error("istft: window must be non-empty",
                     0, 0, "istft", "", "m:istft:badWindow");

    const size_t OL  = (overlap == SIZE_MAX) ? (3 * M) / 4 : overlap;
    if (OL >= M)
        throw Error("istft: OverlapLength must be < window length",
                     0, 0, "istft", "", "m:istft:badOverlap");

    if (range != "twosided" && range != "onesided" && range != "centered")
        throw Error("istft: FrequencyRange must be 'twosided', 'centered' "
                    "or 'onesided'",
                     0, 0, "istft", "", "m:istft:badRange");

    const bool   isOneSided = (range == "onesided");
    const bool   isCentered = (range == "centered");
    size_t NFFT = (fftLength == 0)
                    ? (isOneSided ? 2 * (inRows - 1) : inRows)
                    : fftLength;
    // Inverse of the centered rotation (see stft above): the input
    // column came from twosided via Sd[k] = Fd[(k + cShift) mod N], so
    // we recover Fd[j] = Sd[(j - cShift + N) mod N]. Equivalent forward
    // shift = N - cShift.
    const size_t invCShift = NFFT - ((NFFT % 2 == 0) ? (NFFT / 2 + 1)
                                                     : ((NFFT + 1) / 2));
    if (!isOneSided && inRows != NFFT)
        throw Error("istft: STFT row count does not match FFTLength",
                     0, 0, "istft", "", "m:istft:badShape");
    if (isOneSided && inRows != NFFT / 2 + 1)
        throw Error("istft: one-sided STFT row count must equal "
                    "FFTLength/2 + 1",
                     0, 0, "istft", "", "m:istft:badShape");
    if (NFFT < M)
        throw Error("istft: FFTLength must be >= window length",
                     0, 0, "istft", "", "m:istft:badNfft");

    const size_t hop = M - OL;
    const size_t Nout = (K - 1) * hop + M;

    Value out = Value::matrix(Nout, 1, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    std::fill(od, od + Nout, 0.0);

    // Per-sample window-square accumulator for normalization.
    ScratchVec<double> wnorm(Nout, 0.0, &scratch);

    // Per-frame full spectrum (after undoing centered / onesided
    // packing) and ifft-result.
    Value Fcol = Value::complexMatrix(NFFT, 1, mr);
    Cd *Fd = Fcol.complexDataMut();
    const Cd *Sd = S.complexData();

    for (size_t k = 0; k < K; ++k) {
        // Repopulate Fcol from S column k according to range.
        if (isOneSided) {
            // Hermitian reflection: F[N-i] = conj(F[i]) for i=1..N/2-1.
            for (size_t r = 0; r < inRows; ++r) Fd[r] = Sd[k * inRows + r];
            for (size_t r = 1; r < NFFT - inRows + 1; ++r)
                Fd[NFFT - r] = std::conj(Fd[r]);
        } else if (isCentered) {
            // Undo the centered rotation.
            for (size_t r = 0; r < NFFT; ++r)
                Fd[r] = Sd[k * inRows + ((r + invCShift) % NFFT)];
        } else {
            for (size_t r = 0; r < NFFT; ++r) Fd[r] = Sd[k * inRows + r];
        }

        Value t = ifft(Fcol, static_cast<int>(NFFT), 1, mr);
        // ifft may return DOUBLE or COMPLEX.
        const Cd     *tc = (t.type() == ValueType::COMPLEX) ? t.complexData() : nullptr;
        const double *tr = (t.type() == ValueType::COMPLEX) ? nullptr        : t.doubleData();

        const size_t start = k * hop;
        for (size_t i = 0; i < M; ++i) {
            const double real_i = tc ? tc[i].real() : tr[i];
            od[start + i]    += win[i] * real_i;
            wnorm[start + i] += win[i] * win[i];
        }
    }

    // Normalize sample-wise so COLA-compliant windows reach identity
    // reconstruction. The wnorm[n] = 0 case can only happen outside the
    // covered support (impossible by construction); guard anyway.
    for (size_t i = 0; i < Nout; ++i)
        if (wnorm[i] > 0.0) od[i] /= wnorm[i];

    return out;
}

// ── iscola — Constant OverLap-Add compliance check ─────────────────
//
// MATLAB sig: [tf, m, maxDev] = iscola(window, noverlap[, method]).
// Default method is 'wola' (sum of squared window); 'ola' uses the
// unsquared window. The algorithm builds K = ceil(M/hop) + 2 frames
// stacked at multiples of hop = M - noverlap, sums them up sample-by-
// sample, then inspects the stable overlap region (where every interior
// sample is covered by `nOverlapWindows = ceil(M/hop)` frames). The
// region length is at least `hop` so the periodicity-1 sum can be
// fully characterised by its median and its maximum deviation. Match
// boundary: `maxDev < 2 * eps` (probed against MATLAB R2025b: dev = 1·eps
// passes, dev ≥ 2·eps fails).

std::tuple<Value, Value, Value>
iscola(const Value &window, std::size_t noverlap, const std::string &method,
       std::pmr::memory_resource *mr)
{
    const std::size_t M = window.numel();
    if (M == 0)
        throw Error("iscola: window must be non-empty",
                     0, 0, "iscola", "", "m:iscola:badWindow");
    if (noverlap >= M)
        throw Error("iscola: noverlap must be < window length",
                     0, 0, "iscola", "", "m:iscola:badOverlap");
    if (method != "ola" && method != "wola")
        throw Error("iscola: method must be 'ola' or 'wola'",
                     0, 0, "iscola", "", "m:iscola:badMethod");

    const std::size_t hop  = M - noverlap;
    const std::size_t nOvw = (M + hop - 1) / hop;        // ceil(M/hop)
    const std::size_t K    = nOvw + 2;
    const std::size_t Ltot = (K - 1) * hop + M;
    const bool isWola      = (method == "wola");

    ScratchArena scratch(mr);
    ScratchVec<double> s(Ltot, 0.0, &scratch);
    for (std::size_t k = 0; k < K; ++k)
        for (std::size_t i = 0; i < M; ++i) {
            const double wi = window.elemAsDouble(i);
            s[k * hop + i] += isWola ? wi * wi : wi;
        }

    // Stable region: [(nOvw - 1)*hop, K*hop). Take the first `hop`-long
    // window of it — by periodicity within the stable region every
    // hop-sized slice has the same content (up to FP).
    const std::size_t stableStart = (nOvw - 1) * hop;
    ScratchVec<double> stable(s.begin() + stableStart,
                               s.begin() + stableStart + hop, &scratch);

    // Median.
    ScratchVec<double> sorted(stable.begin(), stable.end(), &scratch);
    std::sort(sorted.begin(), sorted.end());
    const std::size_t H = sorted.size();
    const double median = (H % 2 == 0)
        ? 0.5 * (sorted[H / 2 - 1] + sorted[H / 2])
        : sorted[H / 2];

    // Max absolute deviation from the median.
    double maxDev = 0.0;
    for (double v : stable)
        maxDev = std::max(maxDev, std::abs(v - median));

    // MATLAB tolerance is relative to the median (probed): dev/|m| ≤ eps
    // passes, dev/|m| > eps fails. e.g. hann@48 'ola' has dev = 2·eps and
    // m = 2 → ratio = eps (exact boundary), tf = 1.
    const double eps = std::numeric_limits<double>::epsilon();
    const double tol = (std::abs(median) > 0.0) ? std::abs(median) * eps : eps;
    const bool   tf  = (maxDev <= tol);

    Value out_tf  = Value::scalar(tf ? 1.0 : 0.0, mr);
    Value out_m   = Value::scalar(median, mr);
    Value out_dev = Value::scalar(maxDev, mr);
    return { std::move(out_tf), std::move(out_m), std::move(out_dev) };
}

namespace detail {

void iscola_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("iscola: requires (window, noverlap [, method])",
                     0, 0, "iscola", "", "m:iscola:nargin");
    if (args[1].numel() != 1)
        throw Error("iscola: noverlap must be a scalar",
                     0, 0, "iscola", "", "m:iscola:badOverlap");
    const double novS = args[1].toScalar();
    if (!(novS >= 0.0))
        throw Error("iscola: noverlap must be non-negative",
                     0, 0, "iscola", "", "m:iscola:badOverlap");
    const std::size_t noverlap = static_cast<std::size_t>(novS);

    std::string method = "wola";  // MATLAB default
    if (args.size() >= 3) {
        if (!(args[2].isChar() || args[2].isString()))
            throw Error("iscola: method must be a string",
                         0, 0, "iscola", "", "m:iscola:badMethod");
        method = args[2].toString();
    }

    auto [tf, m, dev] = iscola(args[0], noverlap, method,
                                ctx.engine->resource());
    outs[0] = std::move(tf);
    if (nargout > 1) outs[1] = std::move(m);
    if (nargout > 2) outs[2] = std::move(dev);
}

void spectrogram_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("spectrogram: requires at least 1 argument",
                     0, 0, "spectrogram", "", "m:spectrogram:nargin");

    // MATLAB's spectrogram(x, N) accepts a scalar-N (window length) and
    // builds a Hamming window of that length. If arg 1 is a vector, it's
    // the explicit window. Adapter handles both by synthesizing a Hamming
    // vector here when it sees a scalar.
    Value window = Value::empty();
    if (args.size() >= 2 && !args[1].isChar()) {
        if (args[1].numel() == 1) {
            const size_t winLen = static_cast<size_t>(args[1].toScalar());
            auto w = Value::matrix(1, winLen, ValueType::DOUBLE, ctx.engine->resource());
            if (winLen == 1) {
                w.doubleDataMut()[0] = 1.0;
            } else {
                for (size_t i = 0; i < winLen; ++i)
                    w.doubleDataMut()[i] = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (winLen - 1));
            }
            window = std::move(w);
        } else {
            window = args[1];
        }
    }
    const size_t noverlap = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 0;
    const size_t nfft = (args.size() >= 4) ? static_cast<size_t>(args[3].toScalar()) : 0;

    auto [S, F, T] = spectrogram(args[0], window, noverlap, nfft, ctx.engine->resource());
    outs[0] = std::move(S);
    if (nargout > 1)
        outs[1] = std::move(F);
    if (nargout > 2)
        outs[2] = std::move(T);
}

// Parse MATLAB-style trailing name-value pairs into the stft/istft
// argument set. Recognised keys (case-insensitive): Window, OverlapLength,
// FFTLength, FrequencyRange. Unknown keys throw — better to fail fast
// than to silently misuse a guess.
static void parseStftNVPairs(Span<const Value> args, size_t startIdx,
                             Value &window, std::size_t &overlap,
                             std::size_t &fftLength, std::string &range)
{
    auto eqIgnoreCase = [](const std::string &a, const char *b) {
        if (a.size() != std::strlen(b)) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (std::tolower(a[i]) != std::tolower(b[i])) return false;
        return true;
    };
    for (size_t i = startIdx; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar())
            throw Error("stft: name-value pair name must be a string",
                         0, 0, "stft", "", "m:stft:badNVName");
        const std::string key = args[i].toString();
        const Value &val      = args[i + 1];
        if (eqIgnoreCase(key, "Window"))           window    = val;
        else if (eqIgnoreCase(key, "OverlapLength")) overlap = static_cast<std::size_t>(val.toScalar());
        else if (eqIgnoreCase(key, "FFTLength"))   fftLength = static_cast<std::size_t>(val.toScalar());
        else if (eqIgnoreCase(key, "FrequencyRange")) range  = val.toString();
        else
            throw Error("stft: unknown name-value key '" + key + "'",
                         0, 0, "stft", "", "m:stft:badNVKey");
    }
}

// Optional positional `fs` between `x` and any NV-pairs. Returns the
// index where NV-pairs start (1 if no fs, 2 if fs consumed).
static size_t parseOptionalFs(Span<const Value> args, bool &fsGiven, double &fs)
{
    fsGiven = false;
    fs = 1.0;
    if (args.size() >= 2 && !args[1].isChar() && !args[1].isString()
        && args[1].numel() == 1) {
        fs = args[1].toScalar();
        fsGiven = true;
        return 2;
    }
    return 1;
}

// Build the frequency-axis vector that MATLAB returns from
// `[s, f, ...] = stft(...)`. The angular spacing is `f_scale / NFFT`
// where `f_scale = fs` when given, else `2*pi` (radians/sample default).
static Value buildFreqAxis(std::size_t NFFT, const std::string &range,
                           bool fsGiven, double fs,
                           std::pmr::memory_resource *mr)
{
    const double f_scale = fsGiven ? fs : (2.0 * M_PI);
    const double df      = f_scale / static_cast<double>(NFFT);
    if (range == "onesided") {
        const std::size_t nF = NFFT / 2 + 1;
        Value f = Value::matrix(nF, 1, ValueType::DOUBLE, mr);
        double *fd = f.doubleDataMut();
        for (std::size_t k = 0; k < nF; ++k)
            fd[k] = static_cast<double>(k) * df;
        return f;
    }
    if (range == "centered") {
        Value f = Value::matrix(NFFT, 1, ValueType::DOUBLE, mr);
        double *fd = f.doubleDataMut();
        // Even N: bins are [-N/2+1, ..., N/2] (Nyquist at end).
        // Odd  N: bins are [-(N-1)/2, ..., (N-1)/2].
        const long long Nll = static_cast<long long>(NFFT);
        const long long lo  = (Nll % 2 == 0) ? -(Nll / 2 - 1) : -((Nll - 1) / 2);
        for (long long k = 0; k < Nll; ++k)
            fd[k] = static_cast<double>(lo + k) * df;
        return f;
    }
    // twosided: bins 0, 1, ..., N-1.
    Value f = Value::matrix(NFFT, 1, ValueType::DOUBLE, mr);
    double *fd = f.doubleDataMut();
    for (std::size_t k = 0; k < NFFT; ++k)
        fd[k] = static_cast<double>(k) * df;
    return f;
}

// Build the time-axis vector for stft frame centres. MATLAB places
// each frame's centre at sample `(M/2 + k*hop)` (0-based), scaled by
// `1/fs_t` where `fs_t = fs` if given else 1 (samples).
static Value buildTimeAxisStft(std::size_t M, std::size_t hop, std::size_t K,
                               bool fsGiven, double fs,
                               std::pmr::memory_resource *mr)
{
    const double tscale = fsGiven ? fs : 1.0;
    const double half_M = 0.5 * static_cast<double>(M);
    Value t = Value::matrix(K, 1, ValueType::DOUBLE, mr);
    double *td = t.doubleDataMut();
    for (std::size_t k = 0; k < K; ++k)
        td[k] = (half_M + static_cast<double>(k * hop)) / tscale;
    return t;
}

// Resolve effective window length M given an explicit/empty Window
// arg, matching `resolveWindow` defaults.
static std::size_t resolveWindowLen(const Value &window)
{
    return (window.numel() > 0) ? window.numel() : 128;
}

void stft_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
              CallContext &ctx)
{
    if (args.empty())
        throw Error("stft: requires at least 1 argument",
                     0, 0, "stft", "", "m:stft:nargin");
    auto *mr = ctx.engine->resource();

    bool fsGiven = false;
    double fs    = 1.0;
    const size_t nvStart = parseOptionalFs(args, fsGiven, fs);

    Value window           = Value::empty();
    std::size_t overlap    = SIZE_MAX;
    std::size_t fftLength  = 0;
    std::string range      = "centered";  // matches MATLAB R2019b+ default
    parseStftNVPairs(args, nvStart, window, overlap, fftLength, range);

    // Resolve sizes (mirrors stft() internals) so we can build f, t.
    const std::size_t M    = resolveWindowLen(window);
    const std::size_t OL   = (overlap == SIZE_MAX) ? (3 * M) / 4 : overlap;
    const std::size_t hop  = (OL < M) ? (M - OL) : 1;
    const std::size_t NFFT = (fftLength == 0) ? M : fftLength;
    const std::size_t N    = args[0].numel();
    const std::size_t K    = (N >= M) ? ((N - M) / hop + 1) : 0;

    outs[0] = stft(args[0], window, overlap, fftLength, range, mr);
    if (nargout > 1)
        outs[1] = buildFreqAxis(NFFT, range, fsGiven, fs, mr);
    if (nargout > 2)
        outs[2] = buildTimeAxisStft(M, hop, K, fsGiven, fs, mr);
}

void istft_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
               CallContext &ctx)
{
    if (args.empty())
        throw Error("istft: requires at least 1 argument",
                     0, 0, "istft", "", "m:istft:nargin");
    auto *mr = ctx.engine->resource();

    bool fsGiven = false;
    double fs    = 1.0;
    const size_t nvStart = parseOptionalFs(args, fsGiven, fs);

    Value window           = Value::empty();
    std::size_t overlap    = SIZE_MAX;
    std::size_t fftLength  = 0;
    std::string range      = "centered";
    parseStftNVPairs(args, nvStart, window, overlap, fftLength, range);

    outs[0] = istft(args[0], window, overlap, fftLength, range, mr);
    if (nargout > 1) {
        // t = (0 : Nout-1) / fs_t. Reconstructed length is in outs[0]'s rows.
        const std::size_t Nout = outs[0].dims().rows();
        const double tscale    = fsGiven ? fs : 1.0;
        Value t = Value::matrix(Nout, 1, ValueType::DOUBLE, mr);
        double *td = t.doubleDataMut();
        for (std::size_t i = 0; i < Nout; ++i)
            td[i] = static_cast<double>(i) / tscale;
        outs[1] = std::move(t);
    }
}

} // namespace detail

} // namespace numkit::signal
