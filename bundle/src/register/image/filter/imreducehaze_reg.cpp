// toolboxes/image/src/filter/imreducehaze_reg.cpp
//
// Register half of the image imreducehaze builtins: the CallContext wrappers
// delegating to the engine-free compute in imreducehaze.cpp. library.cpp
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

void imreducehaze_reg(Span<const Value> args, std::size_t nargout,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imreducehaze: requires (I [, AMOUNT] [, NV...])",
                    0, 0, "imreducehaze", "",
                    "numkit:imreducehaze:nargin");
    auto *mr = ctx.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    const Value &I = args[0];
    double amount = 1.0;
    std::string method = "simpledcp";
    Value atmLight;
    std::string ce = "global";
    double boostAmount = 0.0;  // sentinel: default-on-demand
    bool boostExplicitlySet = false;

    std::size_t i = 1;
    if (i < args.size() && !is_string(args[i])) {
        amount = args[i].toScalar();
        ++i;
    }
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("imreducehaze: expected NV-pair name string",
                        0, 0, "imreducehaze", "",
                        "numkit:imreducehaze:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "method") {
            method = args[i + 1].toString();
        } else if (nlo == "atmosphericlight") {
            atmLight = args[i + 1];
        } else if (nlo == "contrastenhancement") {
            ce = args[i + 1].toString();
        } else if (nlo == "boostamount") {
            boostAmount = args[i + 1].toScalar();
            boostExplicitlySet = true;
        } else {
            throw Error("imreducehaze: unknown option '" + name + "'",
                        0, 0, "imreducehaze", "",
                        "numkit:imreducehaze:unknownNv");
        }
        i += 2;
    }
    if (boostExplicitlySet) {
        // Match MATLAB: BoostAmount only allowed with boost contrast.
        std::string celo;
        for (char ch : ce)
            celo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (celo != "boost")
            throw Error("imreducehaze: BoostAmount may only be specified "
                        "when ContrastEnhancement is 'boost'",
                        0, 0, "imreducehaze", "",
                        "numkit:imreducehaze:boostNotAllowed");
    }

    Value T, L;
    outs[0] = imreducehaze(I, amount, method, atmLight, ce,
                           boostAmount > 0.0 ? boostAmount : 0.1,
                           T, L, mr);
    if (nargout >= 2 && outs.size() >= 2) outs[1] = std::move(T);
    if (nargout >= 3 && outs.size() >= 3) outs[2] = std::move(L);
}

}  // namespace detail
}  // namespace numkit::image
