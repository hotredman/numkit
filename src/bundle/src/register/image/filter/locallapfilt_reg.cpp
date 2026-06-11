// toolboxes/image/src/filter/locallapfilt_reg.cpp
//
// Register half of the image locallapfilt builtins: the CallContext wrappers
// delegating to the engine-free compute in locallapfilt.cpp. library.cpp
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

void locallapfilt_reg(Span<const Value> args, std::size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("locallapfilt: requires (I, sigma, alpha [, beta] [, NV...])",
                    0, 0, "locallapfilt", "",
                    "numkit:locallapfilt:nargin");
    auto *mr = ctx.engine->resource();

    const Value &I = args[0];
    const double sigma = args[1].toScalar();
    const double alpha = args[2].toScalar();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    double beta = 1.0;
    int nlevels = -1;          // -1 = auto
    bool processLuminance = true;

    std::size_t i = 3;
    // Optional positional beta (if 4th arg is numeric scalar and not a string).
    if (i < args.size() && !is_string(args[i])) {
        if (args[i].numel() != 1)
            throw Error("locallapfilt: beta must be a scalar",
                        0, 0, "locallapfilt", "",
                        "numkit:locallapfilt:betaShape");
        beta = args[i].toScalar();
        ++i;
    }

    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("locallapfilt: expected NV-pair name string",
                        0, 0, "locallapfilt", "",
                        "numkit:locallapfilt:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "numintensitylevels") {
            const Value &v = args[i + 1];
            if (is_string(v)) {
                std::string s = v.toString();
                std::string slo;
                for (char ch : s)
                    slo += static_cast<char>(std::tolower(
                        static_cast<unsigned char>(ch)));
                if (slo != "auto")
                    throw Error("locallapfilt: NumIntensityLevels string "
                                "must be 'auto'",
                                0, 0, "locallapfilt", "",
                                "numkit:locallapfilt:nIntStr");
                nlevels = -1;
            } else {
                nlevels = static_cast<int>(v.toScalar());
                if (nlevels < 1)
                    throw Error("locallapfilt: NumIntensityLevels must be >= 1",
                                0, 0, "locallapfilt", "",
                                "numkit:locallapfilt:nIntRange");
            }
        } else if (nlo == "colormode") {
            std::string s = args[i + 1].toString();
            std::string slo;
            for (char ch : s)
                slo += static_cast<char>(std::tolower(
                    static_cast<unsigned char>(ch)));
            if (slo == "luminance")     processLuminance = true;
            else if (slo == "separate") processLuminance = false;
            else throw Error("locallapfilt: ColorMode must be 'luminance' or "
                             "'separate'",
                             0, 0, "locallapfilt", "",
                             "numkit:locallapfilt:colorMode");
        } else {
            throw Error("locallapfilt: unknown option '" + name + "'",
                        0, 0, "locallapfilt", "",
                        "numkit:locallapfilt:unknownNv");
        }
        i += 2;
    }

    outs[0] = locallapfilt(I, sigma, alpha, beta, nlevels,
                           processLuminance, mr);
}

} // namespace detail

} // namespace numkit::image
