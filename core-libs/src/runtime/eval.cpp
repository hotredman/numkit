// core-libs/src/runtime/eval.cpp
//
// Language-runtime eval-family builtins (run / eval / evalin), extracted from
// toolboxes/builtin's registerWorkspaceBuiltins into the core-libs layer
// (L2, engine-coupled scripting runtime — NOT a math/io toolbox). The shared
// resolveEvalScope helper moved with them. Behaviour is unchanged; only the
// owning translation unit / layer differs. registerEvalFamily is composed by
// installRuntimeLibrary (runtime.cpp), which bundle/installStandardLibrary calls.
#include <numkit/corelibs/runtime.hpp>

#include <numkit/core/callback_builtin.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <cstdlib>
#include <stdexcept>
#include <string>

namespace numkit::corelibs {

void registerEvalFamily(Engine &engine)
{
    // Legacy compat: env var NUMKIT_LEGACY_EVAL_SCOPE=1 reverts to the
    // pre-2026-05 behaviour where eval/run from inside a function
    // leaked variables and imports into workspaceEnv. Provided as an
    // escape hatch for code that depended on the old (buggy) scope.
    auto resolveEvalScope = [](CallContext &ctx) -> Environment * {
        const char *legacy = std::getenv("NUMKIT_LEGACY_EVAL_SCOPE");
        if (legacy && legacy[0] == '1' && legacy[1] == '\0')
            return &ctx.engine->workspaceEnv();
        return ctx.env;
    };

    engine.registerFunction(
        "run", [resolveEvalScope](Span<const Value> args, size_t,
                                   Span<Value> outs, CallContext &ctx) {
            if (args.empty() || !args[0].isChar())
                throw std::runtime_error("run requires a string filename");
            std::string p = args[0].toString();
            auto rp = ctx.engine->resolvePath(p);
            if (!rp.fs || !rp.fs->exists(rp.path))
                throw std::runtime_error("run: file not found: " + p);
            std::string content = rp.fs->readFile(rp.path);

            // Push script origin for the duration of the run so that
            // sibling .m files in the same directory resolve without
            // addpath. The dir is extracted from the resolved path
            // (rp.path is the script's full path inside rp.fs).
            // Root-level files ("/foo.m") need scriptDir = "/" — an
            // empty string would be treated as "no scriptDir" by the
            // resolver, so substr(0,0) won't do.
            std::string scriptDir;
            {
                size_t slash = rp.path.find_last_of("/\\");
                if (slash == 0)
                    scriptDir.assign(1, rp.path[0]);   // "/" or "\\"
                else if (slash != std::string::npos)
                    scriptDir = rp.path.substr(0, slash);
            }
            ctx.engine->pushScriptOrigin(rp.fs->name(), scriptDir);
            try {
                ctx.engine->eval(content, resolveEvalScope(ctx));
            } catch (...) {
                ctx.engine->popScriptOrigin();
                throw;
            }
            ctx.engine->popScriptOrigin();
            outs[0] = Value();
        });

    // ── eval ─────────────────────────────────────────────────
    // eval(str) executes `str` in the caller's workspace. Matches
    // MATLAB: variables defined in the eval'd code are visible to the
    // caller (when caller is at top-level), and imports are scoped to
    // the caller's lifetime.
    //
    // When the caller captures the result (`r = eval(...)`, nargout>=1),
    // MATLAB suppresses any "ans = ..." display the inner code would
    // otherwise emit. The third arg routes that suppress through the
    // engine, which flips suppressOutput on top-level statements before
    // executing.
    engine.registerFunction(
        "eval", [resolveEvalScope](Span<const Value> args, size_t nargout,
                                    Span<Value> outs, CallContext &ctx) {
            if (args.empty() || !args[0].isChar())
                throw std::runtime_error("eval requires a string");
            const bool suppress = (nargout >= 1);
            outs[0] = ctx.engine->eval(args[0].toString(),
                                       resolveEvalScope(ctx),
                                       suppress);
        });

    // ── evalin ───────────────────────────────────────────────
    // evalin(workspace, str) executes `str` in either the base
    // workspace ('base') or the workspace of the caller of the function
    // containing this evalin call ('caller'). The latter matches
    // MATLAB's two-frames-up rule.
    engine.registerFunction(
        "evalin", [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 2)
                throw std::runtime_error("evalin: requires (workspace, code)");
            if (!args[0].isChar() || !args[1].isChar())
                throw std::runtime_error("evalin: arguments must be strings");
            std::string where = args[0].toString();
            std::string code = args[1].toString();
            Environment *target = nullptr;
            if (where == "base") {
                target = &ctx.engine->workspaceEnv();
            } else if (where == "caller") {
                if (ctx.engine->callerDepth() < 1)
                    throw std::runtime_error(
                        "evalin: 'caller' is not valid in the base workspace");
                target = ctx.engine->callerEnv(1);
                if (!target) target = &ctx.engine->workspaceEnv();
            } else {
                throw std::runtime_error(
                    "evalin: workspace must be 'base' or 'caller', got '" + where + "'");
            }
            outs[0] = ctx.engine->eval(code, target);
        });
}

} // namespace numkit::corelibs
