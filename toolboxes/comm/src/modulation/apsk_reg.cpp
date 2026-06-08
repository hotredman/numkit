// toolboxes/comm/src/modulation/apsk_reg.cpp
//
// Register half of the comm APSK builtins: the CallContext wrappers
// apskmod / apskdemod that marshal the per-ring M / radii Values into
// scratch Span buffers and delegate to the engine-free compute in apsk.cpp.
// library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/comm/modulation/apsk.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <cstddef>

namespace numkit::comm {
namespace detail {

namespace {

// Build a ScratchVec<size_t> from a Value's elements via elemAsDouble.
ScratchVec<size_t> valueToScratchSizes(const Value &v, std::pmr::memory_resource *mr)
{
    ScratchVec<size_t> out(mr);
    const size_t n = v.numel();
    out.reserve(n);
    for (size_t i = 0; i < n; ++i)
        out.push_back(static_cast<size_t>(v.elemAsDouble(i)));
    return out;
}

ScratchVec<double> valueToScratchDoubles(const Value &v, std::pmr::memory_resource *mr)
{
    ScratchVec<double> out(mr);
    const size_t n = v.numel();
    out.reserve(n);
    for (size_t i = 0; i < n; ++i)
        out.push_back(v.elemAsDouble(i));
    return out;
}

} // namespace

void apskmod_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("apskmod: requires (x, M, radii [, phaseoffset [, mapping]])",
                    0, 0, "apskmod", "", "numkit:apskmod:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto Mv = valueToScratchSizes(args[1], &scratch);
    auto Rv = valueToScratchDoubles(args[2], &scratch);
    const Value &po = (args.size() > 3) ? args[3] : Value::Empty;
    const Value &mp = (args.size() > 4) ? args[4] : Value::Empty;
    outs[0] = apskmod(args[0],
                      Span<const size_t>(Mv.data(), Mv.size()),
                      Span<const double>(Rv.data(), Rv.size()),
                      po, mp, mr);
}

void apskdemod_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("apskdemod: requires (y, M, radii [, phaseoffset [, mapping]])",
                    0, 0, "apskdemod", "", "numkit:apskdemod:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto Mv = valueToScratchSizes(args[1], &scratch);
    auto Rv = valueToScratchDoubles(args[2], &scratch);
    const Value &po = (args.size() > 3) ? args[3] : Value::Empty;
    const Value &mp = (args.size() > 4) ? args[4] : Value::Empty;
    outs[0] = apskdemod(args[0],
                        Span<const size_t>(Mv.data(), Mv.size()),
                        Span<const double>(Rv.data(), Rv.size()),
                        po, mp, mr);
}

} // namespace detail
} // namespace numkit::comm
