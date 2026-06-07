// libs/linalg/src/matrix_functions_reg.cpp
//
// Register half of the matrix-function builtins: the CallContext wrappers
// expm / logm / sqrtm / expmv that delegate to the engine-free compute in
// matrix_functions.cpp. library.cpp forward-declares + registers these by
// name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/linalg/matrix_functions.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

namespace numkit::linalg {
namespace detail {

void expm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("expm: requires exactly 1 argument",
                    0, 0, "expm", "", "numkit:expm:nargin");
    outs[0] = expm(args[0], ctx.engine->resource());
}

void logm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("logm: requires exactly 1 argument",
                    0, 0, "logm", "", "numkit:logm:nargin");
    outs[0] = logm_sym(args[0], ctx.engine->resource());
}

void sqrtm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("sqrtm: requires exactly 1 argument",
                    0, 0, "sqrtm", "", "numkit:sqrtm:nargin");
    outs[0] = sqrtm_sym(args[0], ctx.engine->resource());
}

// MATLAB signature: w = expmv(t, A, v) — three positional args, t first.
// Some flavours (e.g. expmv from Higham's package) use (A, v) and an
// optional t; we mirror MATLAB's documented order with t leading.
void expmv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 3)
        throw Error("expmv: requires (t, A, v)",
                    0, 0, "expmv", "", "numkit:expmv:nargin");
    const double t = args[0].toScalar();
    outs[0] = expmv(t, args[1], args[2], ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::linalg
