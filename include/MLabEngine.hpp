// include/MLabEngine.hpp
#pragma once

#include "MLabDebugger.hpp"
#include "MLabFigureManager.hpp"
#include "MLabTypes.hpp"
#include "MLabVM.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace mlab {

class TreeWalker;
class VM;
class Compiler;

class Engine
{
public:
    Engine();
    ~Engine();

    Engine(const Engine &) = delete;
    Engine &operator=(const Engine &) = delete;
    Engine(Engine &&) = delete;
    Engine &operator=(Engine &&) = delete;

    // --- Public API ---
    void setAllocator(Allocator alloc);
    Allocator &allocator();

    void registerBinaryOp(const std::string &op, BinaryOpFunc func);
    void registerUnaryOp(const std::string &op, UnaryOpFunc func);
    void registerFunction(const std::string &name, ExternalFunc func);

    void setVariable(const std::string &name, MValue val);
    MValue *getVariable(const std::string &name);

    // Execute code — uses current backend (TreeWalker by default)
    MValue eval(const std::string &code);

    // Safe execution — never throws, returns result + error diagnostics
    struct EvalResult {
        MValue value;
        bool ok = true;
        bool debugStop = false;
        std::string errorMessage;
        int errorLine = 0;
        int errorCol = 0;
        std::string errorFunc;
        std::string errorContext;  // e.g. "in call to 'sin'"
    };
    EvalResult evalSafe(const std::string &code);

    // Backend selection
    enum class Backend { VM, TreeWalker };
    void setBackend(Backend b) { backend_ = b; }
    Backend backend() const { return backend_; }

    using OutputFunc = std::function<void(const std::string &)>;
    void setOutputFunc(OutputFunc f);
    void setMaxRecursionDepth(int depth);

    std::vector<std::string> workspaceVarNames() const;
    std::string workspaceJSON() const;
    void outputText(const std::string &s);

    FigureManager &figureManager() { return figureManager_; }
    const FigureManager &figureManager() const { return figureManager_; }

    Environment &workspaceEnv() { return *workspaceEnv_; }
    Environment &constantsEnv() { return *constantsEnv_; }
    Environment &globalsEnv() { return *globalsEnv_; }

    bool hasFunction(const std::string &name) const;
    bool hasUserFunction(const std::string &name) const;
    bool hasExternalFunction(const std::string &name) const;

    // Clear user-defined functions from both TW and VM stores
    void clearUserFunctions();

    // --- Debugger API ---
    void setDebugObserver(std::shared_ptr<DebugObserver> observer);
    DebugObserver *debugObserver() const { return debugObserver_.get(); }
    BreakpointManager &breakpointManager() { return breakpointManager_; }
    const BreakpointManager &breakpointManager() const { return breakpointManager_; }
    DebugController *debugController() { return debugController_.get(); }
    const DebugController *debugController() const { return debugController_.get(); }

    // Resume paused debug execution with the given action.
    // Returns Paused on next breakpoint/step, Completed if execution finishes.
    // Throws DebugStopException if Stop is requested, MLabError on runtime errors.
    ExecStatus debugResume(DebugAction action);

    // Reinstall built-in constants (pi, eps, inf, etc.) into constantsEnv.
    // Called after clear to restore the standard environment.
    void reinstallConstants();

    // Tic/toc timer access — used by workspace builtins
    void setTicTimer(TimePoint tp)
    {
        ticBase_ = tp;
        ticCalled_ = true;
    }
    TimePoint ticTimer() const { return ticBase_; }
    bool ticWasCalled() const { return ticCalled_; }

    // Returns true when executing inside a user function call (not top-level script).
    // Used by clear() to avoid modifying workspaceEnv from within function scope.
    bool isInsideFunctionCall() const;

private:
    Allocator allocator_;
    std::unique_ptr<Environment> globalsEnv_;     // MATLAB 'global' variables — shared across functions
    std::unique_ptr<Environment> constantsEnv_;  // pi, eps, inf, etc. — parent for all scopes
    std::unique_ptr<Environment> workspaceEnv_;  // top-level workspace (base workspace)

    std::unordered_map<std::string, BinaryOpFunc> binaryOps_;
    std::unordered_map<std::string, UnaryOpFunc> unaryOps_;
    std::unordered_map<std::string, ExternalFunc> externalFuncs_;
    std::unordered_map<std::string, UserFunction> userFuncs_;

    OutputFunc outputFunc_;
    FigureManager figureManager_;

    // Tic/toc timer
    TimePoint ticBase_{};
    bool ticCalled_ = false;

    // Tracks whether clear/clear all was called during VM execution
    // so that export wipes workspaceEnv before writing back.
    bool clearAllCalled_ = false;

    // Debugger
    std::shared_ptr<DebugObserver> debugObserver_;
    BreakpointManager breakpointManager_;
    std::unique_ptr<DebugController> debugController_;  // created when observer is set

public:
    void markClearAll() { clearAllCalled_ = true; }

private:
    std::unique_ptr<TreeWalker> treeWalker_;
    std::unique_ptr<Compiler> compiler_;

public:
    Compiler *compilerPtr() { return compiler_.get(); }

private:
    std::unique_ptr<VM> vm_;
    Backend backend_ = Backend::VM;

    // Sync VM's exported variables to workspaceEnv (called after execute, even on error)
    void syncVMToWorkspace();

    friend class TreeWalker;
    friend class VM;
    friend class Compiler;
    friend class DebugSession;
};

} // namespace mlab