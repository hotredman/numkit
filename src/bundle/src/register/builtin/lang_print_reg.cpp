#include <nk/core/engine.hpp>
#include <nk/runtime/io.hpp>
#include <nk/builtin/strfun.hpp>
#include <nk/builtin/iofun.hpp>
#include <nk/value/error.hpp>
#include <nk/value/shape_ops.hpp>
#include <nk/value/value.hpp>
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

namespace nk::builtin {
using namespace nk::builtin;

namespace detail {

void disp_reg(Span<const Value> args, size_t, Span<Value>, CallContext &ctx)
{
    if (args.empty()) return;
    if (args[0].isObject()) {
        ctx.engine->outputText(ctx.engine->formatObjectDisplay("", args[0]));
        return;
    }
    ctx.engine->outputText(dispFormat(args[0]));
}

void fprintf_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.empty())
        throw Error("fprintf: requires at least 1 argument", 0, 0, "fprintf", "", "nk:fprintf:nargin");
    size_t count = ::nk::runtime::fprintf(*ctx.engine, args);
    if (nargout >= 1 && !outs.empty())
        outs[0] = Value::scalar(static_cast<double>(count), ctx.engine->resource());
}

} // namespace detail

} // namespace nk::builtin
