// toolboxes/signal/src/descriptive/normalize_reg.cpp
//
// CallContext register half of descriptive/normalize.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/stats/descriptive/descriptive.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "normalize_detail.hpp"
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

void normalize_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("normalize: requires (A [, method])",
                    0, 0, "normalize", "", "numkit:normalize:nargin");
    std::string method = "zscore";
    const Value *param = nullptr;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (!args[1].isChar() && !args[1].isString())
            throw Error("normalize: method must be a string",
                        0, 0, "normalize", "", "numkit:normalize:type");
        method = args[1].toString();
        // Optional method parameter: range bounds / norm-p / scale divisor
        // / center reference. (Previously dropped -> options were ignored.)
        if (args.size() >= 3 && !args[2].isEmpty())
            param = &args[2];
    }
    // MATLAB [N, C, S] = normalize(...): C is the centering value, S the
    // scaling value, with N == (A - C) ./ S.
    Value cVal, sVal;
    Value *cPtr = (nargout >= 2 && outs.size() >= 2) ? &cVal : nullptr;
    Value *sPtr = (nargout >= 3 && outs.size() >= 3) ? &sVal : nullptr;
    outs[0] = normalize(args[0], method, ctx.engine->resource(), param, cPtr, sPtr);
    if (cPtr) outs[1] = std::move(cVal);
    if (sPtr) outs[2] = std::move(sVal);
}

void rescale_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rescale: requires (A [, lo, hi])",
                    0, 0, "rescale", "", "numkit:rescale:nargin");
    double lo = 0.0, hi = 1.0;
    double inputMin = std::numeric_limits<double>::quiet_NaN();
    double inputMax = std::numeric_limits<double>::quiet_NaN();

    // Positional lo/hi come first (only when arg[1] is not a NV-name).
    size_t i = 1;
    if (args.size() >= 2 && !args[1].isChar() && !args[1].isString()) {
        if (!args[1].isEmpty()) lo = args[1].toScalar();
        i = 2;
        if (args.size() >= 3 && !args[2].isChar() && !args[2].isString()) {
            if (!args[2].isEmpty()) hi = args[2].toScalar();
            i = 3;
        }
    }
    // Name-Value: 'InputMin' / 'InputMax' (case-insensitive).
    for (; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("rescale: expected 'InputMin'/'InputMax'",
                        0, 0, "rescale", "", "numkit:rescale:badOpt");
        std::string nm = args[i].toString();
        for (char &c : nm) if (c >= 'A' && c <= 'Z') c = char(c + 32);
        if      (nm == "inputmin") inputMin = args[i + 1].toScalar();
        else if (nm == "inputmax") inputMax = args[i + 1].toScalar();
        else
            throw Error("rescale: name must be 'InputMin' or 'InputMax'",
                        0, 0, "rescale", "", "numkit:rescale:badOpt");
    }
    outs[0] = rescale(args[0], lo, hi, ctx.engine->resource(), inputMin, inputMax);
}

void zscore_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("zscore: requires (A [, flag [, dim]])",
                    0, 0, "zscore", "", "numkit:zscore:nargin");
    int flag = 0, dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty()) flag = static_cast<int>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty()) dim  = static_cast<int>(args[2].toScalar());
    auto *mr = ctx.engine->resource();
    // [Z, MU, SIGMA] = zscore(...): expose the centring mean and scaling std.
    Value mu, sigma;
    outs[0] = zscoreCore(args[0], flag, dim, mr,
                         nargout >= 2 ? &mu : nullptr,
                         nargout >= 3 ? &sigma : nullptr);
    if (nargout >= 2) outs[1] = mu;
    if (nargout >= 3) outs[2] = sigma;
}

} // namespace detail

} // namespace numkit::stats
