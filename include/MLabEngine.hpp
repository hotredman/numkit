// include/MLabEngine.hpp
#pragma once

#include "MLabFigureManager.hpp"
#include "MLabTypes.hpp"

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

    // Backend selection
    // AutoFallback = VM with silent fallback to TW (default, production)
    // VM = VM only, no fallback (for testing)
    // TreeWalker = TW only (for testing)
    enum class Backend { AutoFallback, TreeWalker, VM };
    void setBackend(Backend b) { backend_ = b; }
    Backend backend() const { return backend_; }

    using OutputFunc = std::function<void(const std::string &)>;
    void setOutputFunc(OutputFunc f);
    void setMaxRecursionDepth(int depth);

    std::vector<std::string> globalVarNames() const;
    std::string workspaceJSON() const;
    void outputText(const std::string &s);

    FigureManager &figureManager() { return figureManager_; }
    const FigureManager &figureManager() const { return figureManager_; }

    Environment &globalEnvironment() { return *globalEnv_; }
    GlobalStore &globalVarStore() { return globalStore_; }

    bool hasFunction(const std::string &name) const;
    bool hasUserFunction(const std::string &name) const;
    bool hasExternalFunction(const std::string &name) const;

    // Clear user-defined functions from both TW and VM stores
    void clearUserFunctions();

    // Reinstall built-in constants (pi, eps, inf, etc.) into globalEnv.
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
    // Used by clear() to avoid modifying globalEnv from within function scope.
    bool isInsideFunctionCall() const;

private:
    Allocator allocator_;
    GlobalStore globalStore_;
    std::unique_ptr<Environment> globalEnv_;

    std::unordered_map<std::string, BinaryOpFunc> binaryOps_;
    std::unordered_map<std::string, UnaryOpFunc> unaryOps_;
    std::unordered_map<std::string, ExternalFunc> externalFuncs_;
    std::unordered_map<std::string, UserFunction> userFuncs_;

    OutputFunc outputFunc_;
    FigureManager figureManager_;

    // Tic/toc timer
    TimePoint ticBase_{};
    bool ticCalled_ = false;

    // Tracks variables cleared mid-execution by clear('x') so that
    // Engine::eval's post-VM export skips them instead of resurrecting.
    std::unordered_set<std::string> clearedVars_;
    bool clearAllCalled_ = false;

public:
    // Called by registered clear() to coordinate with VM export
    void markVarCleared(const std::string &name) { clearedVars_.insert(name); }
    void markClearAll() { clearAllCalled_ = true; }

private:
    std::unique_ptr<TreeWalker> treeWalker_;
    std::unique_ptr<Compiler> compiler_;

public:
    Compiler *compilerPtr() { return compiler_.get(); }

private:
    std::unique_ptr<VM> vm_;
    Backend backend_ = Backend::VM;

    friend class TreeWalker;
    friend class VM;
    friend class Compiler;
};

} // namespace mlab