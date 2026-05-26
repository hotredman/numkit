// libs/ode/src/options.cpp
//
// odeset / odeget — options struct for the ODE solvers.
//
// Reference: Shampine & Reichelt, "The MATLAB ODE Suite", 1997.

#include <numkit/ode/options.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>

namespace numkit::ode {

namespace {

// Canonical option names (MATLAB R2025b odeset). Case stays as MATLAB
// writes it; the lookup is case-insensitive.
const char *const kOptionNames[] = {
    "AbsTol", "BDF", "Events", "InitialSlope", "InitialStep",
    "Jacobian", "JPattern", "Mass", "MassSingular", "MaxOrder",
    "MaxStep", "MStateDependence", "MvPattern", "NonNegative",
    "NormControl", "OutputFcn", "OutputSel", "Refine", "RelTol",
    "Stats", "Vectorized",
};
constexpr std::size_t kNumOptions = sizeof(kOptionNames) / sizeof(kOptionNames[0]);

std::string to_lower(std::string s)
{
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Map case-insensitive name → canonical capitalisation; return empty
// string if not a recognised option.
std::string canonical_name(const std::string &raw)
{
    const std::string lower = to_lower(raw);
    for (std::size_t i = 0; i < kNumOptions; ++i) {
        if (to_lower(kOptionNames[i]) == lower) return std::string(kOptionNames[i]);
    }
    return {};
}

// Build the default options struct (all fields = []).
Value default_struct(std::pmr::memory_resource *mr)
{
    Value s = Value::structure(mr);
    for (std::size_t i = 0; i < kNumOptions; ++i) {
        s.field(kOptionNames[i]) = Value::Empty;
    }
    return s;
}

} // anonymous

Value odeset(const Value *args, std::size_t nargs,
             std::pmr::memory_resource *mr)
{
    // Start from defaults, optionally merge in an existing struct, then
    // process name-value pairs.
    Value out = default_struct(mr);
    std::size_t i = 0;
    // First-arg-is-struct cases.
    while (i < nargs && args[i].isStruct()) {
        const Value &src = args[i];
        for (std::size_t k = 0; k < kNumOptions; ++k) {
            const char *name = kOptionNames[k];
            if (src.hasField(name)) {
                out.field(name) = src.field(name);
            }
        }
        ++i;
    }
    // Name-value pairs.
    while (i + 1 < nargs) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("odeset: expected a string option name at arg "
                      + std::to_string(i + 1),
                        0, 0, "odeset", "", "m:odeset:badArg");
        const std::string name = canonical_name(args[i].toString());
        if (name.empty())
            throw Error("odeset: unknown option '" + args[i].toString() + "'",
                        0, 0, "odeset", "", "m:odeset:badName");
        out.field(name.c_str()) = args[i + 1];
        i += 2;
    }
    if (i < nargs)
        throw Error("odeset: trailing unmatched argument at position "
                  + std::to_string(i + 1),
                    0, 0, "odeset", "", "m:odeset:unpaired");
    return out;
}

Value odeget(const Value &opts, const Value &name,
             const Value &default_v, std::pmr::memory_resource *mr)
{
    if (!opts.isStruct())
        throw Error("odeget: first argument must be a struct",
                    0, 0, "odeget", "", "m:odeget:notStruct");
    if (!name.isChar() && !name.isString())
        throw Error("odeget: name must be a string",
                    0, 0, "odeget", "", "m:odeget:badName");
    const std::string canon = canonical_name(name.toString());
    if (canon.empty())
        throw Error("odeget: unknown option '" + name.toString() + "'",
                    0, 0, "odeget", "", "m:odeget:badName");
    if (opts.hasField(canon.c_str())) {
        const Value &v = opts.field(canon.c_str());
        if (!v.isEmpty()) return v;
    }
    if (!default_v.isEmpty()) return default_v;
    return Value::Empty;
}

// ── Engine adapters ─────────────────────────────────────────────────

namespace detail {

void odeset_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    outs[0] = odeset(args.data(), args.size(), ctx.engine->resource());
}

void odeget_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("odeget: requires (opts, name[, default])",
                    0, 0, "odeget", "", "m:odeget:nargin");
    auto *mr = ctx.engine->resource();
    const Value def = (args.size() > 2) ? args[2] : Value::Empty;
    outs[0] = odeget(args[0], args[1], def, mr);
}

} // namespace detail
} // namespace numkit::ode
