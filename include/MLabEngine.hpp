#pragma once

#include "MLabAllocator.hpp"
#include "MLabAst.hpp"
#include "MLabEnvironment.hpp"
#include "MLabValue.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace mlab {

using ExternalFunc = std::function<std::vector<MValue>(const std::vector<MValue> &)>;
using BinaryOpFunc = std::function<MValue(const MValue &, const MValue &)>;
using UnaryOpFunc = std::function<MValue(const MValue &)>;

struct BreakSignal
{};
struct ContinueSignal
{};
struct ReturnSignal
{};

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

private:
    Allocator allocator_;
    GlobalStore globalStore_;
    std::shared_ptr<Environment> globalEnv_;

    std::unordered_map<std::string, BinaryOpFunc> binaryOps_;
    std::unordered_map<std::string, UnaryOpFunc> unaryOps_;
    std::unordered_map<std::string, ExternalFunc> externalFuncs_;
    std::unordered_map<std::string, UserFunction> userFuncs_;

    OutputFunc outputFunc_;
    int maxRecursionDepth_ = 500;
    int currentRecursionDepth_ = 0;
    std::atomic<int> anonCounter_{0};

    struct IndexContext
    {
        const MValue *array = nullptr;
        int dimension = 0;
        int ndims = 1;
    };
    std::vector<IndexContext> indexContextStack_;

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

    MValue execNode(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execBlock(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execIdentifier(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execAssign(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execMultiAssign(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execBinaryOp(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execUnaryOp(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execCall(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execCellIndex(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execFieldAccess(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execMatrixLiteral(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execCellLiteral(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execColonExpr(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execIf(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execFor(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execWhile(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execSwitch(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execFunctionDef(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execExprStmt(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execAnonFunc(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execTryCatch(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execDeleteAssign(const ASTNode *node, std::shared_ptr<Environment> env);
    MValue execGlobalPersistent(const ASTNode *node, std::shared_ptr<Environment> env);

    void execIndexedAssign(const ASTNode *lhs, const MValue &rhs, std::shared_ptr<Environment> env);
    void execFieldAssign(const ASTNode *lhs, const MValue &rhs, std::shared_ptr<Environment> env);
    void execCellAssign(const ASTNode *lhs, const MValue &rhs, std::shared_ptr<Environment> env);
    MValue &resolveFieldLValue(const ASTNode *node, std::shared_ptr<Environment> env);

    std::vector<size_t> resolveIndex(const ASTNode *indexExpr,
                                     const MValue &array,
                                     int dim,
                                     int ndims,
                                     std::shared_ptr<Environment> env);
    MValue execIndexAccess(const MValue &var,
                           const ASTNode *callNode,
                           std::shared_ptr<Environment> env);
    std::vector<MValue> execCallMulti(const ASTNode *node,
                                      std::shared_ptr<Environment> env,
                                      size_t nout);

    MValue callUserFunction(const UserFunction &func,
                            const std::vector<MValue> &args,
                            std::shared_ptr<Environment> env);
    std::vector<MValue> callUserFunctionMulti(const UserFunction &func,
                                              const std::vector<MValue> &args,
                                              std::shared_ptr<Environment> env,
                                              size_t nout);
    MValue callFuncHandle(const MValue &handle,
                          const std::vector<MValue> &args,
                          std::shared_ptr<Environment> env);
    std::vector<MValue> callFuncHandleMulti(const MValue &handle,
                                            const std::vector<MValue> &args,
                                            std::shared_ptr<Environment> env,
                                            size_t nout);

    bool isKnownFunction(const std::string &name) const;
    double colonCount(double start, double step, double stop) const;

private:
    bool tryBuiltinCall(const std::string &name,
                        const std::vector<MValue> &args,
                        std::shared_ptr<Environment> env,
                        MValue &result);
};

} // namespace mlab
