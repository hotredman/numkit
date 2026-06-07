// libs/linalg/src/solvers_reg.cpp
//
// Register half of the linear-solver builtins: the CallContext wrappers
// linsolve / lsqminnorm / lsqnonneg that delegate to the engine-free
// compute in solvers.cpp. lsqnonneg's multi-output forms destructure the
// NnlsResult returned by lsqnonneg_impl. library.cpp forward-declares +
// registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/linalg/solvers.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <string>
#include <utility>

namespace numkit::linalg {
namespace detail {

void linsolve_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args.size() > 3)
        throw Error("linsolve: requires (A, B[, opts])",
                    0, 0, "linsolve", "", "numkit:linsolve:nargin");
    // 3rd arg (opts struct) accepted for MATLAB-compat but ignored.
    outs[0] = linsolve(args[0], args[1], ctx.engine->resource());
}

void lsqminnorm_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("lsqminnorm: requires (A, B [, tol])",
                    0, 0, "lsqminnorm", "", "numkit:lsqminnorm:nargin");
    bool have_tol = (args.size() >= 3);
    double tol = have_tol ? args[2].toScalar() : 0.0;
    outs[0] = lsqminnorm(args[0], args[1], have_tol, tol, ctx.engine->resource());
}

void lsqnonneg_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("lsqnonneg: requires (C, d)",
                    0, 0, "lsqnonneg", "", "numkit:lsqnonneg:nargin");
    auto R = lsqnonneg_impl(args[0], args[1], ctx.engine->resource());
    outs[0] = R.x;
    if (nargout >= 2 && outs.size() >= 2)
        outs[1] = Value::scalar(R.resnorm, ctx.engine->resource());
    if (nargout >= 3 && outs.size() >= 3)
        outs[2] = R.residual;
    if (nargout >= 4 && outs.size() >= 4)
        outs[3] = Value::scalar(static_cast<double>(R.exitflag),
                                ctx.engine->resource());
    if (nargout >= 5 && outs.size() >= 5) {
        Value out_struct = Value::structure(ctx.engine->resource());
        out_struct.structFields()["iterations"] =
            Value::scalar(static_cast<double>(R.iterations),
                          ctx.engine->resource());
        out_struct.structFields()["algorithm"] =
            Value::fromString(R.algorithm, ctx.engine->resource());
        out_struct.structFields()["message"] =
            Value::fromString(R.message, ctx.engine->resource());
        outs[4] = std::move(out_struct);
    }
}

} // namespace detail
} // namespace numkit::linalg
