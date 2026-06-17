// toolboxes/linalg/src/vector_ops_reg.cpp
//
// Register half of the vector builtins: the CallContext wrappers cross /
// dot / kron that delegate to the engine-free compute in vector_ops.cpp.
// library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/linalg/vector_ops.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

namespace numkit::linalg {
namespace detail {

void cross_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("cross: requires 2 arguments",
                     0, 0, "cross", "", "numkit:cross:nargin");
    // cross(A, B, dim): cross along the given dimension (default: first
    // dimension of length 3).
    const int dim = (args.size() >= 3 && !args[2].isEmpty())
                        ? static_cast<int>(args[2].toScalar()) : 0;
    outs[0] = numkit::narrowComplex(cross(args[0], args[1], dim, ctx.engine->resource()),
                                    ctx.engine->resource());
}

void dot_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("dot: requires 2 arguments",
                     0, 0, "dot", "", "numkit:dot:nargin");
    // dot(A, B, dim): reduce along the given dimension (default: vector ->
    // scalar, matrix -> per-column).
    const int dim = (args.size() >= 3 && !args[2].isEmpty())
                        ? static_cast<int>(args[2].toScalar()) : 0;
    outs[0] = numkit::narrowComplex(dot(args[0], args[1], dim, ctx.engine->resource()),
                                    ctx.engine->resource());
}

void kron_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("kron: requires 2 arguments",
                     0, 0, "kron", "", "numkit:kron:nargin");
    outs[0] = numkit::narrowComplex(kron(args[0], args[1], ctx.engine->resource()),
                                    ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::linalg
