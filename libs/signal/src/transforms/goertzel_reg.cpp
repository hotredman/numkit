// libs/signal/src/transforms/goertzel_reg.cpp
//
// CallContext register half of transforms/goertzel.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/signal/transforms/goertzel.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
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

void goertzel_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.empty())
        throw Error("goertzel: requires (x[, ind])",
                     0, 0, "goertzel", "", "numkit:goertzel:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 1 || args[1].isEmpty()) {
        // 1-arg form (or empty 2nd arg): MATLAB defaults `ind = 1:N`
        // and the output has the SAME SHAPE as x (per `doc goertzel`).
        // For a column input we need a column-shaped ind; for a row
        // input we need a row-shaped ind. The Goertzel kernel uses
        // ind.dims() to size the output, so building ind with the same
        // dims as x propagates the shape correctly.
        const Value &x = args[0];
        const size_t N = x.numel();
        const size_t rows = x.dims().rows();
        const size_t cols = x.dims().cols();
        Value ind = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
        double *id = ind.doubleDataMut();
        for (size_t i = 0; i < N; ++i)
            id[i] = static_cast<double>(i + 1);
        outs[0] = goertzel(x, ind, mr);
        return;
    }
    outs[0] = goertzel(args[0], args[1], mr);
}

} // namespace detail

} // namespace numkit::signal
