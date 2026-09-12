// toolboxes/signal/src/language/strings/format_reg.cpp
//
// CallContext register half of language/strings/format.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <nk/core/engine.hpp>
#include <nk/builtin/strfun.hpp>
#include <nk/builtin/iofun.hpp>
#include <nk/value/error.hpp>
#include <nk/value/scratch.hpp>
#include <nk/value/value.hpp>
#include <nk/value/error.hpp>
#include <nk/value/scratch.hpp>
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
using namespace nk::builtin;  // C4c localized (umbrella removed)

namespace detail {

void sprintf_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.empty()) {
        outs[0] = Value::fromString("", mr);
        return;
    }
    Span<const Value> rest{args.data() + 1, args.size() - 1};
    outs[0] = sprintf(args[0], rest, mr);
}

} // namespace detail

} // namespace nk::builtin
