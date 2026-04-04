#include "MLabStdLibrary.hpp"
#include "MLabPlotLibrary.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace mlab {

// ── Warning helper for unsupported features ──────────────────
static void warnNotSupported(CallContext &ctx, const std::string &feature)
{
    ctx.engine->outputText("Warning: '" + feature + "' is not yet supported.\n");
}

void StdLibrary::install(Engine &engine)
{
    registerBinaryOps(engine);
    registerUnaryOps(engine);
    registerMathFunctions(engine);
    registerMatrixFunctions(engine);
    registerIOFunctions(engine);
    registerTypeFunctions(engine);
    registerCellStructFunctions(engine);
    registerStringFunctions(engine);
    registerComplexFunctions(engine);
    registerSignalCoreFunctions(engine);
    registerConvolutionFunctions(engine);
    registerWindowFunctions(engine);
    registerFilterFunctions(engine);
    registerFilterDesignFunctions(engine);
    registerSpectralFunctions(engine);
    registerResampleFunctions(engine);
    registerTransformFunctions(engine);
    registerInterpFunctions(engine);

    PlotLibrary::install(engine);

    registerWorkspaceBuiltins(engine);

    // --- arrayfun (basic scalar version) ---
    engine.registerFunction("arrayfun",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                if (args.size() < 2)
                                    throw std::runtime_error(
                                        "arrayfun requires at least 2 arguments");
                                {
                                    outs[0] = args[1];
                                    return;
                                }
                            });
}

// ============================================================
// Workspace / session builtins
//
// These were previously handled only by TreeWalker::tryBuiltinCall(),
// which meant the VM could never execute them. By registering them
// as externalFuncs, both backends can dispatch them uniformly
// through the standard CALL opcode.
// ============================================================

