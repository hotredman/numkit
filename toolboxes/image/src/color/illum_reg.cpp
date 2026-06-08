// toolboxes/image/src/color/illum_reg.cpp
//
// Register half of the image illuminant builtins: the CallContext wrappers
// delegating to the engine-free compute in illum.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/image/color/color.hpp>
#include "illum_detail.hpp"

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <cctype>
#include <cstddef>
#include <string>
#include <vector>

namespace numkit::image {

namespace detail {

// Name-value parser shared by illumwhite/illumgray; supports the
// two MATLAB options 'Mask' and 'Norm' (the latter is illumgray-only,
// caller passes `allow_norm = false` for illumwhite).
static void parse_nv_pairs(Span<const Value> args, std::size_t start_idx,
                           bool allow_norm, Value &mask, double &norm_exp,
                           const char *fn)
{
    while (start_idx + 1 < args.size()) {
        if (!args[start_idx].isChar() && !args[start_idx].isString())
            throw Error(std::string(fn) + ": expected option name string",
                        0, 0, fn, "", "numkit:image:badNvArg");
        std::string name = args[start_idx].toString();
        for (auto &c : name) c = static_cast<char>(std::tolower(
            static_cast<unsigned char>(c)));
        if (name == "mask") {
            mask = args[start_idx + 1];
        } else if (allow_norm && name == "norm") {
            norm_exp = args[start_idx + 1].toScalar();
        } else {
            throw Error(std::string(fn) + ": unknown option '" + name + "'",
                        0, 0, fn, "", "numkit:image:unknownNvArg");
        }
        start_idx += 2;
    }
    if (start_idx < args.size())
        throw Error(std::string(fn) + ": trailing unpaired name-value arg",
                    0, 0, fn, "", "numkit:image:unpairedNv");
}

void illumwhite_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1)
        throw Error("illumwhite: requires (A [, P] [, 'Mask', M])",
                    0, 0, "illumwhite", "", "numkit:illumwhite:nargin");
    auto *mr = ctx.engine->resource();
    double P = 1.0;       // MATLAB default percentile = 1%.
    std::size_t i = 1;
    if (args.size() >= 2 && !args[1].isChar() && !args[1].isString()) {
        P = args[1].toScalar();
        i = 2;
    }
    Value mask = Value::Empty;
    double dummy_norm = 1.0;
    parse_nv_pairs(args, i, /*allow_norm=*/false, mask, dummy_norm,
                   "illumwhite");
    outs[0] = illumwhite(args[0], P, mask, mr);
}

void imcolordiff_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imcolordiff: requires (I1, I2 [, NameValue...])",
                    0, 0, "imcolordiff", "", "numkit:imcolordiff:nargin");
    auto *mr = ctx.engine->resource();
    std::string standard = "CIE94";
    bool is_input_lab = false;
    double kL = 1.0, kC = 1.0, kH = 1.0, K1 = 0.045, K2 = 0.015;
    std::size_t i = 2;
    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("imcolordiff: expected option name string",
                        0, 0, "imcolordiff", "", "numkit:imcolordiff:badNvArg");
        std::string name = args[i].toString();
        std::string name_lo = name;
        for (auto &c : name_lo)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if      (name_lo == "standard")  standard     = args[i + 1].toString();
        else if (name_lo == "isinputlab") is_input_lab = (args[i + 1].toScalar() != 0.0);
        else if (name_lo == "kl")        kL = args[i + 1].toScalar();
        else if (name_lo == "kc")        kC = args[i + 1].toScalar();
        else if (name_lo == "kh")        kH = args[i + 1].toScalar();
        else if (name_lo == "k1")        K1 = args[i + 1].toScalar();
        else if (name_lo == "k2")        K2 = args[i + 1].toScalar();
        else
            throw Error("imcolordiff: unknown option '" + name + "'",
                        0, 0, "imcolordiff", "", "numkit:imcolordiff:unknownNv");
        i += 2;
    }
    if (i < args.size())
        throw Error("imcolordiff: trailing unpaired name-value arg",
                    0, 0, "imcolordiff", "", "numkit:imcolordiff:unpaired");
    outs[0] = imcolordiff(args[0], args[1], standard, is_input_lab,
                          kL, kC, kH, K1, K2, mr);
}

void illumpca_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1)
        throw Error("illumpca: requires (A [, P] [, 'Mask', M])",
                    0, 0, "illumpca", "", "numkit:illumpca:nargin");
    auto *mr = ctx.engine->resource();
    double P = 3.5;       // MATLAB default percentage.
    std::size_t i = 1;
    if (args.size() >= 2 && !args[1].isChar() && !args[1].isString()) {
        P = args[1].toScalar();
        i = 2;
    }
    Value mask = Value::Empty;
    double dummy_norm = 1.0;
    parse_nv_pairs(args, i, /*allow_norm=*/false, mask, dummy_norm,
                   "illumpca");
    outs[0] = illumpca(args[0], P, mask, mr);
}

void illumgray_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1)
        throw Error("illumgray: requires (A [, P] [, 'Mask', M] [, 'Norm', n])",
                    0, 0, "illumgray", "", "numkit:illumgray:nargin");
    auto *mr = ctx.engine->resource();
    std::vector<double> P;
    std::size_t i = 1;
    if (args.size() >= 2 && !args[1].isChar() && !args[1].isString()) {
        const std::size_t n = args[1].numel();
        if (n == 1) P.push_back(args[1].toScalar());
        else if (n == 2) {
            P.push_back(args[1].elemAsDouble(0));
            P.push_back(args[1].elemAsDouble(1));
        } else if (n != 0) {
            throw Error("illumgray: percentile must be scalar or 2-vector",
                        0, 0, "illumgray", "", "numkit:illumgray:percentile");
        }
        i = 2;
    }
    Value mask = Value::Empty;
    double norm_exp = 1.0;
    parse_nv_pairs(args, i, /*allow_norm=*/true, mask, norm_exp,
                   "illumgray");
    outs[0] = illumgray_impl(args[0], P, mask, norm_exp, mr);
}

} // namespace detail
} // namespace numkit::image
