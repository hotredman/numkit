// libs/signal/src/transforms/hilbert.cpp
//
// FFT-based Hilbert transform + envelope. unwrap moved to
// filter_analysis/unwrap.cpp.

#include <numkit/signal/transforms/hilbert.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "../dsp_helpers.hpp"  // Complex, FFT helpers
#include "helpers.hpp"         // createLike

#include <cmath>
#include <complex>
#include <memory_resource>

namespace numkit::signal {

namespace {

// Shared FFT-based Hilbert transform kernel used by both hilbert() and
// envelope(). Returns a buffer of length fftLen holding the analytic
// signal (possibly zero-padded beyond N). Caller slices to the first N
// samples. Backed by the caller's scratch arena.
ScratchVec<Complex> hilbertBuf(std::pmr::memory_resource *mr, const Value &x)
{
    const size_t N = x.numel();
    const size_t fftLen = nextPow2(N);

    auto buf = prepareFFTBuffer(mr, x, N, fftLen);
    fftRadix2(mr, buf, 1);

    // Zero negative frequencies, double positive (excluding DC and Nyquist).
    for (size_t i = 1; i < fftLen / 2; ++i)
        buf[i] *= 2.0;
    for (size_t i = fftLen / 2 + 1; i < fftLen; ++i)
        buf[i] = Complex(0.0, 0.0);

    // IFFT via conjugate trick
    for (auto &v : buf)
        v = std::conj(v);
    fftRadix2(mr, buf, 1);
    const double invN = 1.0 / static_cast<double>(fftLen);
    for (auto &v : buf)
        v = std::conj(v) * invN;

    // numkit's fftRadix2(dir=+1) uses MATLAB's IFFT sign convention
    // (W[k] = exp(+2πi·k/N) instead of exp(-2πi·k/N)). With "positive
    // frequencies" doubled at indices [1, N/2-1], the output's
    // imaginary part comes out with the wrong sign (real part is
    // unaffected because real(x) is invariant under conjugation).
    // The fix: conjugate the output to flip the imaginary sign.
    for (auto &v : buf)
        v = std::conj(v);

    return buf;
}

} // anonymous namespace

Value hilbert(std::pmr::memory_resource *mr, const Value &x)
{
    const size_t N = x.numel();
    ScratchArena scratch(mr);
    auto buf = hilbertBuf(&scratch, x);
    return packComplexResult(buf.data(), N, mr);
}

Value envelope(std::pmr::memory_resource *mr, const Value &x)
{
    const size_t N = x.numel();
    ScratchArena scratch(mr);
    auto buf = hilbertBuf(&scratch, x);

    auto r = createLike(x, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < N; ++i)
        r.doubleDataMut()[i] = std::abs(buf[i]);
    return r;
}

// 2-output form: returns symmetric (yupper, ylower=-yupper) for the
// analytic-signal envelope. NB MATLAB's `envelope(x)` default uses
// SPLINE-PEAK interpolation (asymmetric output) — that mode is
// DEFERRED. Only the analytic mode + symmetric -upper lower is
// produced here.
void envelope_pair(std::pmr::memory_resource *mr, const Value &x,
                   Value *yupper, Value *ylower)
{
    const size_t N = x.numel();
    ScratchArena scratch(mr);
    auto buf = hilbertBuf(&scratch, x);
    auto up = createLike(x, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < N; ++i)
        up.doubleDataMut()[i] = std::abs(buf[i]);
    if (yupper) *yupper = up;
    if (ylower) {
        auto lo = createLike(x, ValueType::DOUBLE, mr);
        for (size_t i = 0; i < N; ++i)
            lo.doubleDataMut()[i] = -up.doubleData()[i];
        *ylower = std::move(lo);
    }
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void hilbert_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("hilbert: requires 1 argument",
                     0, 0, "hilbert", "", "m:hilbert:nargin");
    outs[0] = hilbert(ctx.engine->resource(), args[0]);
}

void envelope_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("envelope: requires 1 argument",
                     0, 0, "envelope", "", "m:envelope:nargin");
    // Numeric 2nd / 3rd args (filter length, np) and string mode
    // ('analytic' / 'rms' / 'peak') are NOT YET SUPPORTED. Currently
    // only the FFT-based analytic-signal envelope is computed.
    // MATLAB's default form uses spline-peak interpolation which gives
    // ASYMMETRIC upper/lower envelopes — numkit returns lower = -upper
    // (symmetric). Document this gap clearly when extra args appear.
    if (args.size() > 1) {
        // Tolerate but warn (clear error rather than silent divergence).
        throw Error("envelope: filter-length / mode arguments are not "
                    "yet supported (only FFT analytic-signal envelope)",
                     0, 0, "envelope", "", "m:envelope:nyi");
    }
    auto *mr = ctx.engine->resource();
    Value up, lo;
    envelope_pair(mr, args[0], &up, &lo);
    outs[0] = std::move(up);
    if (nargout > 1) outs[1] = std::move(lo);
}

} // namespace detail

} // namespace numkit::signal
