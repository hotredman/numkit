// toolboxes/stats/src/descriptive/sample_corr_reg.cpp
//
// CallContext register half of descriptive/sample_corr.cpp (autocorr / crosscorr).
// Engine-coupled glue: parses the MATLAB call forms (positional NumLags or the
// 'NumLags'/'NumSTD' name-value pairs) and threads the 3 outputs by nargout.
#include <numkit/core/engine.hpp>
#include <numkit/stats/descriptive/descriptive.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <tuple>
#include <utility>

namespace numkit::stats {

namespace detail {

namespace {

// Parse optional NumLags / NumSTD from args starting at `start`. A numeric arg
// at `start` is positional NumLags; 'NumLags'/'NumSTD' name-value pairs (any
// case) override. Leaves numLags = -1 (→ default) when unspecified.
void parseCorrOpts(Span<const Value> args, size_t start, int &numLags, double &numSTD)
{
    for (size_t i = start; i < args.size(); ++i) {
        if (args[i].isChar()) {
            std::string opt = args[i].toString();
            std::transform(opt.begin(), opt.end(), opt.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (i + 1 < args.size()) {
                if (opt == "numlags")      numLags = static_cast<int>(args[i + 1].toScalar());
                else if (opt == "numstd")  numSTD  = args[i + 1].toScalar();
                ++i;  // consume the value
            }
        } else if (i == start) {
            numLags = static_cast<int>(args[i].toScalar());  // positional NumLags
        }
    }
}

} // namespace

void autocorr_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("autocorr: requires at least one argument",
                    0, 0, "autocorr", "", "numkit:autocorr:nargin");
    int    numLags = -1;
    double numSTD  = 2.0;
    parseCorrOpts(args, 1, numLags, numSTD);

    auto [acf, lags, bounds] = autocorr(args[0], numLags, numSTD, ctx.engine->resource());
    outs[0] = std::move(acf);
    if (nargout > 1) outs[1] = std::move(lags);
    if (nargout > 2) outs[2] = std::move(bounds);
}

void crosscorr_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("crosscorr: requires (y1, y2[, NumLags])",
                    0, 0, "crosscorr", "", "numkit:crosscorr:nargin");
    int    numLags = -1;
    double numSTD  = 2.0;
    parseCorrOpts(args, 2, numLags, numSTD);

    auto [xcf, lags, bounds] =
        crosscorr(args[0], args[1], numLags, numSTD, ctx.engine->resource());
    outs[0] = std::move(xcf);
    if (nargout > 1) outs[1] = std::move(lags);
    if (nargout > 2) outs[2] = std::move(bounds);
}

void parcorr_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("parcorr: requires at least one argument",
                    0, 0, "parcorr", "", "numkit:parcorr:nargin");
    int    numLags = -1;
    double numSTD  = 2.0;
    parseCorrOpts(args, 1, numLags, numSTD);

    auto [pacf, lags, bounds] = parcorr(args[0], numLags, numSTD, ctx.engine->resource());
    outs[0] = std::move(pacf);
    if (nargout > 1) outs[1] = std::move(lags);
    if (nargout > 2) outs[2] = std::move(bounds);
}

} // namespace detail

} // namespace numkit::stats
