// libs/image/src/filter/fibermetric_reg.cpp
//
// Register half of the image fibermetric builtins: the CallContext wrappers
// delegating to the engine-free compute in fibermetric.cpp. library.cpp
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

void fibermetric_reg(Span<const Value> args, std::size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("fibermetric: requires (I [, THICKNESS] [, NV...])",
                    0, 0, "fibermetric", "",
                    "numkit:fibermetric:nargin");
    auto *mr = ctx.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    const Value &I = args[0];
    std::vector<double> thickness;
    double c = -1.0;
    bool bright = true;

    std::size_t i = 1;
    if (i < args.size() && !is_string(args[i])) {
        const Value &v = args[i];
        const std::size_t N = v.numel();
        thickness.resize(N);
        for (std::size_t k = 0; k < N; ++k)
            thickness[k] = v.elemAsDouble(k);
        ++i;
    }
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("fibermetric: expected NV-pair name string",
                        0, 0, "fibermetric", "",
                        "numkit:fibermetric:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "structuresensitivity") {
            c = args[i + 1].toScalar();
            if (!std::isfinite(c) || c <= 0.0)
                throw Error("fibermetric: StructureSensitivity must be "
                            "a positive finite scalar",
                            0, 0, "fibermetric", "",
                            "numkit:fibermetric:sens");
        } else if (nlo == "objectpolarity") {
            std::string s = args[i + 1].toString();
            std::string slo;
            for (char ch : s)
                slo += static_cast<char>(std::tolower(
                    static_cast<unsigned char>(ch)));
            if (slo == "bright")    bright = true;
            else if (slo == "dark") bright = false;
            else throw Error("fibermetric: ObjectPolarity must be "
                             "'bright' or 'dark'",
                             0, 0, "fibermetric", "",
                             "numkit:fibermetric:polarity");
        } else {
            throw Error("fibermetric: unknown option '" + name + "'",
                        0, 0, "fibermetric", "",
                        "numkit:fibermetric:unknownNv");
        }
        i += 2;
    }
    outs[0] = fibermetric(I, thickness, c, bright, mr);
}

}  // namespace detail
}  // namespace numkit::image
