// libs/signal/src/transforms/transform_helpers_reg.cpp
//
// CallContext register half of transforms/transform_helpers.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/signal/transforms/transform_helpers.hpp>
#include <numkit/value/error.hpp>
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

void nextpow2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("nextpow2: requires 1 argument",
                     0, 0, "nextpow2", "", "numkit:nextpow2:nargin");
    outs[0] = nextpow2(args[0], ctx.engine->resource());
}

void fftshift_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("fftshift: requires 1 argument",
                     0, 0, "fftshift", "", "numkit:fftshift:nargin");
    if (args.size() >= 2) {
        const int dim = static_cast<int>(args[1].toScalar());
        if (dim < 1 || dim > 3)
            throw Error("fftshift: dim must be 1, 2, or 3",
                         0, 0, "fftshift", "", "numkit:fftshift:dim");
        outs[0] = fftshift(args[0], dim, ctx.engine->resource());
    } else {
        outs[0] = fftshift(args[0], ctx.engine->resource());
    }
}

void ifftshift_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ifftshift: requires 1 argument",
                     0, 0, "ifftshift", "", "numkit:ifftshift:nargin");
    if (args.size() >= 2) {
        const int dim = static_cast<int>(args[1].toScalar());
        if (dim < 1 || dim > 3)
            throw Error("ifftshift: dim must be 1, 2, or 3",
                         0, 0, "ifftshift", "", "numkit:ifftshift:dim");
        outs[0] = ifftshift(args[0], dim, ctx.engine->resource());
    } else {
        outs[0] = ifftshift(args[0], ctx.engine->resource());
    }
}

} // namespace detail

} // namespace numkit::signal
