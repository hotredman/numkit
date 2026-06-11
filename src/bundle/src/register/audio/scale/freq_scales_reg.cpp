// toolboxes/audio/src/scale/freq_scales_reg.cpp
//
// Register half of the frequency-scale conversions: the CallContext
// builtins hz2mel / mel2hz / hz2bark / bark2hz / hz2erb / erb2hz +
// phon2sone / sone2phon (ISO 532-1/532-2 select) that delegate to the
// engine-free compute in freq_scales.cpp. isStandard532_2 is a
// register-side parser. library.cpp forward-declares + registers these.
//
// Compute reconciled to the canonical mr-last API (matching the header)
// as part of this split. Phase 2b — see project_layering_refactor memory.

#include <numkit/audio/scale/freq_scales.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cctype>
#include <string>

namespace numkit::audio {
namespace detail {

#define NK_ELEM_REG(FN)                                                          \
    void FN##_reg(Span<const Value> args, size_t /*nargout*/,                    \
                  Span<Value> outs, CallContext &ctx)                            \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#FN ": requires (x)",                                    \
                        0, 0, #FN, "", "numkit:" #FN ":nargin");                      \
        outs[0] = FN(args[0], ctx.engine->resource());                           \
    }

NK_ELEM_REG(hz2mel)
NK_ELEM_REG(mel2hz)
NK_ELEM_REG(hz2bark)
NK_ELEM_REG(bark2hz)
NK_ELEM_REG(hz2erb)
NK_ELEM_REG(erb2hz)

#undef NK_ELEM_REG

// phon2sone / sone2phon take an optional second arg = "ISO 532-1"
// (default) or "ISO 532-2". Cycle M added the ISO 532-2 path.
namespace {
bool isStandard532_2(const Value &v)
{
    if (v.type() == ValueType::CHAR || v.type() == ValueType::STRING) {
        std::string s = v.toString();
        // Case-insensitive compare.
        std::string lower = s;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                        [](unsigned char c) { return std::tolower(c); });
        return (lower == "iso 532-2");
    }
    return false;
}
} // anon

void phon2sone_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("phon2sone: requires (phon [, standard])",
                    0, 0, "phon2sone", "", "numkit:phon2sone:nargin");
    bool iso532_2 = (args.size() >= 2) && isStandard532_2(args[1]);
    outs[0] = phon2sone(args[0], iso532_2, ctx.engine->resource());
}

void sone2phon_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sone2phon: requires (sone [, standard])",
                    0, 0, "sone2phon", "", "numkit:sone2phon:nargin");
    bool iso532_2 = (args.size() >= 2) && isStandard532_2(args[1]);
    outs[0] = sone2phon(args[0], iso532_2, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::audio