void StdLibrary::registerWorkspaceBuiltins(Engine &engine)
{
    // ── clear ──────────────────────────────────────────────────
    engine.registerFunction("clear",
                            [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
                                auto *env = ctx.env;
                                bool insideFunc = ctx.engine->isInsideFunctionCall();

                                if (args.empty()) {
                                    if (insideFunc) {
                                        env->clearAll();
                                    } else {
                                        env->clearAll();
                                        ctx.engine->clearUserFunctions();
                                        ctx.engine->figureManager().closeAll();
                                        ctx.engine->reinstallConstants();
                                        ctx.engine->markClearAll();
                                    }
                                } else {
                                    std::string first = args[0].isChar() ? args[0].toString() : "";

                                    // Unsupported flags
                                    if (first == "-regexp") {
                                        warnNotSupported(ctx, "clear -regexp");
                                        outs[0] = MValue::empty();
                                        return;
                                    }
                                    if (first == "global") {
                                        auto *gs = ctx.env->globalStore();
                                        if (args.size() == 1) {
                                            // clear global — clear all globals
                                            if (gs)
                                                gs->clear();
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
                                                    ctx.engine->markVarCleared(gname);
                                                }
                                            }
                                        }
                                        outs[0] = MValue::empty();
                                        return;
                                    }
                                    if (first == "import") {
                                        warnNotSupported(ctx, "clear import");
                                        outs[0] = MValue::empty();
                                        return;
                                    }

                                    if (first == "all" || first == "classes") {
                                        if (insideFunc) {
                                            env->clearAll();
                                        } else {
                                            env->clearAll();
                                            ctx.engine->clearUserFunctions();
                                            ctx.engine->figureManager().closeAll();
                                            ctx.engine->reinstallConstants();
                                            ctx.engine->markClearAll();
                                        }
                                    } else if (first == "functions") {
                                        if (!insideFunc)
                                            ctx.engine->clearUserFunctions();
                                    } else {
                                        for (auto &a : args) {
                                            if (a.isChar()) {
                                                std::string varName = a.toString();
                                                if (kBuiltinNames.count(varName) == 0) {
                                                    env->remove(varName);
                                                    if (!insideFunc)
                                                        ctx.engine->markVarCleared(varName);
                                                }
                                            }
                                        }
                                    }
                                }
                                outs[0] = MValue::empty();
                            });

    // ── clc ────────────────────────────────────────────────────
    engine.registerFunction("clc",
                            [](Span<const MValue>, size_t, Span<MValue> outs, CallContext &ctx) {
                                ctx.engine->outputText("__CLEAR__\n");
                                outs[0] = MValue::empty();
                            });

    // ── who ────────────────────────────────────────────────────
    engine.registerFunction("who",
                            [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
                                auto *env = ctx.env;

                                // Check for unsupported flags
                                if (!args.empty() && args[0].isChar()) {
                                    std::string first = args[0].toString();
                                    if (first == "-file") {
                                        warnNotSupported(ctx, "who -file");
                                        outs[0] = MValue::empty();
                                        return;
                                    }
                                }

                                std::vector<std::string> names;
                                if (args.empty()) {
                                    auto all = env->localNames();
                                    for (auto &n : all)
                                        if (kBuiltinNames.count(n) == 0 && n != "nargin"
                                            && n != "nargout")
                                            names.push_back(n);
                                } else {
                                    for (auto &a : args) {
                                        if (a.isChar()) {
                                            std::string varName = a.toString();
                                            if (env->getLocal(varName))
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
                                outs[0] = MValue::empty();
                            });

    // ── whos ───────────────────────────────────────────────────
    engine.registerFunction("whos",
                            [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
                                auto *env = ctx.env;

                                // Check for unsupported flags
                                if (!args.empty() && args[0].isChar()) {
                                    std::string first = args[0].toString();
                                    if (first == "-file") {
                                        warnNotSupported(ctx, "whos -file");
                                        outs[0] = MValue::empty();
                                        return;
                                    }
                                }

                                std::vector<std::string> names;
                                if (args.empty()) {
                                    auto all = env->localNames();
                                    for (auto &n : all)
                                        if (kBuiltinNames.count(n) == 0 && n != "nargin"
                                            && n != "nargout")
                                            names.push_back(n);
                                } else {
                                    for (auto &a : args) {
                                        if (a.isChar()) {
                                            std::string varName = a.toString();
                                            if (env->getLocal(varName))
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
                                        std::string bytesStr = std::to_string(val->rawBytes());
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
                                outs[0] = MValue::empty();
                            });

    // ── which ──────────────────────────────────────────────────
    engine.registerFunction("which",
                            [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error("which requires a name argument");
                                std::string qname = args[0].isChar() ? args[0].toString() : "";
                                auto *env = ctx.env;

                                std::ostringstream os;
                                if (env->getLocal(qname)
                                    || (env->isGlobal(qname) && env->globalStore()
                                        && env->globalStore()->get(qname)))
                                    os << qname << " is a variable.\n";
                                else if (ctx.engine->hasUserFunction(qname))
                                    os << qname << " is a user-defined function.\n";
                                else if (ctx.engine->hasExternalFunction(qname))
                                    os << "built-in (" << qname << ")\n";
                                else
                                    os << "'" << qname << "' not found.\n";

                                ctx.engine->outputText(os.str());
                                outs[0] = MValue::empty();
                            });

    // ── exist ──────────────────────────────────────────────────
    engine.registerFunction("exist",
                            [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error("exist requires a name argument");
                                std::string varName = args[0].toString();
                                auto *env = ctx.env;

                                // Optional second argument: type filter
                                std::string typeFilter;
                                if (args.size() >= 2 && args[1].isChar())
                                    typeFilter = args[1].toString();

                                // Unsupported type filters
                                if (typeFilter == "file" || typeFilter == "dir") {
                                    warnNotSupported(ctx, "exist(name, '" + typeFilter + "')");
                                    outs[0] = MValue::scalar(0.0, &ctx.engine->allocator());
                                    return;
                                }
                                if (typeFilter == "class") {
                                    warnNotSupported(ctx, "exist(name, 'class')");
                                    outs[0] = MValue::scalar(0.0, &ctx.engine->allocator());
                                    return;
                                }

                                double code = 0;
                                // Check local scope only for variables (don't leak to parent)
                                bool isVar = (env->getLocal(varName) != nullptr);
                                // Also check global declarations in current env
                                if (!isVar && env->isGlobal(varName)) {
                                    auto *gs = env->globalStore();
                                    isVar = (gs && gs->get(varName) != nullptr);
                                }
                                bool isFunc = ctx.engine->hasFunction(varName);

                                if (typeFilter.empty()) {
                                    // No filter: return first match
                                    if (isVar)
                                        code = 1;
                                    else if (isFunc)
                                        code = 5;
                                } else if (typeFilter == "var") {
                                    if (isVar)
                                        code = 1;
                                } else if (typeFilter == "builtin") {
                                    if (ctx.engine->hasExternalFunction(varName))
                                        code = 5;
                                }

                                outs[0] = MValue::scalar(code, &ctx.engine->allocator());
                            });

    // ── class ──────────────────────────────────────────────────
    engine.registerFunction("class",
                            [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error("class requires an argument");
                                outs[0] = MValue::fromString(mtypeName(args[0].type()),
                                                             &ctx.engine->allocator());
                            });

    // ── tic ────────────────────────────────────────────────────
    engine.registerFunction("tic",
                            [](Span<const MValue>,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto now = Clock::now();
                                ctx.engine->setTicTimer(now);
                                if (nargout > 0) {
                                    double id = static_cast<double>(
                                        std::chrono::duration_cast<std::chrono::microseconds>(
                                            now.time_since_epoch())
                                            .count());
                                    outs[0] = MValue::scalar(id, &ctx.engine->allocator());
                                } else {
                                    outs[0] = MValue::empty();
                                }
                            });

    // ── toc ────────────────────────────────────────────────────
    engine.registerFunction("toc",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto now = Clock::now();
                                TimePoint start;
                                if (!args.empty() && args[0].isScalar()) {
                                    auto us = static_cast<long long>(args[0].toScalar());
                                    start = TimePoint(std::chrono::microseconds(us));
                                } else if (ctx.engine->ticWasCalled()) {
                                    start = ctx.engine->ticTimer();
                                } else {
                                    throw std::runtime_error(
                                        "toc: You must call 'tic' before calling 'toc'.");
                                }
                                double elapsed = std::chrono::duration<double>(now - start).count();
                                if (nargout > 0) {
                                    outs[0] = MValue::scalar(elapsed, &ctx.engine->allocator());
                                } else {
                                    std::ostringstream os;
                                    os << "Elapsed time is " << elapsed << " seconds.\n";
                                    ctx.engine->outputText(os.str());
                                    outs[0] = MValue::empty();
                                }
                            });
}

} // namespace mlab