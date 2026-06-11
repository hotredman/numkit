// toolboxes/comm/src/modulation/psk_reg.cpp
//
// Register half of the comm PSK builtins: the CallContext wrappers
// pskmod / pskdemod / dpskmod / dpskdemod that parse the optional
// ini_phase + symbol-order args and delegate to the engine-free compute in
// psk.cpp. library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/comm/modulation/psk.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <string>

namespace numkit::comm {
namespace detail {

namespace {
std::string parse_order(Span<const Value> args, size_t i, const std::string &def) {
    if (i >= args.size()) return def;
    if (!(args[i].isChar() || args[i].isString())) return def;
    auto s = args[i].toString();
    if (s == "bin" || s == "gray") return s;
    return def;
}
}

void pskmod_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pskmod: requires (x, M[, ini_phase, symbol_order])",
                    0, 0, "pskmod", "", "numkit:pskmod:nargin");
    const int M = (int)args[1].toScalar();
    const double ini = (args.size() >= 3 && !args[2].isEmpty()
                        && !(args[2].isChar() || args[2].isString()))
                        ? args[2].toScalar() : 0.0;
    auto order = parse_order(args, args.size() >= 4 ? 3 : 2, "gray");
    outs[0] = pskmod(args[0], M, ini, order, ctx.engine->resource());
}

void pskdemod_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pskdemod: requires (y, M[, ini_phase, symbol_order])",
                    0, 0, "pskdemod", "", "numkit:pskdemod:nargin");
    const int M = (int)args[1].toScalar();
    const double ini = (args.size() >= 3 && !args[2].isEmpty()
                        && !(args[2].isChar() || args[2].isString()))
                        ? args[2].toScalar() : 0.0;
    auto order = parse_order(args, args.size() >= 4 ? 3 : 2, "gray");
    outs[0] = pskdemod(args[0], M, ini, order, ctx.engine->resource());
}

void dpskmod_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("dpskmod: requires (x, M[, phaserot, symbol_order])",
                    0, 0, "dpskmod", "", "numkit:dpskmod:nargin");
    const int M = (int)args[1].toScalar();
    const double rot = (args.size() >= 3 && !args[2].isEmpty()
                        && !(args[2].isChar() || args[2].isString()))
                        ? args[2].toScalar() : 0.0;
    auto order = parse_order(args, args.size() >= 4 ? 3 : 2, "gray");
    outs[0] = dpskmod(args[0], M, rot, order, ctx.engine->resource());
}

void dpskdemod_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("dpskdemod: requires (y, M[, phaserot, symbol_order])",
                    0, 0, "dpskdemod", "", "numkit:dpskdemod:nargin");
    const int M = (int)args[1].toScalar();
    const double rot = (args.size() >= 3 && !args[2].isEmpty()
                        && !(args[2].isChar() || args[2].isString()))
                        ? args[2].toScalar() : 0.0;
    auto order = parse_order(args, args.size() >= 4 ? 3 : 2, "gray");
    outs[0] = dpskdemod(args[0], M, rot, order, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::comm
