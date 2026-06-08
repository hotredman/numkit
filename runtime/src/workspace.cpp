// runtime/src/workspace.cpp
//
// Language-runtime workspace builtins, extracted verbatim from toolboxes/builtin's
// registerWorkspaceBuiltins into the runtime layer (L2, engine-coupled scripting
// runtime — NOT a math/io toolbox). Behaviour unchanged. registerWorkspaceRuntime
// is composed by installRuntimeLibrary (runtime.cpp).
//
// Cluster: clear / import / assignin / inputname / who / whos / exist / clearvars.
// (clc / which stay in toolboxes/builtin — path/meta, not workspace runtime.)
#include <numkit/runtime/runtime.hpp>

#include <numkit/core/callback_builtin.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/environment.hpp>
#include <numkit/fs/vfs.hpp>          // FileStat (exist / who -file / whos -file)
#include <numkit/value/scratch.hpp>   // ScratchArena / ScratchVec (who / whos)
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/value_type.hpp> // mtypeName (whos)

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace numkit::runtime {

// save / load (workspace persistence) — impl + matio v5 .mat backend live
// in workspace/saveload.cpp (+ saveload_mat.cpp). Their reg adapters are
// defined there; declared here so registerWorkspaceRuntime can register
// them bare (MATLAB save/load are base builtins).
namespace detail {
void save_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void load_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
} // namespace detail

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

    // ── who ────────────────────────────────────────────────────
    engine.registerFunction("who",
                            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                                auto *env = ctx.env;

                                // -file <fname>: in our ASCII save format the file
                                // contains a single matrix; load() would assign it to
                                // a workspace variable named after the file stem.
                                // Mirror that contract here.
                                if (!args.empty() && args[0].isChar() && args[0].toString() == "-file") {
                                    if (args.size() < 2 || !args[1].isChar())
                                        throw std::runtime_error("who -file requires a filename");
                                    std::string fname = args[1].toString();
                                    auto rp = ctx.engine->resolvePath(fname);
                                    if (!rp.fs || !rp.fs->exists(rp.path))
                                        throw std::runtime_error("who -file: file not found: " + fname);
                                    std::string stem = fname;
                                    size_t sep = stem.find_last_of("/\\:");
                                    if (sep != std::string::npos) stem = stem.substr(sep + 1);
                                    size_t dot = stem.find_last_of('.');
                                    if (dot != std::string::npos && dot > 0) stem = stem.substr(0, dot);
                                    std::ostringstream os;
                                    os << "\nYour variables are:\n\n" << stem << "  \n\n";
                                    ctx.engine->outputText(os.str());
                                    outs[0] = Value();
                                    return;
                                }

                                ScratchArena scratch(ctx.engine->resource());
                                ScratchVec<std::string> names(&scratch);
                                // Pseudo-vars set by callUserFunction (nargin /
                                // nargout) shouldn't appear in `who` output —
                                // matches MATLAB.
                                auto isPseudo = [](const std::string &n) {
                                    return n == "nargin" || n == "nargout";
                                };
                                if (args.empty()) {
                                    // localNames() excludes parent-env constants
                                    // (pi, eps, …) — they show up here only if
                                    // shadowed in the workspace, as in MATLAB.
                                    auto src = env->localNames();
                                    for (auto &n : src)
                                        if (!isPseudo(n)) names.push_back(n);
                                    // Globals declared in this workspace live in
                                    // globalsEnv_, not local storage — MATLAB
                                    // lists them too. Disjoint from localNames.
                                    for (const auto &g : env->globalNames())
                                        if (!isPseudo(g) && env->get(g)) names.push_back(g);
                                } else {
                                    for (auto &a : args) {
                                        if (a.isChar()) {
                                            std::string varName = a.toString();
                                            if (env->getLocal(varName) && !isPseudo(varName))
                                                names.push_back(varName);
                                        }
                                    }
                                }
                                std::sort(names.begin(), names.end());

                                std::ostringstream os;
                                if (!names.empty()) {
                                    os << "\nYour variables are:\n\n";
                                    for (auto &n : names)
                                        os << n << "  ";
                                    os << "\n\n";
                                }
                                ctx.engine->outputText(os.str());
                                outs[0] = Value();
                            });

    // ── whos ───────────────────────────────────────────────────
    engine.registerFunction("whos",
                            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                                auto *env = ctx.env;

                                // -file <fname>: ASCII save format holds a single matrix
                                // assigned to the file's stem. Surface that as a one-row
                                // listing with a usable bytes/size estimate from stat().
                                if (!args.empty() && args[0].isChar() && args[0].toString() == "-file") {
                                    if (args.size() < 2 || !args[1].isChar())
                                        throw std::runtime_error("whos -file requires a filename");
                                    std::string fname = args[1].toString();
                                    auto rp = ctx.engine->resolvePath(fname);
                                    if (!rp.fs || !rp.fs->exists(rp.path))
                                        throw std::runtime_error("whos -file: file not found: " + fname);
                                    auto st = rp.fs->stat(rp.path);
                                    int64_t bytes = st ? st->size : 0;
                                    std::string stem = fname;
                                    size_t sep = stem.find_last_of("/\\:");
                                    if (sep != std::string::npos) stem = stem.substr(sep + 1);
                                    size_t dot = stem.find_last_of('.');
                                    if (dot != std::string::npos && dot > 0) stem = stem.substr(0, dot);
                                    std::ostringstream os;
                                    os << "  Name      Size    Bytes  Class\n";
                                    os << "  " << std::left << std::setw(8) << stem
                                       << "  ?       " << bytes << "  double\n";
                                    ctx.engine->outputText(os.str());
                                    outs[0] = Value();
                                    return;
                                }

                                ScratchArena scratch(ctx.engine->resource());
                                ScratchVec<std::string> names(&scratch);
                                auto isPseudo = [](const std::string &n) {
                                    return n == "nargin" || n == "nargout";
                                };
                                if (args.empty()) {
                                    auto src = env->localNames();
                                    for (auto &n : src)
                                        if (!isPseudo(n)) names.push_back(n);
                                    // Globals declared in this workspace (value
                                    // in globalsEnv_, not local storage) — list
                                    // them too, matching MATLAB.
                                    for (const auto &g : env->globalNames())
                                        if (!isPseudo(g) && env->get(g)) names.push_back(g);
                                } else {
                                    for (auto &a : args) {
                                        if (a.isChar()) {
                                            std::string varName = a.toString();
                                            if (env->getLocal(varName) && !isPseudo(varName))
                                                names.push_back(varName);
                                        }
                                    }
                                }
                                std::sort(names.begin(), names.end());

                                std::ostringstream os;
                                if (!names.empty()) {
                                    os << "  Name" << std::string(6, ' ') << "Size"
                                       << std::string(13, ' ') << "Bytes  Class"
                                       << std::string(5, ' ') << "Attributes\n\n";
                                    for (auto &n : names) {
                                        auto *val = env->get(n);
                                        if (!val)
                                            continue;
                                        auto &d = val->dims();
                                        std::string sizeStr = std::to_string(d.rows()) + "x"
                                                              + std::to_string(d.cols());
                                        if (d.is3D())
                                            sizeStr += "x" + std::to_string(d.pages());
                                        std::string bytesStr = std::to_string(val->deepBytes());
                                        std::string classStr = mtypeName(val->type());
                                        std::string attrStr;
                                        if (env->isGlobal(n))
                                            attrStr = "global";

                                        os << "  " << n;
                                        for (size_t i = n.size(); i < 10; ++i)
                                            os << " ";
                                        os << sizeStr;
                                        for (size_t i = sizeStr.size(); i < 17; ++i)
                                            os << " ";
                                        for (size_t i = bytesStr.size(); i < 5; ++i)
                                            os << " ";
                                        os << bytesStr << "  " << classStr;
                                        for (size_t i = classStr.size(); i < 10; ++i)
                                            os << " ";
                                        os << attrStr << "\n";
                                    }
                                    os << "\n";
                                }
                                ctx.engine->outputText(os.str());
                                outs[0] = Value();
                            });

    // ── exist ──────────────────────────────────────────────────
    engine.registerFunction("exist",
                            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error("exist requires a name argument");
                                std::string varName = args[0].toString();
                                auto *env = ctx.env;

                                // Optional second argument: type filter
                                std::string typeFilter;
                                if (args.size() >= 2 && args[1].isChar())
                                    typeFilter = args[1].toString();

                                if (typeFilter == "class") {
                                    // No user-defined classdef yet (no class system in
                                    // numkit::Engine). Returning 0 matches MATLAB's
                                    // behaviour for unknown class names — scripts that
                                    // probe for a class get a benign "no" rather than
                                    // a fatal "unsupported".
                                    outs[0] = Value::scalar(0.0, ctx.engine->resource());
                                    return;
                                }

                                auto vfsExists = [&](const std::string &p) -> bool {
                                    try {
                                        auto rp = ctx.engine->resolvePath(p);
                                        return rp.fs && rp.fs->exists(rp.path);
                                    } catch (...) { return false; }
                                };
                                auto vfsIsDir = [&](const std::string &p) -> bool {
                                    try {
                                        auto rp = ctx.engine->resolvePath(p);
                                        if (!rp.fs) return false;
                                        auto st = rp.fs->stat(rp.path);
                                        return st && st->kind == FileStat::Kind::Directory;
                                    } catch (...) { return false; }
                                };

                                double code = 0;
                                // Check local scope only for variables (don't leak to parent)
                                bool isVar = (env->getLocal(varName) != nullptr);
                                if (!isVar && env->isGlobal(varName)) {
                                    auto *gs = env->globalsEnv();
                                    isVar = (gs && gs->get(varName) != nullptr);
                                }
                                bool isFunc = ctx.engine->hasFunction(varName);

                                // Walk path list to check for `<name>.m` (m-file resolver)
                                auto findMFile = [&]() -> bool {
                                    for (const auto &dir : ctx.engine->path()) {
                                        std::string p = dir;
                                        if (!p.empty() && p.back() != '/' && p.back() != '\\') p += '/';
                                        p += varName + ".m";
                                        if (vfsExists(p)) return true;
                                    }
                                    return false;
                                };

                                if (typeFilter.empty()) {
                                    if (isVar)              code = 1;
                                    else if (isFunc)        code = 5;
                                    else if (vfsExists(varName)) code = 2;       // file
                                    else if (findMFile())   code = 2;            // m-file in path
                                } else if (typeFilter == "var") {
                                    if (isVar)              code = 1;
                                } else if (typeFilter == "builtin") {
                                    if (ctx.engine->hasExternalFunction(varName)) code = 5;
                                } else if (typeFilter == "file") {
                                    // Direct path — file or m-file in search path
                                    if (vfsExists(varName))                       code = 2;
                                    else if (findMFile())                          code = 2;
                                } else if (typeFilter == "dir") {
                                    if (vfsIsDir(varName))                        code = 7;
                                }

                                outs[0] = Value::scalar(code, ctx.engine->resource());
                            });

    // clearvars — clear named workspace variables. With no args, clears
    // all. With "-except name1 name2 ...", clears everything except the
    // listed names. Doesn't unload functions, never affects globals
    // (matches MATLAB's `clearvars` vs. `clear` distinction).
    engine.registerFunction("clearvars",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            auto *env = ctx.env;
            if (args.empty()) {
                env->clearAll();
                outs[0] = Value();
                return;
            }
            // Parse "-except".
            bool exceptMode = false;
            std::vector<std::string> names;
            for (size_t i = 0; i < args.size(); ++i) {
                if (!args[i].isChar() && !args[i].isString()) continue;
                const std::string s = args[i].toString();
                if (s == "-except") { exceptMode = true; continue; }
                names.push_back(s);
            }
            if (exceptMode) {
                std::set<std::string> keep(names.begin(), names.end());
                for (const auto &n : env->localNames()) {
                    if (!keep.count(n)) env->remove(n);
                }
            } else {
                for (const auto &n : names) env->remove(n);
            }
            outs[0] = Value();
        });

    // ── save / load ────────────────────────────────────────────
    // Workspace persistence (ASCII + matio v5 .mat). Registered BARE
    // because MATLAB save/load are base builtins, not io-toolbox fns;
    // the old io.workspace.* / compat.* aliases are retired (unused).
    engine.registerFunction("save", &detail::save_reg);
    engine.registerFunction("load", &detail::load_reg);
}

} // namespace numkit::runtime
