// libs/builtin/src/MStdEnv.cpp
//
// Process-environment builtins (setenv / getenv). Split off from
// MStdIO.cpp in Phase 6c.8.6.

#include <numkit/m/builtin/lang/commands/env.hpp>
#include <numkit/m/builtin/MStdLibrary.hpp>

#include <numkit/m/core/MBranding.hpp>     // for envGet
#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include <cstdlib>
#include <string>

namespace numkit::m::builtin {

// ════════════════════════════════════════════════════════════════════════
// setenv / getenv — MATLAB-compatible process-environment access.
// Session-scoped (does not persist across restarts), same as MATLAB.
//
//   setenv(name)           → set name to empty string
//   setenv(name, value)    → set name to value
//   v = getenv(name)       → value, or '' if not set
// ════════════════════════════════════════════════════════════════════════

void setenv(Span<const MValue> args)
{
    if (args.empty() || !args[0].isChar())
        throw MError("setenv: first argument must be a variable name");
    std::string name = args[0].toString();
    if (name.empty())
        throw MError("setenv: variable name cannot be empty");
    if (name.find('=') != std::string::npos)
        throw MError("setenv: variable name cannot contain '='");
    std::string value;
    if (args.size() >= 2) {
        if (!args[1].isChar())
            throw MError("setenv: value must be a char array");
        value = args[1].toString();
    }
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    ::setenv(name.c_str(), value.c_str(), 1);
#endif
}

MValue getenv(Allocator &alloc, Span<const MValue> args)
{
    if (args.empty() || !args[0].isChar())
        throw MError("getenv: argument must be a variable name");
    return MValue::fromString(numkit::m::envGet(args[0].toString().c_str()), &alloc);
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void setenv_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    (void)nargout;
    (void)outs;
    (void)ctx;
    setenv(args);
}

void getenv_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    (void)nargout;
    outs[0] = getenv(ctx.engine->allocator(), args);
}

} // namespace detail

} // namespace numkit::m::builtin
