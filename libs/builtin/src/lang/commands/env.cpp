// libs/builtin/src/lang/commands/env.cpp
//
// Process-environment builtins (setenv / getenv).

#include <numkit/builtin/lang/commands/env.hpp>
#include <numkit/builtin/library.hpp>

#include <numkit/core/branding.hpp>     // for envGet
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <cstdlib>
#include <string>

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// setenv / getenv — MATLAB-compatible process-environment access.
// Session-scoped (does not persist across restarts), same as MATLAB.
//
//   setenv(name)           → set name to empty string
//   setenv(name, value)    → set name to value
//   v = getenv(name)       → value, or '' if not set
// ════════════════════════════════════════════════════════════════════════

void setenv(Span<const Value> args)
{
    if (args.empty() || !args[0].isChar())
        throw Error("setenv: first argument must be a variable name");
    std::string name = args[0].toString();
    if (name.empty())
        throw Error("setenv: variable name cannot be empty");
    if (name.find('=') != std::string::npos)
        throw Error("setenv: variable name cannot contain '='");
    std::string value;
    if (args.size() >= 2) {
        if (!args[1].isChar())
            throw Error("setenv: value must be a char array");
        value = args[1].toString();
    }
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    ::setenv(name.c_str(), value.c_str(), 1);
#endif
}

Value getenv(Allocator &alloc, Span<const Value> args)
{
    if (args.empty() || !args[0].isChar())
        throw Error("getenv: argument must be a variable name");
    return Value::fromString(numkit::envGet(args[0].toString().c_str()), &alloc);
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void setenv_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    (void)nargout;
    (void)outs;
    (void)ctx;
    setenv(args);
}

void getenv_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    (void)nargout;
    outs[0] = getenv(ctx.engine->allocator(), args);
}

} // namespace detail

} // namespace numkit::builtin
