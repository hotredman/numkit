// libs/audio/src/features/pitch_harmonics_reg.cpp
//
// Register half of the pitch / harmonicRatio builtins: the CallContext
// wrappers (Method / Range name-value parsing, method dispatch) that
// delegate to the engine-free compute in pitch_harmonics.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/audio/features/pitch_harmonics.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cctype>
#include <string>

namespace numkit::audio {
namespace detail {

// pitch dispatches on optional Method arg. Recognized:
//   'NCF' (default, cycle E)
//   'CEP' (cycle K)
//   'PEF' (cycle K-2)
// 'LHS'/'SRH' deferred — fall through to NCF for now.
// Cycle L (partial) added 'Range' NV pair → minF/maxF override default
// [50, 400] Hz pitch search range.
// Calling convention supports Name-Value pairs:
//   pitch(x, fs)                                — NCF default
//   pitch(x, fs, 'Method', 'CEP')
//   pitch(x, fs, 'Range', [80 250])             — restrict to speech
//   pitch(x, fs, 'Method', 'PEF', 'Range', [...])
void pitch_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pitch: requires (x, fs)",
                    0, 0, "pitch", "", "numkit:pitch:nargin");
    std::string method = "NCF";
    double minF = 50.0, maxF = 400.0;
    // Parse Name-Value pairs starting at args[2].
    for (size_t i = 2; i + 1 < args.size(); i += 2) {
        if (args[i].type() != ValueType::CHAR && args[i].type() != ValueType::STRING)
            continue;
        std::string name = args[i].toString();
        std::transform(name.begin(), name.end(), name.begin(),
                        [](unsigned char c) { return std::tolower(c); });
        if (name == "method") {
            std::string m = args[i + 1].toString();
            std::transform(m.begin(), m.end(), m.begin(),
                            [](unsigned char c) { return std::toupper(c); });
            method = m;
        } else if (name == "range") {
            const Value &r = args[i + 1];
            if (r.numel() != 2)
                throw Error("pitch: Range must be a 2-element vector [lo hi]",
                            0, 0, "pitch", "", "numkit:pitch:BadRange");
            const double lo = r.elemAsDouble(0);
            const double hi = r.elemAsDouble(1);
            if (!(lo > 0.0 && hi > lo))
                throw Error("pitch: Range must satisfy 0 < Range(1) < Range(2)",
                            0, 0, "pitch", "", "numkit:pitch:BadRange");
            minF = lo;
            maxF = hi;
        }
    }
    const double fs = args[1].toScalar();
    if (method == "CEP")
        outs[0] = pitchCEP(args[0], fs, minF, maxF, ctx.engine->resource());
    else if (method == "PEF")
        outs[0] = pitchPEF(args[0], fs, minF, maxF, ctx.engine->resource());
    else if (method == "LHS")
        outs[0] = pitchLHS(args[0], fs, minF, maxF, ctx.engine->resource());
    else if (method == "SRH")
        outs[0] = pitchSRH(args[0], fs, minF, maxF, ctx.engine->resource());
    else
        outs[0] = pitch(args[0], fs, minF, maxF, ctx.engine->resource());
}

void harmonicRatio_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("harmonicRatio: requires (x, fs)",
                    0, 0, "harmonicRatio", "", "numkit:harmonicRatio:nargin");
    outs[0] = harmonicRatio(args[0], args[1].toScalar(), ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::audio
