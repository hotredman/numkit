#include <numkit/builtin/general.hpp>
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


} // namespace numkit::builtin::detail

namespace numkit::builtin {

// ── Pure C++ Catalog & Documentation Implementations ───────────────────────

std::string help(const std::string &query)
{
    const auto &catalog = runtime::HelpCatalog::instance();
    if (query.empty()) {
        return catalog.formatAllCategories();
    }
    const runtime::HelpCategory *cat = catalog.findCategory(query);
    if (cat) {
        return catalog.formatCategory(cat->name);
    }
    const runtime::HelpEntry *func = catalog.findFunction(query);
    if (func) {
        return catalog.formatFunction(func->name);
    }
    return "'" + query + "' not found. Type 'help' for a list of topics.\n";
}

Value help(Span<const Value> args, std::pmr::memory_resource *mr)
{
    std::string query = args.empty() ? "" : args[0].toString();
    return Value::fromString(help(query), mr);
}

std::vector<std::string> what(const std::string &category)
{
    const auto &catalog = runtime::HelpCatalog::instance();
    const runtime::HelpCategory *cat = catalog.findCategory(category);
    if (cat) {
        return catalog.getCategoryFunctions(cat->name);
    }
    return catalog.getCategoryFunctions(category);
}

Value what(Span<const Value> args, std::pmr::memory_resource *mr)
{
    const auto &catalog = runtime::HelpCatalog::instance();
    std::string topic = args.empty() ? "elmat" : args[0].toString();
    const runtime::HelpCategory *cat = catalog.findCategory(topic);
    std::vector<std::string> funcs = what(topic);

    Value cellM = Value::cell(funcs.size(), 1, mr);
    for (size_t i = 0; i < funcs.size(); ++i) {
        cellM.cellAt(i) = Value::fromString(funcs[i], mr);
    }
    Value st = Value::structure(mr);
    st.structFields()["path"] = Value::fromString(cat ? cat->name : topic, mr);
    st.structFields()["m"] = std::move(cellM);
    st.structFields()["classes"] = Value::cell(0, 1, mr);
    st.structFields()["packages"] = Value::cell(0, 1, mr);
    return st;
}

std::vector<std::string> builtins(const std::string &category)
{
    const auto &catalog = runtime::HelpCatalog::instance();
    if (category.empty()) {
        return catalog.getAllFunctions();
    }
    return catalog.getCategoryFunctions(category);
}

Value builtins(Span<const Value> args, std::pmr::memory_resource *mr)
{
    std::string topic = args.empty() ? "" : args[0].toString();
    std::vector<std::string> funcs = builtins(topic);
    Value cellOut = Value::cell(funcs.size(), 1, mr);
    for (size_t i = 0; i < funcs.size(); ++i) {
        cellOut.cellAt(i) = Value::fromString(funcs[i], mr);
    }
    return cellOut;
}

std::vector<std::string> categories()
{
    const auto &catalog = runtime::HelpCatalog::instance();
    std::vector<std::string> cats;
    cats.reserve(catalog.categories().size());
    for (const auto &c : catalog.categories()) {
        cats.push_back(c.name);
    }
    return cats;
}

Value categories(std::pmr::memory_resource *mr)
{
    std::vector<std::string> cats = categories();
    Value cellOut = Value::cell(cats.size(), 1, mr);
    for (size_t i = 0; i < cats.size(); ++i) {
        cellOut.cellAt(i) = Value::fromString(cats[i], mr);
    }
    return cellOut;
}

static void help_builtin(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
    std::string text;

    if (args.empty()) {
        text = help("");
    } else {
        std::string query = args[0].isChar() ? args[0].toString() : "";
        if (query.empty() && args[0].isString()) query = args[0].toString();

        const auto &catalog = runtime::HelpCatalog::instance();
        if (catalog.findCategory(query) || catalog.findFunction(query)) {
            text = help(query);
        } else if (ctx.engine->hasUserFunction(query)) {
            text = query + " is a user-defined function.\n";
        } else if (ctx.engine->hasExternalFunction(query)) {
            text = query + " is a built-in function.\n";
        } else {
            text = "'" + query + "' not found. Type 'help' for a list of topics.\n";
        }
    }

    if (nargout > 0) {
        outs[0] = Value::fromString(text, ctx.engine->resource());
    } else {
        ctx.engine->outputText(text);
        outs[0] = Value();
    }
}

static void what_builtin(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
    if (nargout > 0) {
        outs[0] = what(args, ctx.engine->resource());
    } else {
        std::string topic = args.empty() ? "elmat" : (args[0].isChar() ? args[0].toString() : "");
        const auto &catalog = runtime::HelpCatalog::instance();
        const runtime::HelpCategory *cat = catalog.findCategory(topic);
        std::vector<std::string> funcs = what(topic);
        std::string title = cat ? cat->title : topic;

        std::ostringstream os;
        os << "Functions in " << topic << " (" << title << "):\n\n";
        for (size_t i = 0; i < funcs.size(); ++i) {
            os << std::left << std::setw(16) << funcs[i];
            if ((i + 1) % 4 == 0 || i + 1 == funcs.size()) os << "\n";
        }
        os << "\n";
        ctx.engine->outputText(os.str());
        outs[0] = Value();
    }
}

static void builtins_builtin(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
    outs[0] = builtins(args, ctx.engine->resource());
}

static void inmem_builtin(Span<const Value>, size_t nargout, Span<Value> outs, CallContext &ctx) {
    std::vector<std::string> userFuncs;
    std::vector<std::string> classes;

    for (const auto &name : ctx.engine->namespaces()) {
        classes.push_back(name);
    }

    Value mCell = Value::cell(userFuncs.size(), 1, ctx.engine->resource());
    for (size_t i = 0; i < userFuncs.size(); ++i) {
        mCell.cellAt(i) = Value::fromString(userFuncs[i], ctx.engine->resource());
    }
    outs[0] = std::move(mCell);

    if (nargout > 1) {
        outs[1] = Value::cell(0, 1, ctx.engine->resource());
    }
    if (nargout > 2) {
        Value cCell = Value::cell(classes.size(), 1, ctx.engine->resource());
        for (size_t i = 0; i < classes.size(); ++i) {
            cCell.cellAt(i) = Value::fromString(classes[i], ctx.engine->resource());
        }
        outs[2] = std::move(cCell);
    }
}

void register_general(Engine &engine) {
// ── clc ────────────────────────────────────────────────────
    engine.registerFunction("clc",
                            [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
                                ctx.engine->outputText("__CLEAR__\n");
                                outs[0] = Value();
                            });

    // who / whos extracted to the runtime language-runtime layer:
    //   runtime/src/workspace.cpp → numkit::runtime::registerWorkspaceRuntime
    // (composed by installRuntimeLibrary, called by bundle/installStandardLibrary).

    // ── which ──────────────────────────────────────────────────
    engine.registerFunction("which",
                            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error("which requires a name argument");
                                std::string qname = args[0].isChar() ? args[0].toString() : "";
                                auto *env = ctx.env;

                                std::ostringstream os;
                                if (env->getLocal(qname)
                                    || (env->isGlobal(qname) && env->globalsEnv()
                                        && env->globalsEnv()->get(qname)))
                                    os << qname << " is a variable.\n";
                                else if (ctx.engine->hasUserFunction(qname))
                                    os << qname << " is a user-defined function.\n";
                                else if (ctx.engine->hasExternalFunction(qname))
                                    os << "built-in (" << qname << ")\n";
                                else {
                                    // M-file lookup via Engine path registry.
                                    bool found = false;
                                    for (const auto &dir : ctx.engine->path()) {
                                        std::string p = dir;
                                        if (!p.empty() && p.back() != '/' && p.back() != '\\') p += '/';
                                        p += qname + ".m";
                                        try {
                                            auto rp = ctx.engine->resolvePath(p);
                                            if (rp.fs && rp.fs->exists(rp.path)) {
                                                os << p << "\n";
                                                found = true;
                                                break;
                                            }
                                        } catch (...) {}
                                    }
                                    if (!found)
                                        os << "'" << qname << "' not found.\n";
                                }

                                ctx.engine->outputText(os.str());
                                outs[0] = Value();
                            });

