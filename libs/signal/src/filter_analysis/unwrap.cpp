// libs/signal/src/filter_analysis/unwrap.cpp
//
// unwrap — split out from MDspTransform.cpp. The hilbert/envelope
// pair lives in transforms/hilbert.cpp.

#include <numkit/m/signal/filter_analysis/unwrap.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::m::signal {

MValue unwrap(Allocator &alloc, const MValue &phase)
{
    const size_t n = phase.numel();
    const double *p = phase.doubleData();

    auto r = createLike(phase, MType::DOUBLE, &alloc);
    double *out = r.doubleDataMut();
    if (n == 0)
        return r;
    out[0] = p[0];
    for (size_t i = 1; i < n; ++i) {
        double d = p[i] - p[i - 1];
        d = d - 2.0 * M_PI * std::round(d / (2.0 * M_PI));
        out[i] = out[i - 1] + d;
    }
    return r;
}

// ── Engine adapter ────────────────────────────────────────────────────
namespace detail {

void unwrap_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("unwrap: requires 1 argument",
                     0, 0, "unwrap", "", "m:unwrap:nargin");
    outs[0] = unwrap(ctx.engine->allocator(), args[0]);
}

} // namespace detail

} // namespace numkit::m::signal
