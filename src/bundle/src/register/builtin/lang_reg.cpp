// src/bundle/src/register/builtin/lang_reg.cpp

#include <numkit/builtin/lang.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/callback_builtin.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/core/vm.hpp>
#include <numkit/runtime/runtime.hpp>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace numkit::builtin::detail {
void coder_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void coder_run_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void runNative_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void system_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
struct FevalCallbackBuiltin : CallbackBuiltin
{
    std::shared_ptr<VmContinuation> tryStart(Span<const Value> args, std::size_t nargout,
                                             Value *dest, Engine &eng) override
    {
        if (args.empty() || nargout > 1)
            return nullptr;
        if (!eng.isUserCodeHandle(args[0]))
            return nullptr; // name/string/builtin handle -> synchronous feval
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


namespace numkit::bundle::builtin {

void register_lang(Engine &engine) {
    using namespace ::numkit::builtin::detail;

// ── coder.cpp public-API-backed built-ins ──────────────────────
    // coder = transpile (pure C++, works under WASM); coder_run/system/
    // runNative spawn a native process (native-only, throw under WASM).
    engine.registerFunction("coder",      &::numkit::builtin::detail::coder_reg);
    engine.registerFunction("coder_run",  &::numkit::builtin::detail::coder_run_reg);
    engine.registerFunction("system",     &::numkit::builtin::detail::system_reg);
    engine.registerFunction("runNative",  &::numkit::builtin::detail::runNative_reg);

    
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
        "feval", std::make_shared<::numkit::builtin::detail::FevalCallbackBuiltin>());

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
            // resolve here exactly as `median(...)` would.
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


} // namespace numkit::bundle::builtin
