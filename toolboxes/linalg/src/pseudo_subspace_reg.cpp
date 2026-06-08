// toolboxes/linalg/src/pseudo_subspace_reg.cpp
//
// Register half of the pseudoinverse / subspace builtins: the CallContext
// wrappers pinv / orth / null / subspace that delegate to the engine-free
// compute in pseudo_subspace.cpp. library.cpp forward-declares + registers
// these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/linalg/pseudo_subspace.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

namespace numkit::linalg {
namespace detail {

void pinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1 || args.size() > 2)
        throw Error("pinv: requires (A) or (A, tol)",
                    0, 0, "pinv", "", "numkit:pinv:nargin");
    const double tol = (args.size() >= 2) ? args[1].toScalar() : -1.0;
    outs[0] = pinv(args[0], tol, ctx.engine->resource());
}

void orth_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1 || args.size() > 2)
        throw Error("orth: requires (A) or (A, tol)",
                    0, 0, "orth", "", "numkit:orth:nargin");
    const double tol = (args.size() >= 2) ? args[1].toScalar() : -1.0;
    outs[0] = orth(args[0], tol, ctx.engine->resource());
}

void null_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1 || args.size() > 2)
        throw Error("null: requires (A) or (A, tol)",
                    0, 0, "null", "", "numkit:null:nargin");
    const double tol = (args.size() >= 2) ? args[1].toScalar() : -1.0;
    outs[0] = null_basis(args[0], tol, ctx.engine->resource());
}

void subspace_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 2)
        throw Error("subspace: requires (A, B)",
                    0, 0, "subspace", "", "numkit:subspace:nargin");
    outs[0] = subspace(args[0], args[1], ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::linalg
