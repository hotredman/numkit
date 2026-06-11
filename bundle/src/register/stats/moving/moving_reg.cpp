// toolboxes/signal/src/moving/moving_reg.cpp
//
// CallContext register half of moving/moving.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/stats/moving/moving.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include <numkit/ops/helpers.hpp>            // createLike, createForDims (toolboxes/builtin/src/)
#include "moving/moving_detail.hpp"
#include <numkit/ops/reductions.hpp>  // numkit::ops::firstNonSingletonDim, validateDim
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

void movmean_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movmean: requires at least 2 arguments (x, k)",
                     0, 0, "movmean", "", "numkit:movmean:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto kBuf = decodeWindowValueToScratch(args[1], "movmean", scratch);
    auto opt = parseMovExtras(args, 2, "movmean");
    outs[0] = movmean_impl(args[0], Span<const size_t>(kBuf.data(), kBuf.size()), opt, mr);
}

void movsum_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movsum: requires at least 2 arguments (x, k)",
                     0, 0, "movsum", "", "numkit:movsum:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto kBuf = decodeWindowValueToScratch(args[1], "movsum", scratch);
    auto opt = parseMovExtras(args, 2, "movsum");
    outs[0] = movsum_impl(args[0], Span<const size_t>(kBuf.data(), kBuf.size()), opt, mr);
}

void movmin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movmin: requires at least 2 arguments (x, k)",
                     0, 0, "movmin", "", "numkit:movmin:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto kBuf = decodeWindowValueToScratch(args[1], "movmin", scratch);
    auto opt = parseMovExtras(args, 2, "movmin");
    outs[0] = movmin_impl(args[0], Span<const size_t>(kBuf.data(), kBuf.size()), opt, mr);
}

void movmax_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movmax: requires at least 2 arguments (x, k)",
                     0, 0, "movmax", "", "numkit:movmax:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto kBuf = decodeWindowValueToScratch(args[1], "movmax", scratch);
    auto opt = parseMovExtras(args, 2, "movmax");
    outs[0] = movmax_impl(args[0], Span<const size_t>(kBuf.data(), kBuf.size()), opt, mr);
}

void movprod_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movprod: requires at least 2 arguments (x, k)",
                     0, 0, "movprod", "", "numkit:movprod:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto kBuf = decodeWindowValueToScratch(args[1], "movprod", scratch);
    auto opt = parseMovExtras(args, 2, "movprod");
    outs[0] = movprod_impl(args[0], Span<const size_t>(kBuf.data(), kBuf.size()), opt, mr);
}

void movmedian_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movmedian: requires at least 2 arguments (x, k)",
                     0, 0, "movmedian", "", "numkit:movmedian:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto kBuf = decodeWindowValueToScratch(args[1], "movmedian", scratch);
    auto opt = parseMovExtras(args, 2, "movmedian");
    outs[0] = movmedian_impl(args[0], Span<const size_t>(kBuf.data(), kBuf.size()), opt, mr);
}

void movvar_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movvar: requires at least 2 arguments (x, k)",
                     0, 0, "movvar", "", "numkit:movvar:nargin");
    int normFlag = 0;
    size_t extras_start = 2;
    if (args.size() >= 3 && !args[2].isChar() && !args[2].isString()
        && args[2].isScalar()) {
        // Could be normFlag or dim — disambiguate: normFlag is 0 or 1.
        const double v = args[2].toScalar();
        if (v == 0.0 || v == 1.0) {
            normFlag = static_cast<int>(v);
            extras_start = 3;
        }
    }
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto kBuf = decodeWindowValueToScratch(args[1], "movvar", scratch);
    auto opt = parseMovExtras(args, extras_start, "movvar");
    outs[0] = movvar_impl(args[0], Span<const size_t>(kBuf.data(), kBuf.size()), normFlag, opt, mr);
}

void movstd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movstd: requires at least 2 arguments (x, k)",
                     0, 0, "movstd", "", "numkit:movstd:nargin");
    int normFlag = 0;
    size_t extras_start = 2;
    if (args.size() >= 3 && !args[2].isChar() && !args[2].isString()
        && args[2].isScalar()) {
        const double v = args[2].toScalar();
        if (v == 0.0 || v == 1.0) {
            normFlag = static_cast<int>(v);
            extras_start = 3;
        }
    }
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto kBuf = decodeWindowValueToScratch(args[1], "movstd", scratch);
    auto opt = parseMovExtras(args, extras_start, "movstd");
    outs[0] = movstd_impl(args[0], Span<const size_t>(kBuf.data(), kBuf.size()), normFlag, opt, mr);
}

void movmad_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movmad: requires at least 2 arguments (x, k)",
                     0, 0, "movmad", "", "numkit:movmad:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto kBuf = decodeWindowValueToScratch(args[1], "movmad", scratch);
    auto opt = parseMovExtras(args, 2, "movmad");
    outs[0] = movmad_impl(args[0], Span<const size_t>(kBuf.data(), kBuf.size()), opt, mr);
}

void smoothdata_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("smoothdata: requires at least 1 argument",
                     0, 0, "smoothdata", "", "numkit:smoothdata:nargin");
    std::string method = "movmean";
    int k = 0;
    if (args.size() >= 2) {
        if (args[1].isChar() || args[1].isString())
            method = args[1].toString();
        else
            k = static_cast<int>(args[1].toScalar());
    }
    if (args.size() >= 3) {
        if (k == 0 && (args[2].isScalar() || args[2].numel() == 1))
            k = static_cast<int>(args[2].toScalar());
    }
    outs[0] = smoothdata(args[0], method, k, 0, ctx.engine->resource());
}

void hampel_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("hampel: requires at least 1 argument",
                     0, 0, "hampel", "", "numkit:hampel:nargin");
    const int k = (args.size() >= 2) ? static_cast<int>(args[1].toScalar()) : 3;
    const double nsigmas = (args.size() >= 3) ? args[2].toScalar() : 3.0;
    auto *mr = ctx.engine->resource();
    const Value &x = args[0];

    if (nargout < 2) {
        outs[0] = hampel(x, k, nsigmas, mr);
        return;
    }

    // [y, i, xmedian, xsigma] = hampel(...). i is a logical mask of the
    // replaced (outlier) points; xmedian/xsigma are the local median and
    // 1.4826·MAD estimate at every sample. vs MATLAB R2025b.
    hampelValidate(x, k, nsigmas);
    const std::size_t n = x.numel();
    auto y    = createLike(x, ValueType::DOUBLE,  mr);
    auto imask = createLike(x, ValueType::LOGICAL, mr);
    auto xmed = createLike(x, ValueType::DOUBLE,  mr);
    auto xsig = createLike(x, ValueType::DOUBLE,  mr);
    if (n > 0)
        hampelCore(x.doubleData(), n, k, nsigmas, y.doubleDataMut(),
                   imask.logicalDataMut(), xmed.doubleDataMut(),
                   xsig.doubleDataMut(), mr);

    outs[0] = y;
    outs[1] = imask;
    if (nargout >= 3) outs[2] = xmed;
    if (nargout >= 4) outs[3] = xsig;
}

} // namespace detail

} // namespace numkit::stats
