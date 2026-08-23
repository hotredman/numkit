#include <numkit/builtin/lang.hpp>
#include <numkit/fs/branding.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/callback_builtin.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/object.hpp>
#include <numkit/core/vm.hpp>
#include <numkit/core/build_info.hpp>
#include <numkit/runtime/runtime.hpp>
#include <numkit/runtime/language/cells/cell.hpp>
#include <numkit/runtime/language/structures/struct.hpp>
#include <numkit/runtime/help/help_catalog.hpp>
#include <numkit/lang/operators/binary_ops.hpp>
#include <numkit/lang/operators/unary_ops.hpp>
#include <numkit/lang/types/types.hpp>
#include <numkit/math/arithmetic/rounding.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace numkit::builtin {
void registerSplitapplyCallbackBuiltin(Engine &engine);
void registerIntegralM(Engine &engine);
void registerCellfunCallbackBuiltin(Engine &engine);
void registerStructfunCallbackBuiltin(Engine &engine);
}


namespace numkit::builtin::detail {
void MException_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void assert_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void coder_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void coder_run_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void error_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void getenv_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void lastwarn_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rethrow_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void runNative_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void setenv_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void system_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void throw_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void warning_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
struct FevalCallbackBuiltin : CallbackBuiltin
{
    std::shared_ptr<VmContinuation> tryStart(Span<const Value> args, std::size_t nargout,
                                             Value *dest, Engine &eng) override
    {
        if (args.empty() || nargout > 1)
            return nullptr;
        if (!eng.isUserCodeHandle(args[0]))
            return nullptr; // name/string/builtin handle → synchronous feval
        std::vector<Value> callArgs(args.begin() + 1, args.end());
        auto cont = std::make_shared<LoopContinuation>();
        cont->handle = args[0];
        cont->n = 1;
        cont->dest = dest;
        cont->makeArgs = [callArgs](std::size_t) -> std::vector<Value> { return callArgs; };
        cont->pack = [](std::vector<Value> &results) -> Value {
            return results.empty() ? Value() : std::move(results[0]);
        };
        cont->results.reserve(1);
        return cont;
    }
};
} // namespace numkit::builtin::detail