    // exist extracted to the runtime language-runtime layer:
    //   runtime/src/workspace.cpp → numkit::runtime::registerWorkspaceRuntime
    // (composed by installRuntimeLibrary, called by bundle/installStandardLibrary).

    
// ── addpath / rmpath / path / rehash / run (Phase 9b) ──────
    engine.registerFunction("addpath",
                            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                                for (const auto &a : args) {
                                    if (a.isChar()) ctx.engine->addPath(a.toString());
                                }
                                outs[0] = Value();
                            });

    engine.registerFunction("rmpath",
                            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                                for (const auto &a : args) {
                                    if (a.isChar()) ctx.engine->rmPath(a.toString());
                                }
                                outs[0] = Value();
                            });

    engine.registerFunction("path",
                            [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
                                // path() with no args: print current path; with nargout, return as char string.
                                // path('a:b:c') in MATLAB also sets the path — we treat single-arg as
                                // a list of paths (string with pathsep) and replace.
                                if (args.empty()) {
                                    const auto &paths = ctx.engine->path();
                                    if (nargout == 0) {
                                        std::ostringstream os;
                                        for (const auto &p : paths) os << p << "\n";
                                        ctx.engine->outputText(os.str());
                                        outs[0] = Value();
                                    } else {
                                        // Return as a single newline-joined char vector
                                        std::ostringstream os;
                                        for (size_t i = 0; i < paths.size(); ++i) {
                                            if (i) os << "\n";
                                            os << paths[i];
                                        }
                                        outs[0] = Value::fromString(os.str(), ctx.engine->resource());
                                    }
                                    return;
                                }
                                // path(p1, p2, ...) — replace path with the given list.
                                // Drop existing entries first.
                                auto current = ctx.engine->path();
                                for (const auto &p : current) ctx.engine->rmPath(p);
                                for (const auto &a : args) {
                                    if (a.isChar()) ctx.engine->addPath(a.toString());
                                }
                                outs[0] = Value();
                            });

    engine.registerFunction("rehash",
                            [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
                                ctx.engine->rehashMFiles();
                                outs[0] = Value();
                            });

    // ── run ──────────────────────────────────────────────────
    // run('script.m') executes the script in the caller's workspace
    // (matches MATLAB semantics: scripts share scope with the caller).
    // ctx.env is the caller's frame.env in VM mode, workspaceEnv at
    // top-level. eval(content, scope) routes the inner top-level's
    // imports + variable assignments to that scope.
    //
    // eval-family (run / eval / evalin) + the shared resolveEvalScope helper
    // were extracted to the runtime language-runtime layer:
    //   runtime/src/eval.cpp → numkit::runtime::installRuntimeLibrary
    // which bundle/installStandardLibrary calls right after BuiltinLibrary::install.

    // ── pwd / cd (Phase 9c) ────────────────────────────────────
    engine.registerFunction("pwd",
                            [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
                                std::string c = ctx.engine->cwd();
                                if (c.empty()) {
                                    // No engine-level cwd set — ask the active backend.
                                    try {
                                        auto rp = ctx.engine->resolvePath(".");
                                        if (rp.fs) c = rp.fs->cwd();
                                    } catch (...) {}
                                    if (c.empty()) {
                                        if (auto *fs = ctx.engine->findVirtualFS("native"))
                                            c = fs->cwd();
                                    }
                                }
                                outs[0] = Value::fromString(c, ctx.engine->resource());
                            });

    engine.registerFunction("cd",
                            [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
                                // No args: behave like pwd (return / print current dir).
                                if (args.empty()) {
                                    std::string c = ctx.engine->cwd();
                                    if (nargout == 0) {
                                        ctx.engine->outputText(c + "\n");
                                        outs[0] = Value();
                                    } else {
                                        outs[0] = Value::fromString(c, ctx.engine->resource());
                                    }
                                    return;
                                }
                                if (!args[0].isChar())
                                    throw std::runtime_error("cd: directory must be a string");
                                std::string target = args[0].toString();
                                auto rp = ctx.engine->resolvePath(target);
                                if (!rp.fs)
                                    throw std::runtime_error("cd: cannot resolve '" + target + "'");
                                auto st = rp.fs->stat(rp.path);
                                if (!st || st->kind != FileStat::Kind::Directory)
                                    throw std::runtime_error("cd: not a directory: " + target);
                                std::string prev = ctx.engine->cwd();
                                ctx.engine->setCwd(rp.path);
                                // MATLAB: with output, return the PREVIOUS cwd.
                                if (nargout > 0)
                                    outs[0] = Value::fromString(prev, ctx.engine->resource());
                                else
                                    outs[0] = Value();
                            });

    // ── version ───────────────────────────────────────────────
    // numkit doesn't carry a SemVer; the build's link-time stamp
    // serves as our "version". The timestamp lives in a tiny
    // separate TU (version_string.cpp) that gets recompiled on
    // every build via the `numkit_build_info` CMake target — so the
    // value here ALWAYS reflects the actual link time, not stale
    // __DATE__/__TIME__ macros from whenever library.cpp's .o
    // happened to last refresh. Declaration lives in the generated
    // build_info.hpp (alongside the NUMKIT_BUILD_TIMESTAMP macro).
    engine.registerFunction(
        "version",
        [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
            outs[0] = Value::fromString(std::string(numkit::buildTimestamp()),
                                        ctx.engine->resource());
        });

    // ── mkdir / rmdir / delete ────────────────────────────────
    engine.registerFunction("mkdir",
                            [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
                                if (args.empty() || !args[0].isChar())
                                    throw std::runtime_error("mkdir requires a directory name");
                                std::string p = args[0].toString();
                                if (args.size() >= 2 && args[1].isChar()) {
                                    // mkdir(parent, name) — concatenate.
                                    std::string parent = p;
                                    std::string name = args[1].toString();
                                    if (!parent.empty() && parent.back() != '/' && parent.back() != '\\')
                                        parent += '/';
                                    p = parent + name;
                                }
                                auto rp = ctx.engine->resolvePath(p);
                                if (!rp.fs)
                                    throw std::runtime_error("mkdir: cannot resolve '" + p + "'");
                                bool ok = true;
                                std::string msg;
                                try {
                                    rp.fs->mkdir(rp.path);
                                } catch (const std::exception &e) {
                                    ok = false;
                                    msg = e.what();
                                }
                                // MATLAB returns [status, msg]; default suppresses errors.
                                if (nargout > 0)
                                    outs[0] = Value::logicalScalar(ok, ctx.engine->resource());
                                else if (!ok)
                                    throw std::runtime_error("mkdir: " + msg);
                                else
                                    outs[0] = Value();
                                if (nargout > 1)
                                    outs[1] = Value::fromString(msg, ctx.engine->resource());
                            });

    engine.registerFunction("rmdir",
                            [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
                                if (args.empty() || !args[0].isChar())
                                    throw std::runtime_error("rmdir requires a directory name");
                                std::string p = args[0].toString();
                                auto rp = ctx.engine->resolvePath(p);
                                if (!rp.fs)
                                    throw std::runtime_error("rmdir: cannot resolve '" + p + "'");
                                bool ok = true;
                                std::string msg;
                                try {
                                    rp.fs->rmdir(rp.path);
                                } catch (const std::exception &e) {
                                    ok = false;
                                    msg = e.what();
                                }
                                if (nargout > 0)
                                    outs[0] = Value::logicalScalar(ok, ctx.engine->resource());
                                else if (!ok)
                                    throw std::runtime_error("rmdir: " + msg);
                                else
                                    outs[0] = Value();
                                if (nargout > 1)
                                    outs[1] = Value::fromString(msg, ctx.engine->resource());
                            });

    engine.registerFunction("delete",
                            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                                for (const auto &a : args) {
                                    if (!a.isChar())
                                        throw std::runtime_error("delete: filename must be a string");
                                    std::string p = a.toString();
                                    auto rp = ctx.engine->resolvePath(p);
                                    if (!rp.fs)
                                        throw std::runtime_error("delete: cannot resolve '" + p + "'");
                                    rp.fs->unlink(rp.path);
                                }
                                outs[0] = Value();
                            });

    // ── dir / ls ──────────────────────────────────────────────
    engine.registerFunction("dir",
                            [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
                                std::string target = args.empty() ? std::string(".")
                                                                   : args[0].toString();
                                auto rp = ctx.engine->resolvePath(target);
                                if (!rp.fs)
                                    throw std::runtime_error("dir: cannot resolve '" + target + "'");
                                std::vector<DirEntry> entries;
                                auto st = rp.fs->stat(rp.path);
                                if (st && st->kind == FileStat::Kind::File) {
                                    DirEntry e;
                                    e.name = rp.path;
                                    e.isDirectory = false;
                                    entries.push_back(e);
                                } else {
                                    entries = rp.fs->listDir(rp.path);
                                }

                                if (nargout == 0) {
                                    // Print tabular listing (MATLAB-ish).
                                    std::ostringstream os;
                                    for (const auto &e : entries) {
                                        os << e.name;
                                        if (e.isDirectory) os << "/";
                                        os << "\n";
                                    }
                                    ctx.engine->outputText(os.str());
                                    outs[0] = Value();
                                    return;
                                }

                                // n×1 struct array — same fields per element so MATLAB-style
                                // d(i).name / [d.bytes] usage works.
                                auto *mr = ctx.engine->resource();
                                if (entries.empty()) {
                                    outs[0] = Value::structure(mr);
                                    return;
                                }
                                Value arr = Value::structArray(entries.size(), 1, mr);
                                for (size_t i = 0; i < entries.size(); ++i) {
                                    auto &fields = arr.structArrayElem(i);
                                    fields["name"]    = Value::fromString(entries[i].name, mr);
                                    fields["folder"]  = Value::fromString(rp.path, mr);
                                    fields["isdir"]   = Value::logicalScalar(entries[i].isDirectory, mr);
                                    std::string full = rp.path;
                                    if (!full.empty() && full.back() != '/' && full.back() != '\\')
                                        full += '/';
                                    full += entries[i].name;
                                    auto est = rp.fs->stat(full);
                                    fields["bytes"]   = Value::scalar(est ? double(est->size) : 0.0, mr);
                                    fields["datenum"] = Value::scalar(est ? double(est->mtime) : 0.0, mr);
                                    fields["date"]    = Value::fromString(std::string{}, mr);
                                }
                                outs[0] = std::move(arr);
                            });

    engine.registerFunction("ls",
                            [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
                                std::string target = args.empty() ? std::string(".")
                                                                   : args[0].toString();
                                auto rp = ctx.engine->resolvePath(target);
                                if (!rp.fs)
                                    throw std::runtime_error("ls: cannot resolve '" + target + "'");
                                auto entries = rp.fs->listDir(rp.path);
                                if (nargout == 0) {
                                    std::ostringstream os;
                                    for (const auto &e : entries)
                                        os << e.name << "\n";
                                    ctx.engine->outputText(os.str());
                                    outs[0] = Value();
                                } else {
                                    // Return as newline-joined char vector (MATLAB ls semantics).
                                    std::ostringstream os;
                                    for (size_t i = 0; i < entries.size(); ++i) {
                                        if (i) os << "  ";
                                        os << entries[i].name;
                                    }
                                    outs[0] = Value::fromString(os.str(), ctx.engine->resource());
                                }
                            });

    // ── pathsep ───────────────────────────────────────────────
    // (filesep / fullfile / fileparts / tempdir / tempname moved to
    //  toolboxes/io/src/paths/paths.cpp + compat alias — they belong to the
    //  io toolbox per the MATLAB taxonomy. pathsep stays here only
    //  because no io equivalent exists yet.)
    engine.registerFunction("pathsep",
                            [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
#ifdef _WIN32
                                outs[0] = Value::fromString(";", ctx.engine->resource());
#else
                                outs[0] = Value::fromString(":", ctx.engine->resource());
#endif
                            });

    
