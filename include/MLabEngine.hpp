#pragma once

#include "MLabAllocator.hpp"
#include "MLabAst.hpp"
#include "MLabEnvironment.hpp"
#include "MLabFigureManager.hpp"
#include "MLabSpan.hpp"
#include "MLabValue.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace mlab {

using ExternalFunc = std::function<void(Span<const MValue> args, size_t nargout, Span<MValue> outs)>;
using BinaryOpFunc = std::function<MValue(const MValue &, const MValue &)>;
using UnaryOpFunc = std::function<MValue(const MValue &)>;

struct BreakSignal
{};
struct ContinueSignal
{};
struct ReturnSignal
{};

/** Runtime error with source location info */
class MLabError : public std::runtime_error
{
public:
    MLabError(const std::string &msg, int line = 0, int col = 0)
        : std::runtime_error(msg)
        , line_(line)
        , col_(col)
    {}
    int line() const { return line_; }
    int col() const { return col_; }

private:
    int line_;
    int col_;
};

struct UserFunction
{
    std::string name;
    std::vector<std::string> params;
    std::vector<std::string> returns;
    std::shared_ptr<const ASTNode> body;
    std::shared_ptr<Environment> closureEnv;
};

class Engine
{
public:
    Engine();

    Engine(const Engine &) = delete;
    Engine &operator=(const Engine &) = delete;
    Engine(Engine &&) = delete;
    Engine &operator=(Engine &&) = delete;

    void setAllocator(Allocator alloc);
    Allocator &allocator();

    void registerBinaryOp(const std::string &op, BinaryOpFunc func);
    void registerUnaryOp(const std::string &op, UnaryOpFunc func);
    void registerFunction(const std::string &name, ExternalFunc func);

    void setVariable(const std::string &name, MValue val);
    MValue *getVariable(const std::string &name);

    MValue eval(const std::string &code);

    using OutputFunc = std::function<void(const std::string &)>;
    void setOutputFunc(OutputFunc f);
    void setMaxRecursionDepth(int depth);

    /** Return names of user variables in global scope (excluding builtins) */
    std::vector<std::string> globalVarNames() const;
    /** Return JSON workspace snapshot for the web inspector */
    std::string workspaceJSON() const;

    /** Output text via the configured output function (or stdout) */
    void outputText(const std::string &s);

    /** Figure manager — owned by Engine, reset on Engine recreation */
    FigureManager &figureManager() { return figureManager_; }
    const FigureManager &figureManager() const { return figureManager_; }

private:
    Allocator allocator_;
    GlobalStore globalStore_;
    std::unique_ptr<Environment> globalEnv_;

    std::unordered_map<std::string, BinaryOpFunc> binaryOps_;
    std::unordered_map<std::string, UnaryOpFunc> unaryOps_;
    std::unordered_map<std::string, ExternalFunc> externalFuncs_;
    std::unordered_map<std::string, UserFunction> userFuncs_;

    OutputFunc outputFunc_;
    int maxRecursionDepth_ = 500;
    int currentRecursionDepth_ = 0;
    std::atomic<int> anonCounter_{0};

    FigureManager figureManager_;

    // Tic/toc timer support
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    TimePoint ticBase_{};
    bool ticCalled_ = false;

    struct IndexContext
    {
        const MValue *array = nullptr;
        int dimension = 0;
        int ndims = 1;
    };
    std::vector<IndexContext> indexContextStack_;

    // Reusable buffer for small function call args (avoids heap alloc per call)
    std::vector<MValue> callArgsBuf_;

    // Pool of reusable Environment objects (avoids make_shared per function call)

    class IndexContextGuard
    {
    public:
        IndexContextGuard(std::vector<IndexContext> &stack, IndexContext ctx)
            : stack_(stack)
        {
            stack_.push_back(ctx);
        }
        ~IndexContextGuard() { stack_.pop_back(); }

        IndexContextGuard(const IndexContextGuard &) = delete;
        IndexContextGuard &operator=(const IndexContextGuard &) = delete;

    private:
        std::vector<IndexContext> &stack_;
    };

