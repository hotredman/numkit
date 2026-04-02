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
    enum class Backend { TreeWalker, VM };
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

    bool hasFunction(const std::string &name) const;

    // Clear user-defined functions from both TW and VM stores
    void clearUserFunctions();

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

    std::unique_ptr<TreeWalker> treeWalker_;
    std::unique_ptr<Compiler> compiler_;
    std::unique_ptr<VM> vm_;
    Backend backend_ = Backend::TreeWalker;

    void reinstallConstants();
    friend class TreeWalker;
    friend class VM;
    friend class Compiler;
};

} // namespace mlab