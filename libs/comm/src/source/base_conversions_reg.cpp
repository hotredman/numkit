// libs/comm/src/source/base_conversions_reg.cpp
//
// Register half of the comm base-conversion builtins: the CallContext
// wrappers (bit2int / int2bit / bi2de / de2bi / vec2mat) that parse args
// and delegate to the engine-free compute in base_conversions.cpp.
// The 'left-msb'/'right-msb' flag parser is register-side (only the legacy
// bi2de/de2bi string-option forms use it). library.cpp forward-declares +
// registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/comm/source/base_conversions.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <string>

namespace numkit::comm {

namespace {

// Detect 'left-msb' vs 'right-msb' option (MATLAB style for bi2de/de2bi).
// Returns true if MSB-first, false if LSB-first.  bi2de defaults to
// LSB-first ('right-msb'), de2bi same.
bool parseMsbFlag(const Value &v, const char *who)
{
    if (!v.isChar() && !v.isString())
        throw Error(std::string(who) + ": flag must be 'left-msb' or 'right-msb'",
                    0, 0, who, "", std::string("numkit:") + who + ":BadFlag");
    std::string s = v.toString();
    if (s == "left-msb")  return true;
    if (s == "right-msb") return false;
    throw Error(std::string(who) + ": unknown flag '" + s + "'",
                0, 0, who, "", std::string("numkit:") + who + ":BadFlag");
}

} // namespace

namespace detail {

void bit2int_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("bit2int: requires (b, n [, msbfirst])",
                    0, 0, "bit2int", "", "numkit:bit2int:nargin");
    const int n = static_cast<int>(args[1].toScalar());
    bool msb = true;
    if (args.size() >= 3) msb = (args[2].toScalar() != 0.0);
    outs[0] = bit2int(args[0], n, msb, ctx.engine->resource());
}

void int2bit_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("int2bit: requires (d, n [, msbfirst])",
                    0, 0, "int2bit", "", "numkit:int2bit:nargin");
    const int n = static_cast<int>(args[1].toScalar());
    bool msb = true;
    if (args.size() >= 3) msb = (args[2].toScalar() != 0.0);
    outs[0] = int2bit(args[0], n, msb, ctx.engine->resource());
}

void bi2de_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bi2de: requires (b [, base] [, flag])",
                    0, 0, "bi2de", "", "numkit:bi2de:nargin");
    int base = 2;
    bool msb = false;  // default 'right-msb' = LSB-first
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString())
            msb = parseMsbFlag(args[i], "bi2de");
        else if (!args[i].isEmpty())  // empty [] base -> keep default 2
            base = static_cast<int>(args[i].toScalar());
    }
    if (base < 2)
        throw Error("bi2de: base must be >= 2",
                    0, 0, "bi2de", "", "numkit:bi2de:BadBase");
    outs[0] = bi2de(args[0], base, msb, ctx.engine->resource());
}

void de2bi_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("de2bi: requires (d [, n] [, base] [, flag])",
                    0, 0, "de2bi", "", "numkit:de2bi:nargin");
    int n = -1;       // -1 = auto
    int base = 2;
    bool msb = false; // default 'right-msb'
    int numericFound = 0;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString())
            msb = parseMsbFlag(args[i], "de2bi");
        else if (numericFound == 0) {
            // First positional numeric is n. An EMPTY [] means "auto width"
            // (MATLAB: de2bi(d, [], base)) — keep n = -1, don't toScalar([]).
            if (!args[i].isEmpty()) n = static_cast<int>(args[i].toScalar());
            ++numericFound;
        } else {
            if (!args[i].isEmpty()) base = static_cast<int>(args[i].toScalar());
            ++numericFound;
        }
    }
    outs[0] = de2bi(args[0], n, base, msb, ctx.engine->resource());
}

void vec2mat_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("vec2mat: requires (v, n [, padval])",
                    0, 0, "vec2mat", "", "numkit:vec2mat:nargin");
    const int n = static_cast<int>(args[1].toScalar());
    double padval = 0.0;
    if (args.size() >= 3) padval = args[2].toScalar();
    auto [m, pad] = vec2mat(args[0], n, padval, ctx.engine->resource());
    outs[0] = m;
    if (nargout >= 2 && outs.size() >= 2)
        outs[1] = Value::scalar(static_cast<double>(pad), ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::comm
