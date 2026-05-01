// include/tree_walker.hpp
#pragma once

#include <numkit/core/debugger.hpp>
#include <numkit/core/types.hpp>

#include <string>
#include <vector>

namespace numkit {

class Engine;

class TreeWalker
{
public:
    explicit TreeWalker(Engine &engine);

    Value execute(const ASTNode *ast, Environment *env);
    void setMaxRecursionDepth(int d) { maxRecursionDepth_ = d; }

    // True when executing inside a user function (not top-level script)
    int callDepth() const { return currentRecursionDepth_; }

    // Active call stack (parallel to C++ recursion of callUserFunction).
    // Maintained via FrameGuard RAII so it stays in sync across exception
    // unwind. Front = outermost; back = innermost. Used by Engine's
    // callerEnv() / inputName() so builtins can introspect the stack
    // regardless of which backend is active.
    struct ActiveFrame
    {
        Environment *env = nullptr;
        // Arg names from caller's CALL site (bare identifier or empty).
        // Read by inputname(k).
        std::vector<std::string> callerArgNames;
    };
    const std::vector<ActiveFrame> &activeFrames() const { return activeFrames_; }

    // ── Public callback API ───────────────────────────────────
    // Used by Engine::callFunctionHandle so that builtins (e.g.
    // cellfun, fzero, integral) can invoke a function handle from C++
    // regardless of the active backend (TW or VM). Both forms also
    // accept anonymous handles — they're stored as synthetic user
    // functions and looked up via lookupUserFunction.
    Value callHandlePublic(const Value &handle,
                            Span<const Value> args,
                            Environment *env);
    std::vector<Value> callHandleMultiPublic(const Value &handle,
                                              Span<const Value> args,
                                              Environment *env,
                                              size_t nout);

private:
    Engine &engine_;

    FlowSignal flowSignal_ = FlowSignal::NONE;
    int currentRecursionDepth_ = 0;
    int maxRecursionDepth_ = 500;

    struct IndexContext
    {
        const Value *array = nullptr;
        int dimension = 0;
        int ndims = 1;
    };
    std::vector<IndexContext> indexContextStack_;
    std::vector<Value> callArgsBuf_;
    std::atomic<int> anonCounter_{0};

    // Parallel call stack of (localEnv, argNames) — pushed on entry to
    // callUserFunction(), popped on exit (RAII). Lifetime safe because
    // each localEnv lives on the C++ stack frame of the same call.
    std::vector<ActiveFrame> activeFrames_;

    class FrameGuard
    {
    public:
        FrameGuard(std::vector<ActiveFrame> &stack, Environment *env,
                   std::vector<std::string> argNames = {})
            : stack_(stack)
        {
            ActiveFrame f;
            f.env = env;
            f.callerArgNames = std::move(argNames);
            stack_.push_back(std::move(f));
        }
        ~FrameGuard() { stack_.pop_back(); }
        FrameGuard(const FrameGuard &) = delete;
        FrameGuard &operator=(const FrameGuard &) = delete;

    private:
        std::vector<ActiveFrame> &stack_;
    };

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
    void displayValue(const std::string &name, const Value &val);

    // Core dispatch
    Value execNode(const ASTNode *node, Environment *env);
    Value execNodeInner(const ASTNode *node, Environment *env);
    Value execBlock(const ASTNode *node, Environment *env);
    bool tryEvalFast(const ASTNode *expr, Environment *env, Value &out);

    // Statements
    Value execIdentifier(const ASTNode *node, Environment *env, size_t nargout = 1);
    Value execAssign(const ASTNode *node, Environment *env);
    Value execMultiAssign(const ASTNode *node, Environment *env);
    Value execBinaryOp(const ASTNode *node, Environment *env);
    Value execUnaryOp(const ASTNode *node, Environment *env);
    Value execCall(const ASTNode *node, Environment *env, size_t nargout = 1);
    Value execCellIndex(const ASTNode *node, Environment *env);
    Value execFieldAccess(const ASTNode *node, Environment *env);
    Value execMatrixLiteral(const ASTNode *node, Environment *env);
    Value execCellLiteral(const ASTNode *node, Environment *env);
    Value execColonExpr(const ASTNode *node, Environment *env);
    Value execIf(const ASTNode *node, Environment *env);
    Value execFor(const ASTNode *node, Environment *env);
    Value execWhile(const ASTNode *node, Environment *env);
    Value execSwitch(const ASTNode *node, Environment *env);
    Value execFunctionDef(const ASTNode *node, Environment *env);
    Value execExprStmt(const ASTNode *node, Environment *env);
    Value execCommandCall(const ASTNode *node, Environment *env);
    Value execAnonFunc(const ASTNode *node, Environment *env);
    Value execTryCatch(const ASTNode *node, Environment *env);
    Value execDeleteAssign(const ASTNode *node, Environment *env);
    Value execGlobalPersistent(const ASTNode *node, Environment *env);

    // Assignment helpers
    void execIndexedAssign(const ASTNode *lhs, const Value &rhs, Environment *env);
    void execFieldAssign(const ASTNode *lhs, const Value &rhs, Environment *env);
    void execCellAssign(const ASTNode *lhs, const Value &rhs, Environment *env);
    Value &resolveFieldLValue(const ASTNode *node, Environment *env);

    // Indexing helpers
    bool tryResolveScalarIndex(const ASTNode *indexExpr,
                               const Value &array,
                               int dim,
                               int ndims,
                               Environment *env,
                               size_t &outIdx);
    std::vector<size_t> resolveIndex(
        const ASTNode *indexExpr, const Value &array, int dim, int ndims, Environment *env);
    Value execIndexAccess(const Value &var, const ASTNode *callNode, Environment *env);
    std::vector<Value> execCallMulti(const ASTNode *node, Environment *env, size_t nout);

    // Function call
    // Optional `callNode` is the AST CALL node used to extract caller's
    // arg names for inputname(k); nullptr falls back to no names.
    Value callUserFunction(const UserFunction &func, Span<const Value> args,
                           Environment *env,
                           const ASTNode *callNode = nullptr);
    std::vector<Value> callUserFunctionMulti(const UserFunction &func,
                                              Span<const Value> args,
                                              Environment *env,
                                              size_t nout,
                                              const ASTNode *callNode = nullptr);
    Value callFuncHandle(const Value &handle, Span<const Value> args, Environment *env);
    std::vector<Value> callFuncHandleMulti(const Value &handle,
                                            Span<const Value> args,
                                            Environment *env,
                                            size_t nout);

    // Utilities
    bool isKnownFunction(const std::string &name) const;

    // ── Debugger ──────────────────────────────────────────
    std::string topLevelName_{"<script>"};
    DebugController *debugCtl();
};

} // namespace numkit