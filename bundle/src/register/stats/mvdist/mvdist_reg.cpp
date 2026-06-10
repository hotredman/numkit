// toolboxes/signal/src/mvdist/mvdist_reg.cpp
//
// CallContext register half of mvdist/mvdist.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/stats/mvdist/mvdist.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
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

void mvnpdf_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("mvnpdf: requires X[, mu, Sigma]",
                    0, 0, "mvnpdf", "", "numkit:mvnpdf:nargin");
    auto *mr = ctx.engine->resource();
    Value empty = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    const Value &mu  = (args.size() >= 2) ? args[1] : empty;
    const Value &sig = (args.size() >= 3) ? args[2] : empty;
    outs[0] = mvnpdf(args[0], mu, sig, mr);
}

void mnpdf_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mnpdf: requires (X, P)",
                    0, 0, "mnpdf", "", "numkit:mnpdf:nargin");
    outs[0] = mnpdf(args[0], args[1], ctx.engine->resource());
}

void mvtpdf_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("mvtpdf: requires (X, C, df)",
                    0, 0, "mvtpdf", "", "numkit:mvtpdf:nargin");
    const double df = args[2].toScalar();
    outs[0] = mvtpdf(args[0], args[1], df, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::stats
