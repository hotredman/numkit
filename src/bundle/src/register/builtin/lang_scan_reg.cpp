// toolboxes/signal/src/language/strings/scan_reg.cpp
//
// CallContext register half of language/strings/scan.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <nk/core/engine.hpp>
#include <nk/runtime/io.hpp>
#include <nk/builtin/strfun.hpp>
#include <nk/builtin/iofun.hpp>
#include <nk/value/error.hpp>
#include <nk/value/scratch.hpp>
#include <nk/value/value.hpp>
#include <nk/ops/io_helpers.hpp>
#include <nk/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace nk::builtin {
using namespace nk::builtin;

namespace detail {

void fscanf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    ::nk::runtime::fscanf(*ctx.engine, args, nargout, outs);
}

void sscanf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    sscanf(args, nargout, outs, ctx.engine->resource());
}

void textscan_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    ::nk::runtime::textscan(*ctx.engine, args, nargout, outs);
}

} // namespace detail

} // namespace nk::builtin
