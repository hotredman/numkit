// toolboxes/signal/src/programming/errors/diagnostics_reg.cpp
//
// CallContext register half of programming/errors/diagnostics.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/lang/strings/format.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/builtin/programming/errors/diagnostics.hpp>
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

namespace numkit::builtin {

namespace detail {

void error_reg(Span<const Value> args, size_t, Span<Value>, CallContext &)
{
    error(args);
}

void warning_reg(Span<const Value> args, size_t, Span<Value>, CallContext &ctx)
{
    warning(*ctx.engine, args);
}

void lastwarn_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                  CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    // Two-output read form: [msg, id] = lastwarn();
    if (args.empty()) {
        auto lw = lastwarnGet();
        outs[0] = Value::fromString(lw.msg, mr);
        if (nargout > 1) outs[1] = Value::fromString(lw.id, mr);
        return;
    }
    // Set form: lastwarn(msg) or lastwarn(msg, id).
    if (!args[0].isChar() && !args[0].isString())
        throw Error("lastwarn: msg must be a char or string",
                     0, 0, "lastwarn", "", "numkit:lastwarn:badArg");
    std::string msg = args[0].toString();
    std::string id;
    if (args.size() >= 2) {
        if (!args[1].isChar() && !args[1].isString())
            throw Error("lastwarn: id must be a char or string",
                         0, 0, "lastwarn", "", "numkit:lastwarn:badId");
        id = args[1].toString();
    }
    lastwarnSet(msg, id);
    // MATLAB's set form returns nothing; we mirror that.
    if (nargout > 0) outs[0] = Value::fromString("", mr);
}

void MException_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    outs[0] = mexception(args, ctx.engine->resource());
}

void rethrow_reg(Span<const Value> args, size_t, Span<Value>, CallContext &)
{
    if (args.empty())
        throw Error("rethrow requires an MException struct", 0, 0, "rethrow", "",
                     "numkit:rethrow:nargin");
    rethrowStruct(args[0]);
}

void throw_reg(Span<const Value> args, size_t, Span<Value>, CallContext &)
{
    if (args.empty())
        throw Error("throw requires an MException struct", 0, 0, "throw", "",
                     "numkit:throw:nargin");
    rethrowStruct(args[0]);
}

void assert_reg(Span<const Value> args, size_t, Span<Value>, CallContext &)
{
    assertCond(args);
}

} // namespace detail

} // namespace numkit::builtin