namespace numkit::builtin {

// ── Pure C++ Environment & Identifier Implementations ──────────────────────

void setenv(const std::string &name, const std::string &value)
{
    if (name.empty())
        throw std::runtime_error("setenv: variable name cannot be empty");
    if (name.find('=') != std::string::npos)
        throw std::runtime_error("setenv: variable name cannot contain '='");
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    ::setenv(name.c_str(), value.c_str(), 1);
#endif
}

std::string getenv(const std::string &name)
{
    return numkit::envGet(name.c_str());
}

Value getenv(const Value &name, std::pmr::memory_resource *mr)
{
    if (!name.isChar() && !name.isString())
        throw std::runtime_error("getenv: argument must be a variable name");
    return Value::fromString(getenv(name.toString()), mr);
}

const std::vector<std::string> &keywords()
{
    static const std::vector<std::string> kw = {
        "break", "case", "catch", "classdef", "continue", "else",
        "elseif", "end", "for", "function", "global", "if",
        "otherwise", "parfor", "persistent", "return", "spmd",
        "switch", "try", "while"
    };
    return kw;
}

bool iskeyword(const std::string &name)
{
    const auto &kw = keywords();
    return std::find(kw.begin(), kw.end(), name) != kw.end();
}

Value iskeyword(Span<const Value> args, std::pmr::memory_resource *mr)
{
    const auto &kw = keywords();
    if (args.empty()) {
        auto c = Value::cell(kw.size(), 1, mr);
        for (size_t i = 0; i < kw.size(); ++i)
            c.cellAt(i) = Value::fromString(kw[i], mr);
        return c;
    }
    const std::string s = args[0].toString();
    return Value::logicalScalar(iskeyword(s), mr);
}

bool isvarname(const std::string &s)
{
    if (s.empty()) return false;
    if (std::isalpha(static_cast<unsigned char>(s[0])) == 0) return false;
    for (size_t i = 1; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (!(std::isalnum(c) || c == '_')) return false;
    }
    return !iskeyword(s);
}

Value isvarname(const Value &a, std::pmr::memory_resource *mr)
{
    const bool isText = a.isChar() || (a.isString() && a.numel() == 1);
    if (!isText) return Value::logicalScalar(false, mr);
    return Value::logicalScalar(isvarname(a.toString()), mr);
}

Value isvarname(Span<const Value> args, std::pmr::memory_resource *mr)
{
    if (args.empty())
        throw std::runtime_error("isvarname requires 1 argument");
    return isvarname(args[0], mr);
}

void register_lang(Engine &engine) {
// ── env.cpp public-API-backed built-ins ────────────────────────
    engine.registerFunction("setenv",     &::numkit::builtin::detail::setenv_reg);
    engine.registerFunction("getenv",     &::numkit::builtin::detail::getenv_reg);

    
// ── coder.cpp public-API-backed built-ins ──────────────────────
    // coder = transpile (pure C++, works under WASM); coder_run/system/
    // runNative spawn a native process (native-only, throw under WASM).
    engine.registerFunction("coder",      &::numkit::builtin::detail::coder_reg);
    engine.registerFunction("coder_run",  &::numkit::builtin::detail::coder_run_reg);
    engine.registerFunction("system",     &::numkit::builtin::detail::system_reg);
    engine.registerFunction("runNative",  &::numkit::builtin::detail::runNative_reg);

    
// ── programming/errors/diagnostics.cpp public-API-backed built-ins ──────
    engine.registerFunction("error",      &::numkit::builtin::detail::error_reg);
    engine.registerFunction("warning",    &::numkit::builtin::detail::warning_reg);
    engine.registerFunction("lastwarn",   &::numkit::builtin::detail::lastwarn_reg);
    engine.registerFunction("MException", &::numkit::builtin::detail::MException_reg);
    engine.registerFunction("rethrow",    &::numkit::builtin::detail::rethrow_reg);
    engine.registerFunction("throw",      &::numkit::builtin::detail::throw_reg);
    engine.registerFunction("assert",     &::numkit::builtin::detail::assert_reg);

    
// ── Pack 13: function handles ─────────────────────────────────────
    // feval(handle_or_name, args...) — invoke through the engine's
    // existing handle-call path. Accepts either a real function handle
    // or a name/string (str2func it on the fly).
    engine.registerFunction("feval",
                            [](Span<const Value> args, size_t nargout,
                               Span<Value> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error("feval requires at least 1 argument");
                                Value handle;
                                if (args[0].isFuncHandle()) {
                                    handle = args[0];
                                } else if (args[0].isChar() || args[0].isString()) {
                                    handle = Value::funcHandle(args[0].toString(),
                                                                ctx.engine->resource());
                                } else {
                                    throw std::runtime_error(
                                        "feval: first argument must be a function handle or name");
                                }
                                Span<const Value> callArgs(args.data() + 1, args.size() - 1);
                                if (nargout <= 1) {
                                    outs[0] = ctx.engine->callFunctionHandle(handle, callArgs, ctx.env);
                                } else {
                                    auto rs = ctx.engine->callFunctionHandleMulti(
                                        handle, callArgs, nargout, ctx.env);
                                    for (size_t i = 0; i < nargout && i < outs.size() && i < rs.size(); ++i)
                                        outs[i] = std::move(rs[i]);
                                }
                            });
    // feval into a user-code handle runs as a pausable VM frame; a name/string
    // handle or multi-output falls back to the synchronous feval above.
    engine.registerCallbackBuiltin(
        "feval", std::make_shared<builtin::detail::FevalCallbackBuiltin>());

    // __nk_fwd_call__(n, fname, args...) — internal helper for anonymous
    // multi-output forwarding: call `fname` with nargout = n and return the
    // n results packed into a 1×n cell. The compiler lowers a multi-output
    // anonymous body `@(p) g(...)` to `varargout = __nk_fwd_call__(nargout,
    // 'g', ...)`, so the varargout cell expands to as many outputs as the
    // caller's nargout asked for (anonymous nargout forwarding).
    engine.registerFunction(
        "__nk_fwd_call__",
        [](Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
           CallContext &ctx) {
            if (args.size() < 2)
                throw std::runtime_error(
                    "__nk_fwd_call__: requires (n, fname, args...)");
            long long nll = static_cast<long long>(args[0].toScalar());
            size_t n = (nll > 0) ? static_cast<size_t>(nll) : 1;
            Span<const Value> callArgs(args.data() + 2, args.size() - 2);
            std::vector<Value> rs;
            // Resolve the callee the SAME way a direct call does — `findExternal`
            // is import/namespace-aware, so toolbox functions (e.g. compat.median
            // via `import compat.*`) resolve here exactly as `median(...)` would.
            // Only fall back to the handle path for user / anonymous functions.
            bool done = false;
            if (args[1].isChar() || args[1].isString()) {
                const std::string fname = args[1].toString();
                if (const ExternalFunc *ef = ctx.engine->findExternal(fname, ctx.env)) {
                    rs.assign(n, Value());
                    (*ef)(callArgs, n, Span<Value>(rs.data(), n), ctx);
                    done = true;
                }
            }
            if (!done) {
                Value handle = args[1].isFuncHandle()
                                   ? args[1]
                                   : Value::funcHandle(args[1].toString(),
                                                       ctx.engine->resource());
                rs = ctx.engine->callFunctionHandleMulti(handle, callArgs, n, ctx.env);
            }
            Value cell = Value::cell(1, n, ctx.engine->resource());
            for (size_t i = 0; i < n && i < rs.size(); ++i)
                cell.cellAt(i) = std::move(rs[i]);
            outs[0] = std::move(cell);
        });

    // str2func / func2str moved to the runtime layer
    // (runtime/src/function_handles.cpp, registerFunctionHandles) — they
    // operate the engine (anon-source eval / handle minting / name read), not
    // a math toolbox. feval stays here for now: its pausable
    // FevalCallbackBuiltin adapter travels with the callback adapters that
    // move to bundle in step F.
    // iskeyword() / iskeyword(s) — list MATLAB reserved words / test one.
    engine.registerFunction("iskeyword",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            outs[0] = ::numkit::builtin::iskeyword(args, ctx.engine->resource());
        });

    // isvarname(s) — true if s is a valid MATLAB variable name.
    engine.registerFunction("isvarname",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            outs[0] = ::numkit::builtin::isvarname(args, ctx.engine->resource());
        });

}

} // namespace numkit::builtin
