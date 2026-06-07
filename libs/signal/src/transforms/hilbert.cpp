// libs/signal/src/transforms/hilbert.cpp
//
// FFT-based Hilbert transform + envelope. unwrap moved to
// filter_analysis/unwrap.cpp.

#include <numkit/signal/transforms/hilbert.hpp>

#include <numkit/builtin/math/interp/interp.hpp>     // interp1 (spline)
#include <numkit/signal/windows/windows.hpp>          // kaiser

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include "../dsp_helpers.hpp"  // Complex, FFT helpers
#include "helpers.hpp"         // createLike

#include <algorithm>
#include <cmath>
#include <complex>
#include <memory_resource>
#include <vector>

namespace numkit::signal {

namespace {

// Shared FFT-based Hilbert transform kernel used by both hilbert() and
// envelope(). Returns a buffer of length fftLen holding the analytic
// signal (possibly zero-padded beyond N). Caller slices to the first N
// samples. Backed by the caller's scratch arena.
ScratchVec<Complex> hilbertBuf(const Value &x, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    const size_t fftLen = nextPow2(N);

    auto buf = prepareFFTBuffer(mr, x, N, fftLen);
    // Forward FFT (dir=-1 selects the exp(-2πi·k/N) twiddles per
    // dsp_helpers.hpp's fillFftTwiddles convention).
    fftRadix2(mr, buf, -1);

    // Zero negative frequencies, double positive (excluding DC and Nyquist).
    for (size_t i = 1; i < fftLen / 2; ++i)
        buf[i] *= 2.0;
    for (size_t i = fftLen / 2 + 1; i < fftLen; ++i)
        buf[i] = Complex(0.0, 0.0);

    // IFFT via conjugate trick: ifft(X) = conj(fft(conj(X)))/N.
    for (auto &v : buf)
        v = std::conj(v);
    fftRadix2(mr, buf, -1);
    const double invN = 1.0 / static_cast<double>(fftLen);
    for (auto &v : buf)
        v = std::conj(v) * invN;

    return buf;
}

} // anonymous namespace

Value hilbert(const Value &x, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    ScratchArena scratch(mr);
    auto buf = hilbertBuf(x, &scratch);
    return packComplexResult(buf.data(), N, mr);
}

// ── Envelope modes (matches MATLAB R2025b envelope.m) ────────────────
namespace {

// Read x into a flat scratch buffer.
ScratchVec<double> read_real(const Value &x, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    ScratchVec<double> v(N, mr);
    for (size_t i = 0; i < N; ++i) v[i] = x.elemAsDouble(i);
    return v;
}

// |hilbert(x)| amplitude — used by the default and 'analytic' (no n) paths.
ScratchVec<double> ampl_fft_hilbert(const Value &x, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    auto buf = hilbertBuf(x, mr);
    ScratchVec<double> a(N, mr);
    for (size_t i = 0; i < N; ++i) a[i] = std::abs(buf[i]);
    return a;
}

// envFIR — windowed (n-tap) ideal-Hilbert FIR filter, Kaiser-tapered,
// applied via 'same' mode complex convolution. MATLAB R2025b algorithm:
//   t = ((1-n)/2 : (n-1)/2) / 2
//   hfilt = sinc(t) .* exp(i·π·t)
//   firFilter = hfilt .* kaiser(n, 8)
//   firFilter /= sum(real(firFilter))
//   y = abs(conv2(x, firFilter, 'same'))
ScratchVec<double> ampl_fir(const Value &x_centered, size_t n, std::pmr::memory_resource *mr)
{
    const size_t N = x_centered.numel();
    auto x = read_real(x_centered, mr);

    // Kaiser window via numkit::signal::kaiser (returns Value 1×n).
    Value kw = numkit::signal::kaiser(n, 8.0, mr);
    const double *kd = kw.doubleData();

    // hfilt[k] = sinc(t_k) · exp(iπ·t_k), windowed.
    ScratchVec<Complex> filt(n, mr);
    Complex normR(0.0, 0.0);
    for (size_t k = 0; k < n; ++k) {
        const double t = 0.5 * (double(k) - 0.5 * (double(n) - 1.0));
        const double s = (std::fabs(t) < 1e-15)
                         ? 1.0
                         : std::sin(M_PI * t) / (M_PI * t);
        const double w = kd[k];
        const Complex h = Complex(s * w * std::cos(M_PI * t),
                                  s * w * std::sin(M_PI * t));
        filt[k] = h;
        normR += Complex(h.real(), 0.0);
    }
    const double inv_norm = 1.0 / normR.real();
    for (auto &c : filt) c *= inv_norm;

    // 'same' complex conv of length-N real signal with length-n complex
    // filter. Output length = N. MATLAB's conv2 'same' centers using
    // floor(n/2) for the leading offset (verified vs R2025b on n=8).
    const size_t pad = n / 2;
    ScratchVec<double> out(N, mr);
    for (size_t i = 0; i < N; ++i) {
        Complex acc(0.0, 0.0);
        // y[i] = sum_k x[i+pad-k] · filt[k] (matching conv 'same').
        for (size_t k = 0; k < n; ++k) {
            const long long src = (long long)i + (long long)pad
                                  - (long long)k;
            if (src >= 0 && (size_t)src < N)
                acc += filt[k] * x[(size_t)src];
        }
        out[i] = std::abs(acc);
    }
    return out;
}

// envRMS — sliding-window RMS. y = sqrt(movmean(x², n)).
ScratchVec<double> ampl_rms(const Value &x_centered, size_t n, std::pmr::memory_resource *mr)
{
    const size_t N = x_centered.numel();
    auto x = read_real(x_centered, mr);

    // movmean with 'centered' window of length n. MATLAB's movmean by
    // default uses a centered window: for odd n, [-(n-1)/2 .. (n-1)/2];
    // for even n, [-n/2 .. n/2-1] (asymmetric). Edge handling: shrink
    // window to available samples.
    const long long left  = (long long)(n / 2);
    const long long right = (long long)(n - 1) - left;  // for even n: left = n/2, right = n/2-1
    ScratchVec<double> out(N, mr);
    for (size_t i = 0; i < N; ++i) {
        const long long lo = std::max((long long)0, (long long)i - left);
        const long long hi = std::min((long long)N - 1, (long long)i + right);
        double s = 0.0;
        long long cnt = 0;
        for (long long j = lo; j <= hi; ++j) { s += x[(size_t)j] * x[(size_t)j]; ++cnt; }
        out[i] = std::sqrt(s / double(cnt));
    }
    return out;
}

// findpeaks with MinPeakDistance ≥ n. Greedy: sort indices by value
// descending, accept peak if no already-accepted peak is within n samples.
// Returns indices (0-based) in ascending order.
ScratchVec<size_t> findpeaks_mindist(const ScratchVec<double> &x, size_t n, std::pmr::memory_resource *mr)
{
    const size_t N = x.size();
    ScratchVec<size_t> peaks(mr);
    if (N < 3) return peaks;
    // Strict local maxima.
    ScratchVec<size_t> cand(mr);
    for (size_t i = 1; i + 1 < N; ++i)
        if (x[i] > x[i - 1] && x[i] > x[i + 1]) cand.push_back(i);
    if (cand.empty()) return peaks;
    if (n <= 1) {
        peaks.assign(cand.begin(), cand.end());
        return peaks;
    }
    // Sort by value descending.
    ScratchVec<size_t> order(cand.begin(), cand.end(), mr);
    std::sort(order.begin(), order.end(),
              [&](size_t a, size_t b) { return x[a] > x[b]; });
    ScratchVec<uint8_t> accepted(N, 0, mr);
    for (size_t idx : order) {
        bool ok = true;
        const long long lo = (long long)idx - (long long)n;
        const long long hi = (long long)idx + (long long)n;
        for (long long j = std::max((long long)0, lo);
             j <= std::min((long long)N - 1, hi); ++j)
            if (accepted[(size_t)j]) { ok = false; break; }
        if (ok) accepted[idx] = 1;
    }
    for (size_t i = 0; i < N; ++i) if (accepted[i]) peaks.push_back(i);
    return peaks;
}

// envPeak — spline through local maxima/minima with MinPeakDistance n.
// MATLAB convention: if <2 peaks found, fallback to including endpoints.
// No DC removal in peak mode.
void env_peak(const Value &x, size_t n, ScratchVec<double> &up, ScratchVec<double> &lo, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    auto v = read_real(x, mr);
    ScratchVec<double> neg(N, mr);
    for (size_t i = 0; i < N; ++i) neg[i] = -v[i];

    auto build_envelope = [&](const ScratchVec<double> &sig,
                              ScratchVec<double> &out) {
        ScratchVec<size_t> peaks(mr);
        if (N > n + 1)
            peaks = findpeaks_mindist(sig, n, mr);
        // If <2 peaks, fall back to endpoints + any peaks.
        ScratchVec<size_t> locs(mr);
        if (peaks.size() < 2) {
            ScratchVec<size_t> draft(mr);
            draft.push_back(0);
            for (size_t p : peaks) draft.push_back(p);
            draft.push_back(N - 1);
            // De-dup if endpoints coincide with peaks.
            for (size_t l : draft) {
                if (locs.empty() || locs.back() != l) locs.push_back(l);
            }
        } else {
            locs.assign(peaks.begin(), peaks.end());
        }
        // MATLAB's `spline` fits a PARABOLA when given exactly 3
        // knots (not-a-knot is degenerate). numkit's general spline
        // falls back to natural BCs which doesn't match — handle the
        // 3-point case inline.
        out.resize(N);
        if (locs.size() == 3) {
            const double x1 = double(locs[0] + 1);
            const double x2 = double(locs[1] + 1);
            const double x3 = double(locs[2] + 1);
            const double y1 = sig[locs[0]];
            const double y2 = sig[locs[1]];
            const double y3 = sig[locs[2]];
            // Solve y = a·x² + b·x + c via Lagrange-style determinants.
            const double det = (x1 - x2) * (x1 - x3) * (x2 - x3);
            const double a = (y1 * (x2 - x3) - y2 * (x1 - x3) + y3 * (x1 - x2))
                             / det;
            const double b = (y1 * (x3 * x3 - x2 * x2)
                              - y2 * (x3 * x3 - x1 * x1)
                              + y3 * (x2 * x2 - x1 * x1)) / det;
            const double c = (y1 * (x2 * x2 * x3 - x2 * x3 * x3)
                              - y2 * (x1 * x1 * x3 - x1 * x3 * x3)
                              + y3 * (x1 * x1 * x2 - x1 * x2 * x2)) / det;
            for (size_t i = 0; i < N; ++i) {
                const double xi = double(i + 1);
                out[i] = a * xi * xi + b * xi + c;
            }
            return;
        }
        // interp1 with 'spline' for 2 (linear) or 4+ (not-a-knot).
        Value x_loc = Value::matrix(locs.size(), 1, ValueType::DOUBLE, mr);
        Value y_loc = Value::matrix(locs.size(), 1, ValueType::DOUBLE, mr);
        Value xq    = Value::matrix(N, 1, ValueType::DOUBLE, mr);
        double *xd = x_loc.doubleDataMut();
        double *yd = y_loc.doubleDataMut();
        double *qd = xq.doubleDataMut();
        for (size_t i = 0; i < locs.size(); ++i) {
            xd[i] = double(locs[i] + 1);
            yd[i] = sig[locs[i]];
        }
        for (size_t i = 0; i < N; ++i) qd[i] = double(i + 1);
        Value yi = numkit::builtin::interp1(x_loc, y_loc, xq, "spline", mr);
        for (size_t i = 0; i < N; ++i) out[i] = yi.elemAsDouble(i);
    };
    build_envelope(v, up);
    build_envelope(neg, lo);
    for (auto &v_ : lo) v_ = -v_;
}

} // anonymous

Value envelope(const Value &x, std::pmr::memory_resource *mr)
{
    // Default: mean + |hilbert(x - mean)|.
    const size_t N = x.numel();
    ScratchArena scratch(mr);
    auto v = read_real(x, &scratch);
    double xmean = 0.0;
    for (double d : v) xmean += d;
    xmean /= double(N);
    Value xc = Value::matrix(N, 1, ValueType::DOUBLE, &scratch);
    {
        double *cd = xc.doubleDataMut();
        for (size_t i = 0; i < N; ++i) cd[i] = v[i] - xmean;
    }
    auto a = ampl_fft_hilbert(xc, &scratch);
    auto r = createLike(x, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < N; ++i) r.doubleDataMut()[i] = xmean + a[i];
    return r;
}

// Full multi-mode envelope: returns (yupper, ylower).
//   mode = 0 default (n unused — uses FFT |hilbert(x-mean)|)
//   mode = 1 'analytic' (n-tap Kaiser-Hilbert FIR)
//   mode = 2 'rms'      (sliding RMS over n samples)
//   mode = 3 'peak'     (spline through extrema with MinPeakDistance n)
std::pair<Value, Value>
envelope_full(const Value &x, int mode, size_t n, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    ScratchArena scratch(mr);

    if (mode == 3) {
        // Peak mode — no DC removal.
        ScratchVec<double> up(&scratch), lo(&scratch);
        env_peak(x, n, up, lo, &scratch);
        Value yupper = createLike(x, ValueType::DOUBLE, mr);
        Value ylower = createLike(x, ValueType::DOUBLE, mr);
        for (size_t i = 0; i < N; ++i) yupper.doubleDataMut()[i] = up[i];
        for (size_t i = 0; i < N; ++i) ylower.doubleDataMut()[i] = lo[i];
        return {std::move(yupper), std::move(ylower)};
    }

    // Modes 0/1/2: remove DC, compute amplitude, restore.
    auto v = read_real(x, &scratch);
    double xmean = 0.0;
    for (double d : v) xmean += d;
    xmean /= double(N);
    Value xc = Value::matrix(N, 1, ValueType::DOUBLE, &scratch);
    {
        double *cd = xc.doubleDataMut();
        for (size_t i = 0; i < N; ++i) cd[i] = v[i] - xmean;
    }

    ScratchVec<double> a(&scratch);
    if      (mode == 0) a = ampl_fft_hilbert(xc, &scratch);
    else if (mode == 1) a = ampl_fir(xc, n, &scratch);
    else                a = ampl_rms(xc, n, &scratch);

    Value yupper = createLike(x, ValueType::DOUBLE, mr);
    Value ylower = createLike(x, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < N; ++i) yupper.doubleDataMut()[i] = xmean + a[i];
    for (size_t i = 0; i < N; ++i) ylower.doubleDataMut()[i] = xmean - a[i];
    return {std::move(yupper), std::move(ylower)};
}

// Back-compat 2-output adapter: dispatches to default mode.
std::pair<Value, Value>
envelope_pair(const Value &x, std::pmr::memory_resource *mr)
{
    return envelope_full(x, 0, 0, mr);
}

} // namespace numkit::signal