    class RecursionGuard
    {
    public:
        RecursionGuard(int &depth, int maxDepth)
            : depth_(depth)
        {
            if (++depth_ > maxDepth)
                throw std::runtime_error("Maximum recursion depth (" + std::to_string(maxDepth)
                                         + ") exceeded");
        }
        ~RecursionGuard() { --depth_; }

        RecursionGuard(const RecursionGuard &) = delete;
        RecursionGuard &operator=(const RecursionGuard &) = delete;

    private:
        int &depth_;
    };

    void output(const std::string &s);
    void displayValue(const std::string &name, const MValue &val);

    MValue execNode(const ASTNode *node, Environment *env);
    MValue execBlock(const ASTNode *node, Environment *env);
    bool tryEvalScalar(const ASTNode *expr, Environment *env, double &out);
    MValue execIdentifier(const ASTNode *node, Environment *env);
    MValue execAssign(const ASTNode *node, Environment *env);
    MValue execMultiAssign(const ASTNode *node, Environment *env);
    MValue execBinaryOp(const ASTNode *node, Environment *env);
    MValue execUnaryOp(const ASTNode *node, Environment *env);
    MValue execCall(const ASTNode *node, Environment *env);
    MValue execCellIndex(const ASTNode *node, Environment *env);
    MValue execFieldAccess(const ASTNode *node, Environment *env);
    MValue execMatrixLiteral(const ASTNode *node, Environment *env);
    MValue execCellLiteral(const ASTNode *node, Environment *env);
    MValue execColonExpr(const ASTNode *node, Environment *env);
    MValue execIf(const ASTNode *node, Environment *env);
    MValue execFor(const ASTNode *node, Environment *env);
    MValue execWhile(const ASTNode *node, Environment *env);
    MValue execSwitch(const ASTNode *node, Environment *env);
    MValue execFunctionDef(const ASTNode *node, Environment *env);
    MValue execExprStmt(const ASTNode *node, Environment *env);
    MValue execCommandCall(const ASTNode *node, Environment *env);
    MValue execAnonFunc(const ASTNode *node, Environment *env);
    MValue execTryCatch(const ASTNode *node, Environment *env);
    MValue execDeleteAssign(const ASTNode *node, Environment *env);
    MValue execGlobalPersistent(const ASTNode *node, Environment *env);

    void execIndexedAssign(const ASTNode *lhs, const MValue &rhs, Environment *env);
    void execFieldAssign(const ASTNode *lhs, const MValue &rhs, Environment *env);
    void execCellAssign(const ASTNode *lhs, const MValue &rhs, Environment *env);
    MValue &resolveFieldLValue(const ASTNode *node, Environment *env);

    bool tryResolveScalarIndex(const ASTNode *indexExpr,
                               const MValue &array,
                               int dim,
                               int ndims,
                               Environment *env,
                               size_t &outIdx);
    std::vector<size_t> resolveIndex(
        const ASTNode *indexExpr, const MValue &array, int dim, int ndims, Environment *env);
    MValue execIndexAccess(const MValue &var, const ASTNode *callNode, Environment *env);
    std::vector<MValue> execCallMulti(const ASTNode *node, Environment *env, size_t nout);

    MValue callUserFunction(const UserFunction &func, Span<const MValue> args, Environment *env);
    std::vector<MValue> callUserFunctionMulti(const UserFunction &func,
                                              Span<const MValue> args,
                                              Environment *env,
                                              size_t nout);
    MValue callFuncHandle(const MValue &handle, Span<const MValue> args, Environment *env);
    std::vector<MValue> callFuncHandleMulti(const MValue &handle,
                                            Span<const MValue> args,
                                            Environment *env,
                                            size_t nout);

    bool isKnownFunction(const std::string &name) const;
    double colonCount(double start, double step, double stop) const;
    void reinstallConstants();

private:
    bool tryBuiltinCall(const std::string &name,
                        Span<const MValue> args,
                        Environment *env,
                        MValue &result,
                        size_t nargout = 0);
};

} // namespace mlab