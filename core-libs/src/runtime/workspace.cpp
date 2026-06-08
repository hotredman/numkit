// core-libs/src/runtime/workspace.cpp
//
// Language-runtime workspace builtins, extracted verbatim from toolboxes/builtin's
// registerWorkspaceBuiltins into the core-libs layer (L2, engine-coupled scripting
// runtime — NOT a math/io toolbox). Behaviour unchanged. registerWorkspaceRuntime
// is composed by installRuntimeLibrary (runtime.cpp).
//
// Cluster so far: clear / import / assignin / inputname. (who / whos / exist /
// clearvars follow as the extraction proceeds.)
#include <numkit/corelibs/runtime.hpp>

#include <numkit/core/callback_builtin.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/environment.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace numkit::corelibs {

void registerWorkspaceRuntime(Engine &engine)
{
    // ── clear ──────────────────────────────────────────────────
    engine.registerFunction("clear",
                            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                                auto *env = ctx.env;
                                bool insideFunc = ctx.engine->isInsideFunctionCall();

                                if (args.empty()) {
                                    // MATLAB: bare 'clear' clears workspace variables only,
                                    // NOT user functions or figures.
                                    env->clearAll();
                                    if (!insideFunc) {
                                        ctx.engine->reinstallConstants();
                                        ctx.engine->markClearAll();
                                    }
                                } else {
                                    std::string first = args[0].isChar() ? args[0].toString() : "";

                                    if (first == "-regexp") {
                                        // clear -regexp pat1 pat2 ... — drop every workspace
                                        // variable whose name matches at least one pattern.
                                        // MATLAB uses regexp-style partial match (so `^foo`
                                        // matches "foo1"), not whole-string regex_match.
                                        // std::regex is slow but the workspace is tiny.
                                        std::vector<std::regex> pats;
                                        for (size_t i = 1; i < args.size(); ++i) {
                                            if (!args[i].isChar() && !args[i].isString()) continue;
                                            try { pats.emplace_back(args[i].toString()); }
                                            catch (const std::regex_error &) {
                                                throw std::runtime_error(
                                                    "clear -regexp: invalid pattern '"
                                                    + args[i].toString() + "'");
                                            }
                                        }
                                        // Inside a function `env` is the local frame; the
                                        // user's intent for `clear -regexp` is the SAME
                                        // workspace they'd see via plain `clear x`. Apply
                                        // to both env and (when distinct) the engine's base
                                        // workspace so VM-mode top-level evals work too.
                                        auto applyTo = [&](Environment *e) {
                                            if (!e) return;
                                            for (const auto &n : e->localNames()) {
                                                for (const auto &re : pats) {
                                                    if (std::regex_search(n, re)) {
                                                        e->remove(n);
                                                        break;
                                                    }
                                                }
                                            }
                                        };
                                        applyTo(env);
                                        if (env != &ctx.engine->workspaceEnv())
                                            applyTo(&ctx.engine->workspaceEnv());
                                        outs[0] = Value();
                                        return;
                                    }
                                    if (first == "global") {
                                        auto *gs = ctx.env->globalsEnv();
                                        if (args.size() == 1) {
                                            // clear global — clear all globals
                                            if (gs)
                                                gs->clearAll();
                                            env->clearAll();
                                            ctx.engine->markClearAll();
                                        } else {
                                            // clear global x y — clear specific globals
                                            for (size_t i = 1; i < args.size(); ++i) {
                                                if (args[i].isChar()) {
                                                    std::string gname = args[i].toString();
                                                    if (gs)
                                                        gs->remove(gname);
                                                    env->remove(gname);
                                                }
                                            }
                                        }
                                        outs[0] = Value();
                                        return;
                                    }
                                    if (first == "import") {
                                        // Drop every active import in the current scope.
                                        // Subsequent unqualified lookups fall back to core +
                                        // parent-scope imports (if any).
                                        env->clearImports();
                                        outs[0] = Value();
                                        return;
                                    }

                                    if (first == "all" || first == "classes") {
                                        if (insideFunc) {
                                            env->clearAll();
                                        } else {
                                            env->clearAll();
                                            ctx.engine->clearUserFunctions();
                                            ctx.engine->clearClassDefs();
                                            ctx.engine->figureManager().closeAll();
                                            ctx.engine->reinstallConstants();
                                            ctx.engine->markClearAll();
                                        }
                                    } else if (first == "functions") {
                                        if (!insideFunc)
                                            ctx.engine->clearUserFunctions();
                                    } else {
                                        // `clear x`, `clear pi`, etc.
                                        // Un-shadow a built-in by removing the
                                        // workspace slot — the next read then
                                        // falls back to constantsEnv_. No
                                        // special filtering: MATLAB allows it.
                                        for (auto &a : args) {
                                            if (a.isChar())
                                                env->remove(a.toString());
                                        }
                                    }
                                }
                                outs[0] = Value();
                            });

    // ── import ────────────────────────────────────────────────
    // Command-style: `import signal.*` → import('signal.*').
    // Function-style: `import('signal', 'as', 's')` → alias form.
    // Each string arg is one of:
    //   'a.b.c'   — single-symbol import (path = [a, b, c])
    //   'a.b.*'   — wildcard import      (path = [a, b], wildcard=true)
    //   3-arg form 'a.b' / 'as' / 'name'  → alias
    // Multiple args allowed: `import a.* b.*` pushes two imports.
    engine.registerFunction(
        "import", [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            auto fail = [](const std::string &msg) {
                throw std::runtime_error("import: " + msg);
            };
            auto asString = [&](const Value &v, size_t i) {
                if (!v.isChar() && !v.isString())
                    fail("argument " + std::to_string(i + 1) + " must be a string");
                return v.toString();
            };
            auto parseSpec = [&](const std::string &spec, Import &imp) {
                if (spec.empty()) fail("empty import specifier");
                size_t pos = 0;
                while (pos < spec.size()) {
                    size_t dot = spec.find('.', pos);
                    std::string seg = spec.substr(pos, dot == std::string::npos ? std::string::npos
                                                                                : dot - pos);
                    if (seg == "*") {
                        if (dot != std::string::npos)
                            fail("'*' must be the last component in '" + spec + "'");
                        imp.wildcard = true;
                        return;
                    }
                    if (seg.empty())
                        fail("empty path component in '" + spec + "'");
                    imp.path.push_back(std::move(seg));
                    if (dot == std::string::npos) break;
                    pos = dot + 1;
                }
                if (imp.path.empty()) fail("missing path in '" + spec + "'");
            };

            if (args.empty()) fail("requires at least one argument");

            // Alias form: import('a.b', 'as', 'name') — exactly 3 args, args[1] == 'as'.
            if (args.size() == 3 && (args[1].isChar() || args[1].isString())
                && args[1].toString() == "as") {
                Import imp;
                parseSpec(asString(args[0], 0), imp);
                if (imp.wildcard) fail("'as' alias is not allowed with wildcard import");
                imp.alias = asString(args[2], 2);
                if (imp.alias.empty()) fail("alias name must be non-empty");
                ctx.env->pushImport(std::move(imp));
                outs[0] = Value();
                return;
            }

            for (size_t i = 0; i < args.size(); ++i) {
                std::string spec = asString(args[i], i);
                Import imp;
                parseSpec(spec, imp);
                ctx.env->pushImport(std::move(imp));
            }
            outs[0] = Value();
        });

    // ── assignin ──────────────────────────────────────────────
    // assignin(workspace, name, val) — write `name = val` in
    // `workspace`, where `workspace` is 'base' (top-level) or 'caller'
    // (the workspace of the function that called the function
    // containing this assignin). VM mode also write-throughs to the
    // target frame's register if `name` is statically allocated, so
    // subsequent register-based reads in the target pick up the value.
    engine.registerFunction(
        "assignin", [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() != 3)
                throw std::runtime_error("assignin: requires 3 arguments (workspace, name, value)");
            if (!args[0].isChar())
                throw std::runtime_error("assignin: workspace must be 'base' or 'caller'");
            if (!args[1].isChar())
                throw std::runtime_error("assignin: name must be a string");
            std::string where = args[0].toString();
            std::string name = args[1].toString();
            if (name.empty())
                throw std::runtime_error("assignin: name must be non-empty");
            if (where == "base") {
                ctx.engine->workspaceEnv().set(name, args[2]);
            } else if (where == "caller") {
                // 'caller' is invalid when assignin is called directly
                // from the base workspace — there's no enclosing
                // function to take a caller of (matches MATLAB's
                // "ASSIGNIN cannot have 'caller' as a workspace when
                // used in the base workspace").
                if (ctx.engine->callerDepth() < 1)
                    throw std::runtime_error(
                        "assignin: 'caller' is not valid in the base workspace");
                // Depth 1 = the function that called the function
                // containing this assignin call. Depth 0 would be the
                // assignin-containing function itself.
                ctx.engine->assignToCaller(1, name, args[2]);
            } else {
                throw std::runtime_error(
                    "assignin: workspace must be 'base' or 'caller', got '" + where + "'");
            }
            outs[0] = Value();
        });

    // ── inputname ────────────────────────────────────────────
    // inputname(k) returns the variable name of the k-th input arg as
    // written at the call site of the function containing this call.
    // Empty string if the arg was a literal / expression / non-identifier.
    // Throws when called from outside a function or for k < 1.
    engine.registerFunction(
        "inputname", [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() != 1)
                throw std::runtime_error("inputname: requires one argument (k)");
            double kd = args[0].toScalar();
            int k = static_cast<int>(kd);
            if (static_cast<double>(k) != kd || k < 1)
                throw std::runtime_error("inputname: k must be a positive integer");
            std::string name = ctx.engine->inputName(k);
            outs[0] = Value::fromString(name, ctx.engine->resource());
        });
}

} // namespace numkit::corelibs
