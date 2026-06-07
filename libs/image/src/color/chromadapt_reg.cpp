// libs/image/src/color/chromadapt_reg.cpp
//
// Register half of the image chromadapt builtin: the CallContext wrapper
// (NV-pair parsing) delegating to the engine-free compute in chromadapt.cpp.
// library.cpp forward-declares + registers it by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/image/color/color.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <cctype>
#include <cstddef>
#include <string>

namespace numkit::image {

namespace detail {

void chromadapt_reg(Span<const Value> args, std::size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("chromadapt: requires (A, illuminant [, NV...])",
                    0, 0, "chromadapt", "", "numkit:chromadapt:nargin");
    auto *mr = ctx.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };
    std::string method = "bradford";
    std::string colorSpace = "srgb";
    std::size_t i = 2;
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("chromadapt: expected NV-pair name string",
                        0, 0, "chromadapt", "", "numkit:chromadapt:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "method")           method     = args[i + 1].toString();
        else if (nlo == "colorspace")  colorSpace = args[i + 1].toString();
        else throw Error("chromadapt: unknown option '" + name + "'",
                         0, 0, "chromadapt", "",
                         "numkit:chromadapt:unknownNv");
        i += 2;
    }
    outs[0] = chromadapt(args[0], args[1], method, colorSpace, mr);
}

}  // namespace detail
}  // namespace numkit::image
