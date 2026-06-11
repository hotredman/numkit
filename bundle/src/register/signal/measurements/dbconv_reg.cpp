// toolboxes/signal/src/measurements/dbconv_reg.cpp
//
// CallContext register half of measurements/dbconv.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/signal/measurements/dbconv.hpp>
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

namespace numkit::signal {

namespace detail {

void db_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("db: requires at least 1 argument",
                     0, 0, "db", "", "numkit:db:nargin");
    std::string mode = "voltage";
    if (args.size() >= 2) {
        if (!args[1].isChar() && !args[1].isString())
            throw Error("db: 2nd argument must be a string",
                         0, 0, "db", "", "numkit:db:badType");
        mode = args[1].toString();
    }
    outs[0] = db(args[0], mode, ctx.engine->resource());
}

void db2mag_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("db2mag: requires 1 argument",
                     0, 0, "db2mag", "", "numkit:db2mag:nargin");
    outs[0] = db2mag(args[0], ctx.engine->resource());
}

void mag2db_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("mag2db: requires 1 argument",
                     0, 0, "mag2db", "", "numkit:mag2db:nargin");
    outs[0] = mag2db(args[0], ctx.engine->resource());
}

void db2pow_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("db2pow: requires 1 argument",
                     0, 0, "db2pow", "", "numkit:db2pow:nargin");
    outs[0] = db2pow(args[0], ctx.engine->resource());
}

void pow2db_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("pow2db: requires 1 argument",
                     0, 0, "pow2db", "", "numkit:pow2db:nargin");
    outs[0] = pow2db(args[0], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::signal
