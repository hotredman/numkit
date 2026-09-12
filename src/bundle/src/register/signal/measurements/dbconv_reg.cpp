// toolboxes/signal/src/measurements/dbconv_reg.cpp
//
// CallContext register half of measurements/dbconv.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <nk/core/engine.hpp>
#include <nk/signal/measurements/dbconv.hpp>
#include <nk/value/error.hpp>
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

namespace nk::signal {

namespace detail {

void db_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("db: requires at least 1 argument",
                     0, 0, "db", "", "nk:db:nargin");
    std::string mode = "voltage";
    if (args.size() >= 2) {
        if (!args[1].isChar() && !args[1].isString())
            throw Error("db: 2nd argument must be a string",
                         0, 0, "db", "", "nk:db:badType");
        mode = args[1].toString();
    }
    outs[0] = db(args[0], mode, ctx.engine->resource());
}

void db2mag_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("db2mag: requires 1 argument",
                     0, 0, "db2mag", "", "nk:db2mag:nargin");
    outs[0] = db2mag(args[0], ctx.engine->resource());
}

void mag2db_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("mag2db: requires 1 argument",
                     0, 0, "mag2db", "", "nk:mag2db:nargin");
    outs[0] = mag2db(args[0], ctx.engine->resource());
}

void db2pow_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("db2pow: requires 1 argument",
                     0, 0, "db2pow", "", "nk:db2pow:nargin");
    outs[0] = db2pow(args[0], ctx.engine->resource());
}

void pow2db_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("pow2db: requires 1 argument",
                     0, 0, "pow2db", "", "nk:pow2db:nargin");
    outs[0] = pow2db(args[0], ctx.engine->resource());
}

} // namespace detail

} // namespace nk::signal
