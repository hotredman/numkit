// include/MLabCompiler.hpp
#pragma once

#include "MAst.hpp"
#include "MBytecode.hpp"
#include <bitset>
#include <unordered_set>

#include <string>
#include <unordered_map>
#include <vector>

namespace numkit {

class Engine;

class Compiler
{
    friend class IndexContextGuard;

public:
    explicit Compiler(Engine &engine);

    BytecodeChunk compile(const ASTNode *ast,
                          std::shared_ptr<const std::string> sourceCode = nullptr);

    // Compile a function definition into a BytecodeChunk
    BytecodeChunk compileFunction(const ASTNode *funcDef,
                                  std::shared_ptr<const std::string> sourceCode = nullptr);

    // Register a FUNCTION_DEF node in the currently-active compiled
    // function table (+ engine.{script,user}Funcs_) without running
    // it. Used by Engine::beginScript to pre-compile a script's
    // local functions into the script-scope bucket, and by the
    // split-mode driver so forward references resolve.
    void registerFunction(const ASTNode *funcDef);

    // Workspace-scope compiled functions. Populated by `function` at
    // the REPL or by anonymous-function allocation. Cleared by
    // `clear all` / `clear functions` (see Engine::clearUserFunctions).
    const std::unordered_map<std::string, BytecodeChunk> &compiledFuncs() const
    {
        return compiledFuncs_;
    }
    void clearCompiledFuncs() { compiledFuncs_.clear(); }

    // Script-scope compiled functions. Populated by beginScriptScope
    // via compileFunctionDef routing. NEVER cleared by
    // clearCompiledFuncs — that's the whole point of keeping this
    // bucket separate. Its lifetime is bounded by beginScriptScope /
    // endScriptScope pairs.
    const std::unordered_map<std::string, BytecodeChunk> &scriptLocalCompiledFuncs() const
    {
        return scriptLocalCompiledFuncs_;
    }

    // Unified lookup used by dispatch sites outside the compiler —
    // tries script-scope first, then workspace-scope. Returns
    // nullptr if unknown.
    const BytecodeChunk *findCompiled(const std::string &name) const;

    // Enter/leave a script-lexical scope. While inside, compiled
    // FUNCTION_DEFs land in scriptLocalCompiledFuncs_ rather than
    // the workspace map. Nestable — each pair push/pop an isolated
    // bucket, so a recursive eval's inner script can't leak into
    // the outer one.
    void beginScriptScope();
    void endScriptScope();
    bool inScriptScope() const { return scriptDepth_ > 0; }

    // Peer of Engine::promoteScriptLocalsToWorkspace: copy the
    // script-scope compiled chunks into the workspace bucket so
    // their VM dispatches keep resolving after the scope ends.
    void promoteScriptLocalsToWorkspace();

    // Debug: dump bytecode
    static std::string disassemble(const BytecodeChunk &chunk);

private:
    Engine &engine_;

    // Current chunk being compiled
    BytecodeChunk chunk_;

    // Register allocation: variable name → register index
    std::unordered_map<std::string, uint8_t> varRegisters_;
    uint8_t nextReg_ = 0;
    int anonCounter_ = 0;
    bool isTopLevel_ = false;
    uint8_t nargoutContext_ = 1; // expected number of outputs (0=statement, 1=expression)

    // Index context for END_VAL compilation
    uint8_t indexContextArr_ = 0;
    uint8_t indexContextDim_ = 0;
    uint8_t indexContextNdims_ = 1;

    // Current source location (updated before compiling each node)
    SourceLoc currentLoc_{};

    // Compiled function table (persists across compile() calls) —
    // workspace-scope (cleared by `clear all`/`clear functions`).
    std::unordered_map<std::string, BytecodeChunk> compiledFuncs_;

    // Script-lexical compiled functions. Separated from workspace-
    // scope so `clear all` mid-script can't wipe them — MATLAB
    // treats local functions as part of the script's code, not its
    // workspace. Managed by begin/endScriptScope; nesting via the
    // save stack.
    std::unordered_map<std::string, BytecodeChunk> scriptLocalCompiledFuncs_;
    std::vector<std::unordered_map<std::string, BytecodeChunk>> savedScriptLocalCompiledFuncs_;
    int scriptDepth_ = 0;