// ── Pack 25: workspace / display utilities ────────────────────────

    // clearvars extracted to the runtime language-runtime layer:
    //   runtime/src/workspace.cpp → numkit::runtime::registerWorkspaceRuntime
    // (composed by installRuntimeLibrary, called by bundle/installStandardLibrary).

    // formatteddisplaytext(x) — return what disp(x) would print, but
    // as a string instead of writing it to the output stream.
    engine.registerFunction("formatteddisplaytext",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.empty())
                throw std::runtime_error("formatteddisplaytext requires 1 argument");
            outs[0] = Value::fromString(args[0].formatDisplay(""),
                                         ctx.engine->resource());
        });

    // format(spec) — accepted for compatibility, no-op (numkit always
    // formats with ~15 significant digits). Recognised specs are
    // 'short', 'long', 'compact', 'loose', 'shortG', 'longG', 'shortE',
    // 'longE', 'rat', 'hex'; unknown specs throw.
    engine.registerFunction("format",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &) {
            if (!args.empty()) {
                if (!args[0].isChar() && !args[0].isString())
                    throw std::runtime_error(
                        "format: argument must be a string");
                static const std::set<std::string> known = {
                    "short", "long", "compact", "loose",
                    "shortg", "longg", "shorte", "longe",
                    "shorteng", "longeng", "rat", "hex", "bank", "+", "default"
                };
                std::string s = args[0].toString();
                for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (!known.count(s))
                    throw std::runtime_error(
                        "format: unrecognised format spec '" + s + "'");
            }
            // No-op: numkit's display already runs at full precision.
            outs[0] = Value();
        });

    
