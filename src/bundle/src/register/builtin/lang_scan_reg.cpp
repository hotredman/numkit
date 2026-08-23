// toolboxes/signal/src/language/strings/scan_reg.cpp
//
// CallContext register half of language/strings/scan.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/runtime/io.hpp>
#include <numkit/builtin/strfun.hpp>
#include <numkit/builtin/iofun.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include <numkit/ops/io_helpers.hpp>
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

namespace numkit::builtin {
using namespace numkit::builtin;

namespace detail {

void fscanf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    ::numkit::runtime::fscanf(*ctx.engine, args, nargout, outs);
}

void sscanf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    sscanf(args, nargout, outs, ctx.engine->resource());
}

void textscan_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    ::numkit::runtime::textscan(*ctx.engine, args, nargout, outs);
}

} // namespace detail

} // namespace numkit::builtin
