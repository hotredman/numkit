// toolboxes/signal/src/filter_analysis/frequency_response_reg.cpp
//
// Register half of the signal frequency_response builtins: the CallContext wrappers
// delegating to the engine-free compute in frequency_response.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/signal/filter_analysis/frequency_response.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::signal {

namespace detail {

void freqz_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("freqz: requires at least 2 arguments",
                     0, 0, "freqz", "", "numkit:freqz:nargin");
    // Parse the trailing args: 'whole' (string, any position) and up to two
    // numerics after b,a — the first is n (npts), the second is fs (the
    // freqz(b,a,n,fs) / freqz(b,a,n,'whole',fs) sample-rate form).
    size_t npts = 512;
    double fs   = 0.0;
    bool   whole = false;
    int    numericSeen = 0;
    for (size_t i = 2; i < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            std::string s = args[i].toString();
            for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (s == "whole") whole = true;
        } else if (!args[i].isEmpty()) {
            if (numericSeen == 0)      npts = static_cast<size_t>(args[i].toScalar());
            else if (numericSeen == 1) fs   = args[i].toScalar();
            ++numericSeen;
        }
    }

    auto [H, W] = freqz(args[0], args[1], npts, ctx.engine->resource(), whole, fs);
    outs[0] = std::move(H);
    if (nargout > 1)
        outs[1] = std::move(W);
}

void phasez_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("phasez: requires at least 2 arguments",
                     0, 0, "phasez", "", "numkit:phasez:nargin");
    // n (1st numeric after b,a) and optional fs (2nd) — phasez(b,a,n,fs).
    size_t npts = 512;
    double fs = 0.0;
    int numericSeen = 0;
    for (size_t i = 2; i < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString() || args[i].isEmpty()) continue;
        if (numericSeen == 0)      npts = static_cast<size_t>(args[i].toScalar());
        else if (numericSeen == 1) fs   = args[i].toScalar();
        ++numericSeen;
    }
    auto [phi, W] = phasez(args[0], args[1], npts, ctx.engine->resource(), fs);
    outs[0] = std::move(phi);
    if (nargout > 1) outs[1] = std::move(W);
}

void grpdelay_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("grpdelay: requires at least 2 arguments",
                     0, 0, "grpdelay", "", "numkit:grpdelay:nargin");
    // n (1st numeric after b,a) and optional fs (2nd) — grpdelay(b,a,n,fs).
    size_t npts = 512;
    double fs = 0.0;
    int numericSeen = 0;
    for (size_t i = 2; i < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString() || args[i].isEmpty()) continue;
        if (numericSeen == 0)      npts = static_cast<size_t>(args[i].toScalar());
        else if (numericSeen == 1) fs   = args[i].toScalar();
        ++numericSeen;
    }
    auto [gd, W] = grpdelay(args[0], args[1], npts, ctx.engine->resource(), fs);
    outs[0] = std::move(gd);
    if (nargout > 1) outs[1] = std::move(W);
}

} // namespace detail

} // namespace numkit::signal
