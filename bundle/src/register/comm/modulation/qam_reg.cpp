// toolboxes/comm/src/modulation/qam_reg.cpp
//
// Register half of the comm PAM/QAM builtins: the CallContext wrappers
// pammod / pamdemod / qammod / qamdemod / modnorm that parse the symbol-
// order / UnitAveragePower options and delegate to the engine-free compute
// in qam.cpp. library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/comm/modulation/qam.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <string>

namespace numkit::comm {
namespace detail {

namespace {
// MATLAB R2025b defaults differ by function: pammod/pamdemod default to
// 'bin' (binary symbol mapping); qammod/qamdemod default to 'gray'.
std::string parse_order(Span<const Value> args, size_t start, const char *dflt) {
    for (size_t i = start; i < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            auto s = args[i].toString();
            if (s == "bin" || s == "gray") return s;
        }
    }
    return dflt;
}

bool parse_unit_power(Span<const Value> args, size_t start) {
    for (size_t i = start; i + 1 < args.size(); ++i) {
        if ((args[i].isChar() || args[i].isString())
            && args[i].toString() == "UnitAveragePower")
            return args[i + 1].toScalar() != 0.0;
    }
    return false;
}
}

void pammod_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pammod: requires (x, M[, ini_phase, symbol_order])",
                    0, 0, "pammod", "", "numkit:pammod:nargin");
    const int M = (int)args[1].toScalar();
    const double ini = (args.size() >= 3 && !args[2].isEmpty()
                        && !(args[2].isChar() || args[2].isString()))
                        ? args[2].toScalar() : 0.0;
    auto order = parse_order(args, 2, "bin");
    outs[0] = pammod(args[0], M, ini, order, ctx.engine->resource());
}

void pamdemod_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pamdemod: requires (y, M[, ini_phase, symbol_order])",
                    0, 0, "pamdemod", "", "numkit:pamdemod:nargin");
    const int M = (int)args[1].toScalar();
    const double ini = (args.size() >= 3 && !args[2].isEmpty()
                        && !(args[2].isChar() || args[2].isString()))
                        ? args[2].toScalar() : 0.0;
    auto order = parse_order(args, 2, "bin");
    outs[0] = pamdemod(args[0], M, ini, order, ctx.engine->resource());
}

void qammod_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("qammod: requires (x, M[, symbol_order, 'UnitAveragePower', tf])",
                    0, 0, "qammod", "", "numkit:qammod:nargin");
    const int M = (int)args[1].toScalar();
    auto order = parse_order(args, 2, "gray");
    bool up = parse_unit_power(args, 2);
    outs[0] = qammod(args[0], M, order, up, ctx.engine->resource());
}

void qamdemod_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("qamdemod: requires (y, M[, symbol_order, 'UnitAveragePower', tf])",
                    0, 0, "qamdemod", "", "numkit:qamdemod:nargin");
    const int M = (int)args[1].toScalar();
    auto order = parse_order(args, 2, "gray");
    bool up = parse_unit_power(args, 2);
    outs[0] = qamdemod(args[0], M, order, up, ctx.engine->resource());
}

void modnorm_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("modnorm: requires (ref, type, target)",
                    0, 0, "modnorm", "", "numkit:modnorm:nargin");
    std::string type = "avpow";
    if (args[1].isChar() || args[1].isString()) type = args[1].toString();
    const double target = args[2].toScalar();
    outs[0] = modnorm(args[0], type, target, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::comm
