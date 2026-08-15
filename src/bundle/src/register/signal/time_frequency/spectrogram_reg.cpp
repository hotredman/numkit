// toolboxes/signal/src/time_frequency/spectrogram_reg.cpp
//
// CallContext register half of time_frequency/spectrogram.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.

// The reg wrappers build Hamming/Hann windows + the radian frequency axis with
// M_PI — define it before any <cmath> include (MSVC needs _USE_MATH_DEFINES).
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <numkit/core/engine.hpp>
#include <numkit/signal/time_frequency/spectrogram.hpp>
#include <numkit/signal/transforms/fft.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::signal {

namespace detail {

void iscola_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("iscola: requires (window, noverlap [, method])",
                     0, 0, "iscola", "", "numkit:iscola:nargin");
    if (args[1].numel() != 1)
        throw Error("iscola: noverlap must be a scalar",
                     0, 0, "iscola", "", "numkit:iscola:badOverlap");
    const double novS = args[1].toScalar();
    if (!(novS >= 0.0))
        throw Error("iscola: noverlap must be non-negative",
                     0, 0, "iscola", "", "numkit:iscola:badOverlap");
    const std::size_t noverlap = static_cast<std::size_t>(novS);

    std::string method = "wola";  // MATLAB default
    if (args.size() >= 3) {
        if (!(args[2].isChar() || args[2].isString()))
            throw Error("iscola: method must be a string",
                         0, 0, "iscola", "", "numkit:iscola:badMethod");
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
                     0, 0, "spectrogram", "", "numkit:spectrogram:nargin");

    // MATLAB's spectrogram(x, N) accepts a scalar-N (window length) and
    // builds a Hamming window of that length. If arg 1 is a vector, it's
    // the explicit window. Adapter handles both by synthesizing a Hamming
    // vector here when it sees a scalar.
    Value window = Value();
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
    const size_t noverlap = (args.size() >= 3 && !args[2].isEmpty())
                                ? static_cast<size_t>(args[2].toScalar()) : 0;
    const size_t nfft = (args.size() >= 4 && !args[3].isEmpty())
                            ? static_cast<size_t>(args[3].toScalar()) : 0;

    auto [S, F, T] = spectrogram(args[0], window, noverlap, nfft, ctx.engine->resource());

    // Sample-rate scaling of the frequency / time axes. MATLAB's
    // spectrogram returns f in Hz = k*fs/nfft and t in seconds = centre/fs
    // when fs is supplied (5th positional arg). With no fs the normalized
    // convention uses fs = 2*pi, so f spans [0, pi] and t = centre/(2*pi).
    // The core builds the normalized axes (f = k*2*pi/nfft, t = centre in
    // samples); rescale both here so f and t honour fs.
    double fs = 2.0 * M_PI;
    if (args.size() >= 5 && !args[4].isEmpty())
        fs = args[4].toScalar();
    {
        const size_t nFreqs  = F.numel();
        const size_t nfftEff = (nFreqs > 1) ? (nFreqs - 1) * 2 : 1;
        double *fd = F.doubleDataMut();
        for (size_t i = 0; i < nFreqs; ++i)
            fd[i] = static_cast<double>(i) * fs / static_cast<double>(nfftEff);
        double *td = T.doubleDataMut();
        for (size_t j = 0; j < T.numel(); ++j)
            td[j] /= fs;
    }

