// toolboxes/image/src/filter/fir2d_reg.cpp
//
// Register half of the image fir2d builtins: the CallContext wrappers
// delegating to the engine-free compute in fir2d.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/image/filter/filter.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace numkit::image {

namespace detail {

void fsamp2_reg(Span<const Value> args, std::size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("fsamp2: requires (Hd) or (f1, f2, Hd, [m n])",
                    0, 0, "fsamp2", "", "numkit:fsamp2:nargin");
    if (args.size() == 1) {
        outs[0] = fsamp2(args[0], Value::Empty, Value::Empty, {},
                         ctx.engine->resource());
        return;
    }
    if (args.size() == 4) {
        // (f1, f2, Hd, siz) — non-uniform form (not supported).
        outs[0] = fsamp2(args[2], args[0], args[1], {},
                         ctx.engine->resource());
        return;
    }
    throw Error("fsamp2: requires 1 or 4 arguments",
                0, 0, "fsamp2", "", "numkit:fsamp2:nargin");
}

void ftrans2_reg(Span<const Value> args, std::size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ftrans2: requires (b [, t])",
                    0, 0, "ftrans2", "", "numkit:ftrans2:nargin");
    Value t = (args.size() >= 2) ? args[1] : Value::Empty;
    outs[0] = ftrans2(args[0], t, ctx.engine->resource());
}

void fwind1_reg(Span<const Value> args, std::size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fwind1: requires (Hd, win) or (Hd, win1, win2)",
                    0, 0, "fwind1", "", "numkit:fwind1:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 2) {
        // (Hd, win) — Huang's method.
        outs[0] = fwind1(args[0], args[1], Value::Empty, mr);
    } else if (args.size() == 3) {
        // (Hd, win1, win2) — separable.
        outs[0] = fwind1(args[0], args[1], args[2], mr);
    } else {
        // (f1, f2, Hd, ...) — non-uniform spacing case (not supported).
        throw Error("fwind1: non-uniform-spacing form is not yet supported",
                    0, 0, "fwind1", "", "numkit:fwind1:nonuniform");
    }
}

void fwind2_reg(Span<const Value> args, std::size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fwind2: requires (Hd, W) or (f1, f2, Hd, W)",
                    0, 0, "fwind2", "", "numkit:fwind2:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 2) {
        outs[0] = fwind2(args[0], args[1], mr);
    } else {
        throw Error("fwind2: non-uniform-spacing form is not yet supported",
                    0, 0, "fwind2", "", "numkit:fwind2:nonuniform");
    }
}

}  // namespace detail
}  // namespace numkit::image
