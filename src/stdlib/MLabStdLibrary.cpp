#include "MLabStdLibrary.hpp"
#include "MLabPlotLibrary.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace mlab {

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
                            [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
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
    // In MATLAB, `clear` inside a function only affects the local workspace.
    // Since externalFuncs don't have access to the local register frame,
    // we skip globalEnv modification when called from inside a function.
    // The VM's register frame handles local scope automatically.
    engine.registerFunction("clear", [&engine](Span<const MValue> args, size_t, Span<MValue> outs) {
        // Inside a function call: clear only affects local scope.
        // VM registers are already isolated per call frame,
        // so there's nothing to do for the global environment.
        if (engine.isInsideFunctionCall()) {
            outs[0] = MValue::empty();
            return;
        }

        auto &env = engine.globalEnvironment();
        if (args.empty()) {
            env.clearAll();
            engine.clearUserFunctions();
            engine.figureManager().closeAll();
            engine.reinstallConstants();
            engine.markClearAll();
        } else {
            std::string first = args[0].isChar() ? args[0].toString() : "";
            if (first == "all" || first == "classes") {
                env.clearAll();
                engine.clearUserFunctions();
                engine.figureManager().closeAll();
                engine.reinstallConstants();
                engine.markClearAll();
            } else if (first == "functions") {
                engine.clearUserFunctions();
            } else if (first == "global") {
                // TODO: clear global variables specifically
            } else {
                for (auto &a : args) {
                    if (a.isChar()) {
                        std::string varName = a.toString();
                        if (kBuiltinNames.count(varName) == 0) {
                            env.remove(varName);
                            engine.markVarCleared(varName);
                        }
                    }
                }
            }
        }
        outs[0] = MValue::empty();
    });

    // ── clc ────────────────────────────────────────────────────
    engine.registerFunction("clc", [&engine](Span<const MValue>, size_t, Span<MValue> outs) {
        engine.outputText("__CLEAR__\n");
        outs[0] = MValue::empty();
    });

    // ── who ────────────────────────────────────────────────────
    engine.registerFunction("who", [&engine](Span<const MValue> args, size_t, Span<MValue> outs) {
        auto &env = engine.globalEnvironment();
        std::vector<std::string> names;
        if (args.empty()) {
            auto all = env.localNames();
            for (auto &n : all)
                if (kBuiltinNames.count(n) == 0)
                    names.push_back(n);
        } else {
            for (auto &a : args) {
                if (a.isChar()) {
                    std::string varName = a.toString();
                    if (env.has(varName))
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
        engine.outputText(os.str());
        outs[0] = MValue::empty();
    });

    // ── whos ───────────────────────────────────────────────────
    engine.registerFunction("whos", [&engine](Span<const MValue> args, size_t, Span<MValue> outs) {
        auto &env = engine.globalEnvironment();
        std::vector<std::string> names;
        if (args.empty()) {
            auto all = env.localNames();
            for (auto &n : all)
                if (kBuiltinNames.count(n) == 0)
                    names.push_back(n);
        } else {
            for (auto &a : args) {
                if (a.isChar()) {
                    std::string varName = a.toString();
                    if (env.has(varName))
                        names.push_back(varName);
                }
            }
        }
        std::sort(names.begin(), names.end());

        std::ostringstream os;
        if (!names.empty()) {
            os << "  Name" << std::string(6, ' ') << "Size" << std::string(13, ' ')
               << "Bytes  Class" << std::string(5, ' ') << "Attributes\n\n";
            for (auto &n : names) {
                auto *val = env.get(n);
                if (!val)
                    continue;
                auto &d = val->dims();
                std::string sizeStr = std::to_string(d.rows()) + "x" + std::to_string(d.cols());
                if (d.is3D())
                    sizeStr += "x" + std::to_string(d.pages());
                std::string bytesStr = std::to_string(val->rawBytes());
                std::string classStr = mtypeName(val->type());

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
                os << "\n";
            }
            os << "\n";
        }
        engine.outputText(os.str());
        outs[0] = MValue::empty();
    });

    // ── which ──────────────────────────────────────────────────
    engine.registerFunction("which", [&engine](Span<const MValue> args, size_t, Span<MValue> outs) {
        if (args.empty())
            throw std::runtime_error("which requires a name argument");
        std::string qname = args[0].isChar() ? args[0].toString() : "";
        auto &env = engine.globalEnvironment();

        std::ostringstream os;
        if (env.has(qname))
            os << qname << " is a variable.\n";
        else if (engine.hasUserFunction(qname))
            os << qname << " is a user-defined function.\n";
        else if (engine.hasExternalFunction(qname))
            os << "built-in (" << qname << ")\n";
        else
            os << "'" << qname << "' not found.\n";

        engine.outputText(os.str());
        outs[0] = MValue::empty();
    });

    // ── exist ──────────────────────────────────────────────────
    engine.registerFunction("exist", [&engine](Span<const MValue> args, size_t, Span<MValue> outs) {
        if (args.empty())
            throw std::runtime_error("exist requires a name argument");
        std::string varName = args[0].toString();
        auto &env = engine.globalEnvironment();

        double code = 0;
        if (env.has(varName))
            code = 1;
        else if (engine.hasFunction(varName))
            code = 5;

        outs[0] = MValue::scalar(code, &engine.allocator());
    });

    // ── class ──────────────────────────────────────────────────
    engine.registerFunction("class", [&engine](Span<const MValue> args, size_t, Span<MValue> outs) {
        if (args.empty())
            throw std::runtime_error("class requires an argument");
        outs[0] = MValue::fromString(mtypeName(args[0].type()), &engine.allocator());
    });

    // ── tic ────────────────────────────────────────────────────
    engine.registerFunction("tic", [&engine](Span<const MValue>, size_t nargout, Span<MValue> outs) {
        auto now = Clock::now();
        engine.setTicTimer(now);
        if (nargout > 0) {
            double id = static_cast<double>(
                std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch())
                    .count());
            outs[0] = MValue::scalar(id, &engine.allocator());
        } else {
            outs[0] = MValue::empty();
        }
    });

    // ── toc ────────────────────────────────────────────────────
    engine.registerFunction("toc",
                            [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                auto now = Clock::now();
                                TimePoint start;
                                if (!args.empty() && args[0].isScalar()) {
                                    auto us = static_cast<long long>(args[0].toScalar());
                                    start = TimePoint(std::chrono::microseconds(us));
                                } else if (engine.ticWasCalled()) {
                                    start = engine.ticTimer();
                                } else {
                                    throw std::runtime_error(
                                        "toc: You must call 'tic' before calling 'toc'.");
                                }
                                double elapsed = std::chrono::duration<double>(now - start).count();
                                if (nargout > 0) {
                                    outs[0] = MValue::scalar(elapsed, &engine.allocator());
                                } else {
                                    std::ostringstream os;
                                    os << "Elapsed time is " << elapsed << " seconds.\n";
                                    engine.outputText(os.str());
                                    outs[0] = MValue::empty();
                                }
                            });
}

} // namespace mlab