// libs/signal/src/spline/spline_reg.cpp
//
// CallContext register half of spline/spline.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/stats/spline/spline.hpp>
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

void aveknt_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("aveknt: requires (t, k)",
                    0, 0, "aveknt", "", "numkit:aveknt:nargin");
    outs[0] = aveknt(args[0], static_cast<int>(args[1].toScalar()), ctx.engine->resource());
}

void augknt_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("augknt: requires (knots, k)",
                    0, 0, "augknt", "", "numkit:augknt:nargin");
    outs[0] = augknt(args[0], static_cast<int>(args[1].toScalar()), ctx.engine->resource());
}

void brk2knt_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("brk2knt: requires (breaks, mults)",
                    0, 0, "brk2knt", "", "numkit:brk2knt:nargin");
    outs[0] = brk2knt(args[0], args[1], ctx.engine->resource());
}

void ppmak_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ppmak: requires (breaks, coefs[, d])",
                    0, 0, "ppmak", "", "numkit:ppmak:nargin");
    int d = 1;
    if (args.size() >= 3 && !args[2].isEmpty())
        d = static_cast<int>(args[2].toScalar());
    outs[0] = ppmak(args[0], args[1], d, ctx.engine->resource());
}

void fnval_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fnval: requires (pp, x)",
                    0, 0, "fnval", "", "numkit:fnval:nargin");
    outs[0] = fnval(args[0], args[1], ctx.engine->resource());
}

void fnder_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("fnder: requires (pp[, order])",
                    0, 0, "fnder", "", "numkit:fnder:nargin");
    int order = 1;
    if (args.size() >= 2 && !args[1].isEmpty())
        order = static_cast<int>(args[1].toScalar());
    outs[0] = fnder(args[0], order, ctx.engine->resource());
}

void fnint_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("fnint: requires (pp)",
                    0, 0, "fnint", "", "numkit:fnint:nargin");
    outs[0] = fnint(args[0], ctx.engine->resource());
}

void csapi_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("csapi: requires (x, y)",
                    0, 0, "csapi", "", "numkit:csapi:nargin");
    outs[0] = csapi(args[0], args[1], ctx.engine->resource());
}

void fncmb_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    auto isStruct = [](const Value &v) { return v.isStruct(); };
    if (args.size() == 2) {
        // fncmb(pp, c) or fncmb(c, pp)
        if (isStruct(args[0]) && !isStruct(args[1])) {
            outs[0] = fncmb(args[0], args[1].toScalar(), Value::Empty, 0.0, mr);
        } else if (!isStruct(args[0]) && isStruct(args[1])) {
            outs[0] = fncmb(args[1], args[0].toScalar(), Value::Empty, 0.0, mr);
        } else {
            throw Error("fncmb: 2-arg form requires (pp, scalar) or (scalar, pp)",
                        0, 0, "fncmb", "", "numkit:fncmb:nargin");
        }
        return;
    }
    if (args.size() == 4) {
        outs[0] = fncmb(args[0], args[1].toScalar(), args[2], args[3].toScalar(), mr);
        return;
    }
    throw Error("fncmb: requires (pp, c) or (pp1, c1, pp2, c2)",
                0, 0, "fncmb", "", "numkit:fncmb:nargin");
}

void fnbrk_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fnbrk: requires (pp, part)",
                    0, 0, "fnbrk", "", "numkit:fnbrk:nargin");
    if (!args[1].isChar() && !args[1].isString())
        throw Error("fnbrk: numkit only supports the named-part form",
                    0, 0, "fnbrk", "", "numkit:fnbrk:type");
    outs[0] = fnbrk(args[0], args[1].toString(), ctx.engine->resource());
}

void knt2brk_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("knt2brk: requires (knots)",
                    0, 0, "knt2brk", "", "numkit:knt2brk:nargin");
    auto [b, m] = knt2brk(args[0], ctx.engine->resource());
    outs[0] = std::move(b);
    if (nargout > 1) outs[1] = std::move(m);
}

} // namespace detail

} // namespace numkit::stats
