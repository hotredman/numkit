// include/MTreeWalker.hpp
#pragma once

#include <numkit/m/core/MDebugger.hpp>
#include <numkit/m/core/MTypes.hpp>

#include <string>
#include <vector>

namespace numkit::m {

class Engine;

class TreeWalker
{
public:
    explicit TreeWalker(Engine &engine);

    MValue execute(const ASTNode *ast, Environment *env);
    void setMaxRecursionDepth(int d) { maxRecursionDepth_ = d; }

    // True when executing inside a user function (not top-level script)
    int callDepth() const { return currentRecursionDepth_; }

private:
    Engine &engine_;

    FlowSignal flowSignal_ = FlowSignal::NONE;
    int currentRecursionDepth_ = 0;
    int maxRecursionDepth_ = 500;

    struct IndexContext
    {
        const MValue *array = nullptr;
        int dimension = 0;
        int ndims = 1;
    };
    std::vector<IndexContext> indexContextStack_;
    std::vector<MValue> callArgsBuf_;
    std::atomic<int> anonCounter_{0};

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

    // Display helpers
    void output(const std::string &s);
    void displayValue(const std::string &name, const MValue &val);

    // Core dispatch
    MValue execNode(const ASTNode *node, Environment *env);
    MValue execNodeInner(const ASTNode *node, Environment *env);
    MValue execBlock(const ASTNode *node, Environment *env);
    bool tryEvalFast(const ASTNode *expr, Environment *env, MValue &out);

    // Statements
    MValue execIdentifier(const ASTNode *node, Environment *env, size_t nargout = 1);
    MValue execAssign(const ASTNode *node, Environment *env);
    MValue execMultiAssign(const ASTNode *node, Environment *env);
    MValue execBinaryOp(const ASTNode *node, Environment *env);
    MValue execUnaryOp(const ASTNode *node, Environment *env);
    MValue execCall(const ASTNode *node, Environment *env, size_t nargout = 1);
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

    // Assignment helpers
    void execIndexedAssign(const ASTNode *lhs, const MValue &rhs, Environment *env);
    void execFieldAssign(const ASTNode *lhs, const MValue &rhs, Environment *env);
    void execCellAssign(const ASTNode *lhs, const MValue &rhs, Environment *env);
    MValue &resolveFieldLValue(const ASTNode *node, Environment *env);

    // Indexing helpers
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

    // Function call
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

    // Utilities
    bool isKnownFunction(const std::string &name) const;

    // ── Debugger ──────────────────────────────────────────
    std::string topLevelName_{"<script>"};
    DebugController *debugCtl();
};

} // namespace numkit::m