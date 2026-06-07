// libs/image/src/quality/quality_reg.cpp
//
// Register half of the image quality builtins: the CallContext wrappers
// delegating to the engine-free compute in quality.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/image/quality/quality.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::image {

namespace detail {

void immse_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("immse: requires (A, B)", 0, 0, "immse", "",
                    "numkit:immse:nargin");
    outs[0] = immse(args[0], args[1], ctx.engine->resource());
}

void psnr_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("psnr: requires (A, B[, peak])", 0, 0, "psnr", "",
                    "numkit:psnr:nargin");
    const double peak = (args.size() >= 3 && !args[2].isEmpty())
                        ? args[2].toScalar() : std::nan("");
    outs[0] = psnr(args[0], args[1], peak, ctx.engine->resource());
}

void ssim_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ssim: requires (A, B)", 0, 0, "ssim", "",
                    "numkit:ssim:nargin");
    outs[0] = ssim(args[0], args[1], ctx.engine->resource());
}

void mean2_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("mean2: requires (A)", 0, 0, "mean2", "",
                    "numkit:mean2:nargin");
    outs[0] = mean2(args[0], ctx.engine->resource());
}

void std2_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("std2: requires (A)", 0, 0, "std2", "",
                    "numkit:std2:nargin");
    outs[0] = std2(args[0], ctx.engine->resource());
}

void corr2_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("corr2: requires (A, B)", 0, 0, "corr2", "",
                    "numkit:corr2:nargin");
    outs[0] = corr2(args[0], args[1], ctx.engine->resource());
}

void multissim3_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("multissim3: requires (V, Vref [, NV...])",
                    0, 0, "multissim3", "",
                    "numkit:multissim3:nargin");
    auto *mr = ctx.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    int num_scales = 5;
    std::vector<double> scale_weights;
    double sigma = 1.5;
    double dynamic_range = -1.0;

    std::size_t i = 2;
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("multissim3: expected NV-pair name string",
                        0, 0, "multissim3", "", "numkit:multissim3:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "numscales") {
            num_scales = static_cast<int>(args[i + 1].toScalar());
        } else if (nlo == "scaleweights") {
            const Value &v = args[i + 1];
            const std::size_t N = v.numel();
            scale_weights.resize(N);
            for (std::size_t k = 0; k < N; ++k)
                scale_weights[k] = v.elemAsDouble(k);
        } else if (nlo == "sigma") {
            sigma = args[i + 1].toScalar();
        } else if (nlo == "dynamicrange") {
            dynamic_range = args[i + 1].toScalar();
        } else {
            throw Error("multissim3: unknown option '" + name + "'",
                        0, 0, "multissim3", "",
                        "numkit:multissim3:unknownNv");
        }
        i += 2;
    }
    std::vector<Value> qmaps;
    outs[0] = multissim3(args[0], args[1], num_scales, scale_weights,
                         sigma, dynamic_range,
                         nargout >= 2 ? &qmaps : nullptr, mr);
    if (nargout >= 2 && outs.size() >= 2) {
        Value cell = Value::cell(1, qmaps.size(), mr);
        for (std::size_t k = 0; k < qmaps.size(); ++k)
            cell.cellAt(k) = std::move(qmaps[k]);
        outs[1] = std::move(cell);
    }
}

void multissim_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("multissim: requires (I, Iref [, NV...])",
                    0, 0, "multissim", "", "numkit:multissim:nargin");
    auto *mr = ctx.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    int num_scales = 5;
    std::vector<double> scale_weights;
    double sigma = 1.5;
    double dynamic_range = -1.0;

    std::size_t i = 2;
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("multissim: expected NV-pair name string",
                        0, 0, "multissim", "", "numkit:multissim:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "numscales") {
            num_scales = static_cast<int>(args[i + 1].toScalar());
        } else if (nlo == "scaleweights") {
            const Value &v = args[i + 1];
            const std::size_t N = v.numel();
            scale_weights.resize(N);
            for (std::size_t k = 0; k < N; ++k)
                scale_weights[k] = v.elemAsDouble(k);
        } else if (nlo == "sigma") {
            sigma = args[i + 1].toScalar();
        } else if (nlo == "dynamicrange") {
            dynamic_range = args[i + 1].toScalar();
        } else {
            throw Error("multissim: unknown option '" + name + "'",
                        0, 0, "multissim", "",
                        "numkit:multissim:unknownNv");
        }
        i += 2;
    }
    std::vector<Value> qmaps;
    outs[0] = multissim(args[0], args[1], num_scales, scale_weights,
                        sigma, dynamic_range,
                        nargout >= 2 ? &qmaps : nullptr, mr);
    if (nargout >= 2 && outs.size() >= 2) {
        // Return cell array of per-scale maps.
        Value cell = Value::cell(1, qmaps.size(), mr);
        for (std::size_t k = 0; k < qmaps.size(); ++k)
            cell.cellAt(k) = std::move(qmaps[k]);
        outs[1] = std::move(cell);
    }
}

} // namespace detail
} // namespace numkit::image
