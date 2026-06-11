// toolboxes/signal/src/regress/regress_reg.cpp
//
// CallContext register half of regress/regress.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/stats/distributions/fisher_f.hpp>
#include <numkit/stats/distributions/students_t.hpp>
#include <numkit/stats/regress/regress.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "regress/regress_detail.hpp"
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

namespace numkit::stats {

namespace detail {

void regress_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("regress: requires (y, X[, alpha])",
                    0, 0, "regress", "", "numkit:regress:nargin");
    const double alpha = (args.size() >= 3 && !args[2].isEmpty())
                         ? args[2].toScalar() : 0.05;
    auto [b, bint, r, rint, stats] = regress_full(args[0], args[1], alpha, ctx.engine->resource());
    outs[0] = std::move(b);
    if (nargout > 1) outs[1] = std::move(bint);
    if (nargout > 2) outs[2] = std::move(r);
    if (nargout > 3) outs[3] = std::move(rint);
    if (nargout > 4) outs[4] = std::move(stats);
}

void ridge_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ridge: requires (y, X, k[, scaled])",
                    0, 0, "ridge", "", "numkit:ridge:nargin");
    bool scaled = true;
    if (args.size() >= 4 && !args[3].isEmpty())
        scaled = (args[3].toScalar() != 0.0);
    outs[0] = ridge(args[0], args[1], args[2], scaled, ctx.engine->resource());
}

void lscov_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("lscov: requires (A, b[, w])",
                    0, 0, "lscov", "", "numkit:lscov:nargin");
    auto *mr = ctx.engine->resource();
    Value w_empty = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    const Value &w = (args.size() >= 3) ? args[2] : w_empty;
    auto [x, stdx, mse, S] = lscov(args[0], args[1], w, mr);
    outs[0] = std::move(x);
    if (nargout > 1) outs[1] = std::move(stdx);
    if (nargout > 2) outs[2] = std::move(mse);
    if (nargout > 3) outs[3] = std::move(S);
}

} // namespace detail

} // namespace numkit::stats