    // 4th output / Auto-plot: ps — one-sided power spectral density per (freq, time),
    // ps = c[k]·|S|² / (fs·Σwin²), with c = 2 on interior bins and 1 at DC
    // and (when present) Nyquist. Reuses the already-computed STFT S.
    Value PS;
    if (nargout == 0 || nargout > 3) {
        const std::size_t R = S.dims().rows();
        const std::size_t C = S.dims().cols();
        const std::size_t nx = args[0].numel();
        // Window coefficients — replicate the core's default (hamming of
        // length floor(nx/4.5)) when none was supplied; keep in sync with
        // resolveWindow() in this TU.
        std::pmr::vector<double> win(ctx.engine->resource());
        if (!window.isEmpty() && window.numel() > 0) {
            const std::size_t L = window.numel();
            win.resize(L);
            for (std::size_t i = 0; i < L; ++i) win[i] = window.elemAsDouble(i);
        } else {
            std::size_t L = std::max<std::size_t>(
                1, static_cast<std::size_t>(std::floor(static_cast<double>(nx) / 4.5)));
            if (L > nx && nx > 0) L = nx;
            win.resize(L);
            if (L == 1) win[0] = 1.0;
            else for (std::size_t i = 0; i < L; ++i)
                win[i] = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (L - 1));
        }
        double U = 0.0;
        for (double w : win) U += w * w;
        const std::size_t nfftEff = (R > 1) ? (R - 1) * 2 : 1;
        const bool nyquist = (nfftEff % 2 == 0);   // last bin is Nyquist
        const double denom = fs * U;
        PS = Value::matrix(R, C, ValueType::DOUBLE, ctx.engine->resource());
        const std::complex<double> *sd = S.complexData();
        double *pd = PS.doubleDataMut();
        for (std::size_t k = 0; k < R; ++k) {
            const double c = (k == 0 || (nyquist && k == R - 1)) ? 1.0 : 2.0;
            for (std::size_t m = 0; m < C; ++m) {
                const std::complex<double> v = sd[m * R + k];
                pd[m * R + k] = c * std::norm(v) / denom;
            }
        }
    }

    // Auto-plot when called without LHS (MATLAB spectrogram convention).
    if (nargout == 0) {
        const std::size_t R = S.dims().rows();
        const std::size_t C = S.dims().cols();
        Value pdb = Value::matrix(R, C, ValueType::DOUBLE, ctx.engine->resource());
        const double *pd = PS.doubleData();
        double *pdb_data = pdb.doubleDataMut();
        for (std::size_t i = 0; i < R * C; ++i) {
            double v = pd[i];
            if (v < 1e-15) v = 1e-15;
            pdb_data[i] = 10.0 * std::log10(v);
        }
        Value imagescFn = Value::funcHandle("imagesc", ctx.engine->resource());
        Value imgArgs[3] = { T, F, pdb };
        ctx.engine->callFunctionHandle(imagescFn, Span<const Value>(imgArgs, 3), ctx.env);
        auto &fm = ctx.engine->figureManager();
        auto &ax = fm.currentAxes();
        ax.axisMode = "xy";
        ax.yDir = "normal"; // normal frequency axis orientation: 0 Hz at bottom
        if (ax.xlabel.empty()) ax.xlabel = "Time (s)";
        if (ax.ylabel.empty()) ax.ylabel = "Frequency (Hz)";
        return;
    }

    outs[0] = std::move(S);
    if (nargout > 1)
        outs[1] = std::move(F);
    if (nargout > 2)
        outs[2] = std::move(T);
    if (nargout > 3)
        outs[3] = std::move(PS);
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
                         0, 0, "stft", "", "numkit:stft:badNVName");
        const std::string key = args[i].toString();
        const Value &val      = args[i + 1];
        if (eqIgnoreCase(key, "Window"))           window    = val;
        else if (eqIgnoreCase(key, "OverlapLength")) overlap = static_cast<std::size_t>(val.toScalar());
        else if (eqIgnoreCase(key, "FFTLength"))   fftLength = static_cast<std::size_t>(val.toScalar());
        else if (eqIgnoreCase(key, "FrequencyRange")) range  = val.toString();
        else
            throw Error("stft: unknown name-value key '" + key + "'",
                         0, 0, "stft", "", "numkit:stft:badNVKey");
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
                     0, 0, "stft", "", "numkit:stft:nargin");
    auto *mr = ctx.engine->resource();

    bool fsGiven = false;
    double fs    = 1.0;
    const size_t nvStart = parseOptionalFs(args, fsGiven, fs);

    Value window           = Value();
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
                     0, 0, "istft", "", "numkit:istft:nargin");
    auto *mr = ctx.engine->resource();

    bool fsGiven = false;
    double fs    = 1.0;
    const size_t nvStart = parseOptionalFs(args, fsGiven, fs);

    Value window           = Value();
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
