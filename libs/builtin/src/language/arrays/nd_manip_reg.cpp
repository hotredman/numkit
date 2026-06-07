// libs/signal/src/language/arrays/nd_manip_reg.cpp
//
// CallContext register half of language/arrays/nd_manip.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/language/arrays/matrix.hpp>  // reshape, horzcat, vertcat
#include <numkit/builtin/language/arrays/nd_manip.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/shape_ops.hpp>      // computeStridesColMajor, incrementCoords
#include <numkit/value/value.hpp>
#include "helpers.hpp"
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

namespace numkit::builtin {

namespace detail {

namespace {

ScratchVec<int> permFromValue(const Value &v, std::pmr::memory_resource *mr)
{
    ScratchVec<int> p(mr);
    p.reserve(v.numel());
    for (size_t i = 0; i < v.numel(); ++i)
        p.push_back(static_cast<int>(v.doubleData()[i]));
    return p;
}

} // namespace

void permute_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("permute: requires (A, perm)",
                     0, 0, "permute", "", "numkit:permute:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto perm = permFromValue(args[1], &scratch);
    outs[0] = permute(args[0], Span<const int>(perm.data(), perm.size()), mr);
}

void ipermute_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ipermute: requires (A, perm)",
                     0, 0, "ipermute", "", "numkit:ipermute:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto perm = permFromValue(args[1], &scratch);
    outs[0] = ipermute(args[0], Span<const int>(perm.data(), perm.size()), mr);
}

void squeeze_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.empty())
        throw Error("squeeze: requires 1 argument",
                     0, 0, "squeeze", "", "numkit:squeeze:nargin");
    outs[0] = squeeze(args[0], ctx.engine->resource());
}

void cat_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("cat: requires (dim, A, ...)",
                     0, 0, "cat", "", "numkit:cat:nargin");
    const int dim = static_cast<int>(args[0].toScalar());
    // Pass &args[1] as the start of the values array.
    outs[0] = cat(dim, args.subspan(1), ctx.engine->resource());
}

void blkdiag_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    outs[0] = blkdiag(args, ctx.engine->resource());
}

void shiftdim_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.empty())
        throw Error("shiftdim: requires 1 or 2 arguments",
                     0, 0, "shiftdim", "", "numkit:shiftdim:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const int n = static_cast<int>(args[1].toScalar());
        outs[0] = shiftdim(args[0], n, mr);
        return;
    }
    // Auto form: [B, k] = shiftdim(A).
    auto res = shiftdimAuto(args[0], mr);
    outs[0] = std::move(res.v);
    if (nargout > 1)
        outs[1] = Value::scalar(static_cast<double>(res.dropped), mr);
}

} // namespace detail

} // namespace numkit::builtin
