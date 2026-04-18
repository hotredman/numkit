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
#include <unordered_set>
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

    // Clear user-defined functions from both TW and VM stores.
    // Script-local functions of the currently-running script (see
    // beginScript/endScript) are re-installed immediately afterward —
    // MATLAB treats file-scoped functions as lexically part of the
    // script, so `clear all` mid-execution must not make them
    // disappear from under the calls that follow.
    void clearUserFunctions();

    // Mark the entry/exit of a top-level script or function evaluation.
    // While a script is active, any FUNCTION_DEF children are treated
    // as the script's local functions and preserved across
    // clearUserFunctions. Nesting is supported via an internal save
    // stack, so recursive eval() calls don't lose their outer scope.
    //
    // The AST pointer must outlive the matching endScript() — the
    // caller (eval, DebugSession) owns lifetime.
    void beginScript(const ASTNode *ast);
    void endScript();

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
    // Called after clear to restore the standard environment. Also
    // re-installs any constants the host has registered via
    // registerConstant() so `clear all` doesn't wipe them.
    void reinstallConstants();

    // Register a host-level constant — visible to every script as if it
    // were `pi`/`eps`/etc. Does not appear in `whos` or the debug
    // Workspace panel, can be shadowed by `name = …` and un-shadowed by
    // `clear name`, and survives `clear all`.
    void registerConstant(const std::string &name, MValue val);

    // Is `name` a reserved name (MATLAB built-in OR host-registered
    // constant)? Used by the compiler / VM / debug workspace to hide
    // these names from user-workspace views and skip unnecessary
    // runtime safety checks.
    bool isReservedName(const std::string &name) const;

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

    // Names declared `global` at the base workspace level. Populated after
    // each top-level chunk executes; the compiler mirrors them into every
    // subsequent chunk's globalNames so that a split-mode
    // `global X; X = 0;` keeps routing X through globalsEnv_, matching the
    // single-chunk path.
    std::unordered_set<std::string> topLevelGlobals_;

    // Host-registered constants — (name → value). Restored into
    // constantsEnv_ alongside the built-ins by reinstallConstants() so
    // `clear all` does not drop them. Name presence also feeds
    // isReservedName() so these behave the same as `pi`/`eps`/…
    std::unordered_map<std::string, MValue> userConstants_;

    // AST pointers to the currently-running script's local functions
    // (populated by beginScript from top-level FUNCTION_DEFs). Used by
    // clearUserFunctions to re-install them so a mid-script `clear all`
    // doesn't strand later calls with "undefined function". The
    // pointers are non-owning — callers (eval, DebugSession) keep the
    // AST alive across the script's lifetime. Nested scripts push
    // their previous state onto savedScriptLocalFuncs_.
    std::vector<const ASTNode *> scriptLocalFuncs_;
    std::vector<std::vector<const ASTNode *>> savedScriptLocalFuncs_;

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

    // Compile one AST subtree as a VM chunk, run it, sync registers to
    // workspaceEnv. eval() calls this once per top-level statement when
    // splitting a multi-statement script for MATLAB-parity semantics.
    MValue runOneChunk(const ASTNode *ast, std::shared_ptr<const std::string> src);

    friend class TreeWalker;
    friend class VM;
    friend class Compiler;
    friend class DebugSession;
};

} // namespace mlab