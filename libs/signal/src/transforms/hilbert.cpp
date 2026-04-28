// libs/signal/src/transforms/hilbert.cpp
//
// FFT-based Hilbert transform + envelope. unwrap moved to
// filter_analysis/unwrap.cpp.

#include <numkit/m/signal/transforms/hilbert.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "../MDspHelpers.hpp"
#include "MStdHelpers.hpp"

#include <cmath>
#include <complex>
#include <vector>

namespace numkit::m::signal {

namespace {

// Shared FFT-based Hilbert transform kernel used by both hilbert() and
// envelope(). Returns a buffer of length fftLen holding the analytic signal
// (possibly zero-padded beyond N). Caller slices to the first N samples.
std::vector<Complex> hilbertBuf(const MValue &x)
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

MValue hilbert(Allocator &alloc, const MValue &x)
{
    const size_t N = x.numel();
    auto buf = hilbertBuf(x);
    return packComplexResult(buf, N, &alloc);
}

MValue envelope(Allocator &alloc, const MValue &x)
{
    const size_t N = x.numel();
    auto buf = hilbertBuf(x);

    auto r = createLike(x, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < N; ++i)
        r.doubleDataMut()[i] = std::abs(buf[i]);
    return r;
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void hilbert_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("hilbert: requires 1 argument",
                     0, 0, "hilbert", "", "m:hilbert:nargin");
    outs[0] = hilbert(ctx.engine->allocator(), args[0]);
}

void envelope_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("envelope: requires 1 argument",
                     0, 0, "envelope", "", "m:envelope:nargin");
    outs[0] = envelope(ctx.engine->allocator(), args[0]);
}

} // namespace detail

} // namespace numkit::m::signal
