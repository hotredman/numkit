// toolboxes/comm/src/modulation/analog_reg.cpp
//
// Register half of the comm analog-modulation builtins: the CallContext
// wrappers pmmod / ammod / fmmod / mskmod / ssbmod that parse the carrier /
// deviation / phase / sideband options and delegate to the engine-free
// compute in analog.cpp. library.cpp forward-declares + registers these by
// name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/comm/modulation/analog.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <cctype>
#include <string>

namespace numkit::comm {
namespace detail {

void pmmod_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("pmmod: requires (x, Fc, Fs, phasedev [, ini_phase])",
                    0, 0, "pmmod", "", "numkit:pmmod:nargin");
    const double fc = args[1].toScalar();
    const double fs = args[2].toScalar();
    const double phasedev = args[3].toScalar();
    double ini_phase = 0.0;
    if (args.size() >= 5 && !args[4].isEmpty()) {
        ini_phase = args[4].toScalar();
    }
    outs[0] = pmmod(args[0], fc, fs, phasedev, ini_phase,
                    ctx.engine->resource());
}

void ammod_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ammod: requires (x, Fc, Fs [, ini_phase [, carr_amp]])",
                    0, 0, "ammod", "", "numkit:ammod:nargin");
    const double fc = args[1].toScalar();
    const double fs = args[2].toScalar();
    double ini_phase = 0.0;
    double carr_amp  = 0.0;
    if (args.size() >= 4 && !args[3].isEmpty())
        ini_phase = args[3].toScalar();
    if (args.size() >= 5 && !args[4].isEmpty())
        carr_amp = args[4].toScalar();
    outs[0] = ammod(args[0], fc, fs, ini_phase, carr_amp,
                    ctx.engine->resource());
}

void fmmod_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("fmmod: requires (x, Fc, Fs, freqdev [, ini_phase])",
                    0, 0, "fmmod", "", "numkit:fmmod:nargin");
    const double fc = args[1].toScalar();
    const double fs = args[2].toScalar();
    const double freqdev = args[3].toScalar();
    double ini_phase = 0.0;
    if (args.size() >= 5 && !args[4].isEmpty())
        ini_phase = args[4].toScalar();
    outs[0] = fmmod(args[0], fc, fs, freqdev, ini_phase,
                    ctx.engine->resource());
}

void mskmod_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mskmod: requires (x, nSamp [, ini_phase])",
                    0, 0, "mskmod", "", "numkit:mskmod:nargin");
    const int nSamp = static_cast<int>(args[1].toScalar());
    double ini_phase = 0.0;
    if (args.size() >= 3 && !args[2].isEmpty())
        ini_phase = args[2].toScalar();
    outs[0] = mskmod(args[0], nSamp, ini_phase, ctx.engine->resource());
}

void ssbmod_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ssbmod: requires (x, Fc, Fs [, ini_phase [, 'upper']])",
                    0, 0, "ssbmod", "", "numkit:ssbmod:nargin");
    const double fc = args[1].toScalar();
    const double fs = args[2].toScalar();
    double ini_phase = 0.0;
    if (args.size() >= 4 && !args[3].isEmpty())
        ini_phase = args[3].toScalar();
    bool upper = false;
    if (args.size() >= 5 && !args[4].isEmpty()) {
        if (!args[4].isChar() && !args[4].isString())
            throw Error("ssbmod: method must be a string ('upper')",
                        0, 0, "ssbmod", "", "numkit:ssbmod:InvStr");
        std::string m = args[4].toString();
        // Match MATLAB behaviour: any string containing 'up' selects USB.
        for (auto &c : m) c = static_cast<char>(std::tolower(c));
        upper = (m.find("up") != std::string::npos);
        if (!upper && !m.empty())
            throw Error("ssbmod: method must be 'upper'",
                        0, 0, "ssbmod", "", "numkit:ssbmod:InvStr");
    }
    outs[0] = ssbmod(args[0], fc, fs, ini_phase, upper,
                     ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::comm
