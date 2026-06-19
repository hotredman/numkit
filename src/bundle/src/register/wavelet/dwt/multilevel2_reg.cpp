// toolboxes/wavelet/src/dwt/multilevel2_reg.cpp
//
// Register the 2-D multilevel DWT family: the CallContext builtins
// wavedec2 / waverec2 / appcoef2 / detcoef2 (argument parsing, the
// detcoef2 'all' 3-output shape, default-level handling) that delegate to
// the engine-free compute in multilevel2.cpp. library.cpp forward-declares
// + registers these by name.

#include <numkit/wavelet/dwt/multilevel2.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

#include <cctype>
#include <string>
#include <utility>

namespace numkit::wavelet {
namespace detail {

static std::string argString(const Value &v) {
    if (!v.isChar() && !v.isString())
        throw Error("wavelet: expected string argument",
                    0, 0, "", "", "numkit:wavelet:type");
    return v.toString();
}

void wavedec2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("wavedec2: requires (X, N, wname)",
                    0, 0, "wavedec2", "", "numkit:wavedec2:nargin");
    auto *mr = ctx.engine->resource();
    auto r = wavedec2(args[0], static_cast<int>(args[1].toScalar()),
                      argString(args[2]), mr);
    if (outs.size() >= 1) outs[0] = std::move(r.c);
    if (outs.size() >= 2) outs[1] = std::move(r.s);
}

void waverec2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("waverec2: requires (C, S, wname)",
                    0, 0, "waverec2", "", "numkit:waverec2:nargin");
    outs[0] = waverec2(args[0], args[1], argString(args[2]),
                       ctx.engine->resource());
}

void appcoef2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("appcoef2: requires (C, S, wname[, level])",
                    0, 0, "appcoef2", "", "numkit:appcoef2:nargin");
    auto *mr = ctx.engine->resource();
    int level = -1;   // default = coarsest
    if (args.size() >= 4 && !args[3].isEmpty()
        && !(args[3].isChar() || args[3].isString()))
        level = static_cast<int>(args[3].toScalar());
    outs[0] = appcoef2(args[0], args[1], argString(args[2]), level, mr);
}

void detcoef2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("detcoef2: requires (type, C, S, level)",
                    0, 0, "detcoef2", "", "numkit:detcoef2:nargin");
    auto *mr = ctx.engine->resource();
    std::string type = argString(args[0]);
    std::string lt = type;
    for (auto &c : lt) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    const int level = static_cast<int>(args[3].toScalar());

    if (lt == "all") {
        // [H, V, D] = detcoef2('all', C, S, level).
        if (outs.size() >= 1) outs[0] = detcoef2("h", args[1], args[2], level, mr);
        if (outs.size() >= 2) outs[1] = detcoef2("v", args[1], args[2], level, mr);
        if (outs.size() >= 3) outs[2] = detcoef2("d", args[1], args[2], level, mr);
        return;
    }
    outs[0] = detcoef2(type, args[1], args[2], level, mr);
}

} // namespace detail
} // namespace numkit::wavelet
