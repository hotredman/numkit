// toolboxes/signal/src/language/strings/print_reg.cpp
//
// CallContext register half of language/strings/print.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/lang/strings/format.hpp>
#include <numkit/lang/strings/print.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/shape_ops.hpp>
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
using namespace numkit::lang;  // C4c localized (umbrella removed)

namespace detail {

void disp_reg(Span<const Value> args, size_t, Span<Value>, CallContext &ctx)
{
    // OBJECT: disp(obj) uses the class display hook (no name header).
    if (!args.empty() && args[0].isObject()) {
        ctx.engine->outputText(ctx.engine->formatObjectDisplay("", args[0]));
        return;
    }
    disp(*ctx.engine, args);
}

void fprintf_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                 CallContext &ctx)
{
    std::size_t count = fprintf(*ctx.engine, args);
    // MATLAB: `count = fprintf(...)` returns the number of bytes written.
    // Only materialise the output when explicitly requested — a bare
    // `fprintf(...)` sets no `ans`.
    if (nargout >= 1 && !outs.empty())
        outs[0] = Value::scalar(static_cast<double>(count), ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::builtin