// home — like clc; cursor to top + clear.
    engine.registerFunction("home",
        [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
            ctx.engine->outputText("__CLEAR__\n");
            outs[0] = Value();
        });

    // optimset / optimget — option struct utility
    engine.registerFunction("optimset",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            auto *mr = ctx.engine->resource();
            auto s = Value::structure(mr);
            s.field("Display")     = Value::fromString("notify", mr);
            s.field("MaxFunEvals") = Value::scalar(1000.0, mr);
            s.field("MaxIter")     = Value::scalar(500.0, mr);
            s.field("TolFun")      = Value::scalar(1e-6, mr);
            s.field("TolX")        = Value::scalar(1e-6, mr);
            for (size_t i = 0; i + 1 < args.size(); i += 2) {
                if (!args[i].isChar() && !args[i].isString())
                    throw std::runtime_error("optimset: option name must be a string");
                s.field(args[i].toString()) = args[i + 1];
            }
            outs[0] = std::move(s);
        });

    engine.registerFunction("optimget",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 2)
                throw std::runtime_error("optimget requires (options, name[, default])");
            const Value &opts = args[0];
            const std::string name = args[1].toString();
            if (opts.isStruct() && opts.hasField(name)) {
                outs[0] = opts.field(name);
                return;
            }
            if (args.size() >= 3) outs[0] = args[2];
            else outs[0] = Value();
        });

    // freqspace(n) — frequency-spacing vector for FFT-style problems.
    engine.registerFunction("freqspace",
        [](Span<const Value> args, size_t nargout, Span<Value> outs,
           CallContext &ctx) {
            if (args.empty())
                throw std::runtime_error("freqspace requires (n[, 'whole'])");
            size_t n_rows = 0, n_cols = 0;
            const Value &a0 = args[0];
            if (a0.numel() == 1) {
                n_rows = n_cols = static_cast<size_t>(a0.toScalar());
            } else if (a0.numel() == 2) {
                n_rows = static_cast<size_t>(a0.elemAsDouble(0));
                n_cols = static_cast<size_t>(a0.elemAsDouble(1));
            } else {
                throw std::runtime_error("freqspace: N must be a scalar or 2-element vector");
            }
            bool whole = false;
            if (args.size() >= 2 && (args[1].isChar() || args[1].isString())) {
                std::string s = args[1].toString();
                for (auto &c : s)
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                whole = (s == "whole");
            }
            auto *mr = ctx.engine->resource();

            auto whole_vec = [&](size_t n) {
                auto v = Value::matrix(1, n, ValueType::DOUBLE, mr);
                if (n == 0) return v;
                double *d = v.doubleDataMut();
                const double step = 2.0 / static_cast<double>(n);
                for (size_t i = 0; i < n; ++i)
                    d[i] = step * static_cast<double>(i);
                return v;
            };
            auto centered_vec = [&](size_t n) {
                auto v = Value::matrix(1, n, ValueType::DOUBLE, mr);
                if (n == 0) return v;
                double *d = v.doubleDataMut();
                const long start = (n % 2 == 0) ? -static_cast<long>(n)
                                                : -static_cast<long>(n) + 1;
                for (size_t i = 0; i < n; ++i)
                    d[i] = static_cast<double>(start + 2 * static_cast<long>(i))
                         / static_cast<double>(n);
                return v;
            };
            auto half_vec = [&](size_t n) {
                if (n == 0)
                    return Value::matrix(1, 0, ValueType::DOUBLE, mr);
                size_t m;
                double last;
                if (n % 2 == 0) { m = n / 2 + 1; last = 1.0; }
                else            { m = (n + 1) / 2;
                                  last = 1.0 - 1.0 / static_cast<double>(n); }
                auto v = Value::matrix(1, m, ValueType::DOUBLE, mr);
                double *d = v.doubleDataMut();
                if (m == 1) d[0] = 0.0;
                else {
                    const double step = last / static_cast<double>(m - 1);
                    for (size_t i = 0; i < m; ++i)
                        d[i] = step * static_cast<double>(i);
                }
                return v;
            };

            if (nargout > 1) {
                if (whole)
                    throw std::runtime_error("freqspace: 2-output 'whole' form is not supported");
                outs[0] = centered_vec(n_cols);
                outs[1] = centered_vec(n_rows);
                return;
            }
            if (a0.numel() == 2)
                throw std::runtime_error("freqspace: 2-vec input requires 2 output args");
            outs[0] = whole ? whole_vec(n_rows) : half_vec(n_rows);
        });

    engine.registerFunction("help", &help_builtin);
    engine.registerFunction("doc", &help_builtin);
    engine.registerFunction("what", &what_builtin);
    engine.registerFunction("builtins", &builtins_builtin);
    engine.registerFunction("inmem", &inmem_builtin);
}

} // namespace numkit::builtin