    // ── Variable register access API ────────────────────────────
    //
    // Every assignment-like compile site MUST call `varRegWrite(name)` so
    // BytecodeChunk::assignedVars is populated — the debug workspace uses
    // it to distinguish "user assigned" from "just read" (MATLAB's
    // whos-parity for shadowed built-ins). Reads and pure re-lookups use
    // the other two methods.
    //
    //   varRegWrite(name)  — allocates + marks as assigned. Use for every
    //                        AST node that writes to `name`.
    //   varRegRead(name)   — allocates + emits ASSERT_DEF. Use for checked
    //                        reads at the AST level.
    //   varRegLookup(name) — allocates without marking or asserting. Use
    //                        for internal plumbing: re-fetching a register
    //                        for an already-allocated variable, pre-loading
    //                        pseudo-vars (nargin/nargout), workspace-env
    //                        imports, etc. NEVER use at a user-assignment
    //                        site — that would defeat the whole point.
    //
    // `markAssigned(name)` is a rare helper for write sites that bypass
    // varRegWrite (e.g. move-elimination in compileAssign that writes via
    // `varRegisters_[name] = src` directly).
    uint8_t varRegWrite(const std::string &name);
    uint8_t varRegRead(const std::string &name);
    uint8_t varRegLookup(const std::string &name);
    void markAssigned(const std::string &name) { chunk_.assignedVars.insert(name); }
    // Pre-import global variables before compiling AST
    void preImportGlobals(const ASTNode *ast);
    void collectAllIdentifiers(const ASTNode *node, std::unordered_set<std::string> &out);
    // Allocate a temporary register
    uint8_t tempReg();

    // Emit instructions
    void emit(Instruction instr);
    void emitABC(OpCode op, uint8_t a, uint8_t b, uint8_t c);
    void emitAB(OpCode op, uint8_t a, uint8_t b);
    void emitAD(OpCode op, uint8_t a, int16_t d);
    void emitA(OpCode op, uint8_t a);
    void emitD(OpCode op, int16_t d);
    void emitNone(OpCode op);

    // Current instruction index (for patching jumps)
    size_t currentPos() const;
    // Patch a jump instruction's offset at given position
    void patchJump(size_t instrPos, int16_t offset);

    // Add constant to pool, return index
    int16_t addConstant(double value);
    int16_t addStringConstant(const std::string &s);

    // Compile AST nodes — return register holding result
    uint8_t compileNode(const ASTNode *node);
    uint8_t compileBlock(const ASTNode *node);
    uint8_t compileNumber(const ASTNode *node);
    uint8_t compileString(const ASTNode *node);
    uint8_t compileBool(const ASTNode *node);
    uint8_t compileIdentifier(const ASTNode *node);
    uint8_t compileAssign(const ASTNode *node);
    uint8_t compileMultiAssign(const ASTNode *node);
    uint8_t compileBinaryOp(const ASTNode *node);
    uint8_t compileUnaryOp(const ASTNode *node);
    uint8_t compileExprStmt(const ASTNode *node);

    // Phase 2: control flow
    uint8_t compileIf(const ASTNode *node);
    uint8_t compileSwitch(const ASTNode *node);
    uint8_t compileTryCatch(const ASTNode *node);
    uint8_t compileGlobalPersistent(const ASTNode *node);
    uint8_t compileFieldAccess(const ASTNode *node);
    uint8_t compileFieldAssign(const ASTNode *node);
    uint8_t compileCellLiteral(const ASTNode *node);
    uint8_t compileCellIndex(const ASTNode *node);
    uint8_t compileCellAssign(const ASTNode *node);
    uint8_t compileAnonFunc(const ASTNode *node);
    void collectFreeVars(const ASTNode *node,
                         const std::vector<std::string> &params,
                         std::vector<std::string> &freeVars);
    uint8_t compileWhile(const ASTNode *node);
    uint8_t compileBreak(const ASTNode *node);
    uint8_t compileContinue(const ASTNode *node);

    // Phase 3: for-loop, colon, arrays
    uint8_t compileFor(const ASTNode *node);
    uint8_t compileColonExpr(const ASTNode *node);
    uint8_t compileMatrixLiteral(const ASTNode *node);
    uint8_t compileIndexExpr(const ASTNode *node);
    uint8_t compileIndexAssign(const ASTNode *node);

    // Phase 4+5: function calls
    uint8_t compileCall(const ASTNode *node);
    uint8_t compileCommandCall(const ASTNode *node);
    uint8_t compileFunctionDef(const ASTNode *node);
    uint8_t compileReturn(const ASTNode *node);
    uint8_t compileDeleteAssign(const ASTNode *node);
    static int8_t resolveBuiltinId(const std::string &name, size_t nargs);

    // Constant register cache: avoid redundant LOAD_CONST in loops
    // Maps constant pool index → register that already holds the value
    std::unordered_map<int16_t, uint8_t> constRegCache_;

    // Scalar register tracking: registers known to hold double scalars
    // Enables emission of type-check-free _SS opcodes
    std::bitset<256> scalarRegs_;

    // Peephole optimization pass
    void peepholeOptimize();

    // Break/continue loop patching
    struct LoopContext
    {
        std::vector<size_t> breakPatches;
        std::vector<size_t> continuePatches;
        bool isForLoop = false; // true for for-loops (need forStack_ pop on break)
    };
    std::vector<LoopContext> loopStack_;
};

} // namespace numkit