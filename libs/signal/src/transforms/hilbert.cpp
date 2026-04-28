// libs/signal/src/transforms/hilbert.cpp
//
// FFT-based Hilbert transform + envelope. unwrap moved to
// filter_analysis/unwrap.cpp.

#include <numkit/signal/transforms/hilbert.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "../dsp_helpers.hpp"  // Complex, FFT helpers
#include "helpers.hpp"         // createLike

#include <cmath>
#include <complex>
#include <vector>

namespace numkit::signal {

namespace {

// Shared FFT-based Hilbert transform kernel used by both hilbert() and
// envelope(). Returns a buffer of length fftLen holding the analytic signal
// (possibly zero-padded beyond N). Caller slices to the first N samples.
std::vector<Complex> hilbertBuf(const Value &x)
{
    const size_t N = x.numel();
    const size_t fftLen = nextPow2(N);

    auto buf = prepareFFTBuffer(x, N, fftLen);
    fftRadix2(buf, 1);

    // Zero negative frequencies, double positive (excluding DC and Nyquist).
    for (size_t i = 1; i < fftLen / 2; ++i)
        buf[i] *= 2.0;
    for (size_t i = fftLen / 2 + 1; i < fftLen; ++i)
        buf[i] = Complex(0.0, 0.0);

    // IFFT via conjugate trick
    for (auto &v : buf)
        v = std::conj(v);
    fftRadix2(buf, 1);
    const double invN = 1.0 / static_cast<double>(fftLen);
    for (auto &v : buf)
        v = std::conj(v) * invN;

    return buf;
}

} // anonymous namespace

Value hilbert(Allocator &alloc, const Value &x)
{
    const size_t N = x.numel();
    auto buf = hilbertBuf(x);
    return packComplexResult(buf, N, &alloc);
}

Value envelope(Allocator &alloc, const Value &x)
{
    const size_t N = x.numel();
    auto buf = hilbertBuf(x);

    auto r = createLike(x, ValueType::DOUBLE, &alloc);
    for (size_t i = 0; i < N; ++i)
        r.doubleDataMut()[i] = std::abs(buf[i]);
    return r;
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void hilbert_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("hilbert: requires 1 argument",
                     0, 0, "hilbert", "", "m:hilbert:nargin");
    outs[0] = hilbert(ctx.engine->allocator(), args[0]);
}

void envelope_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("envelope: requires 1 argument",
                     0, 0, "envelope", "", "m:envelope:nargin");
    outs[0] = envelope(ctx.engine->allocator(), args[0]);
}

} // namespace detail

} // namespace numkit::signal
