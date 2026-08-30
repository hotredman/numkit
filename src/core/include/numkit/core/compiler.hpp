// include/compiler.hpp
#pragma once

#include <numkit/core/ast.hpp>
#include <numkit/core/bytecode.hpp>
#include <bitset>
#include <unordered_set>

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace numkit {

class Engine;
struct UserFunction; // defined in types.hpp; used by-reference here

// Thrown when a single chunk needs more than the 255 register slots the
// bytecode operand byte can address. Distinct type (not a bare runtime_error)
// so callers can tell "function too large for the VM" apart from other compile
// failures and surface it instead of silently dropping VM compilation. Derives
// from runtime_error so existing broad `catch (std::exception&)` TW-fallbacks
// (class methods) keep working.
struct RegisterExhaustionError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

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

    // Same as above but binds the function under an explicit name
    // instead of funcDef->strValue. resolveMFile_ uses this for
    // package-qualified m-files (`+pkg/foo.m` is bound under
    // "pkg.foo", not the leaf "foo", so two packages with the same
    // leaf don't collide in compiledFuncs_).
    void registerFunctionAs(const std::string &qualifiedName,
                             const ASTNode *funcDef);

    // Compile a classdef method / constructor body (reconstructed from its
    // UserFunction) into the GLOBAL compiled-function table under `uf.name`
    // ("Class>member"), returning the chunk. Idempotent. Unlike
    // registerFunctionAs this never lands in the script-local bucket —
    // classdef bodies persist with the class across evals. Lets the VM run
    // classdef method bodies as native frames (see vm_callbacks_plan.md).
    const BytecodeChunk *ensureClassMethodCompiled(const UserFunction &uf);

    // Workspace-scope compiled functions. Populated by `function` at
    // the REPL or by anonymous-function allocation. Cleared by
    // `clear all` / `clear functions` (see Engine::clearUserFunctions).
    const std::unordered_map<std::string, BytecodeChunk> &compiledFuncs() const
    {
        return compiledFuncs_;
    }
    void clearCompiledFuncs() { compiledFuncs_.clear(); }
    // Drop a single compiled chunk by name (used by rehash to invalidate just
    // the reloaded m-file, not every compiled function).
    void eraseCompiledFunc(const std::string &name) { compiledFuncs_.erase(name); }
    // Drop every compiled classdef-method chunk for `className` — keys of the
    // form "ClassName>method" — plus any "uncompilable" marks, so redefining
    // the class recompiles its bodies from scratch instead of reusing the
    // stale cached chunks. See Engine::unregisterClassDef.
    void eraseCompiledFuncsForClass(const std::string &className);

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
    // nextReg_ is `int` (not uint8_t) so we can detect overflow (>256)
    // before it silently wraps. Bytecode reg fields are uint8_t — so we
    // must throw cleanly when a chunk's allocation hits that hard limit
    // rather than corrupt low/variable slots. See peakReg_ for the value
    // stored in BytecodeChunk::numRegisters.
    int nextReg_ = 0;
    // High-water mark for nextReg_ across the chunk. Used at end of
    // compilation to size the runtime register file. Distinct from
    // nextReg_, which may shrink when compileBlock releases temps at
    // statement boundaries (see end of compileBlock).
    int peakReg_ = 0;
    // Highest variable register reserved so far (exclusive). Slots
    // [0, maxVarReg_) are pinned to variables and never reclaimable;
    // [maxVarReg_, nextReg_) are transient temps that compileBlock
    // releases between statements.
    int maxVarReg_ = 0;
    int anonCounter_ = 0;
    bool isTopLevel_ = false;
    // True while compiling a fusion idiom's fallback path (the normal compile),
    // so the nested compileCall doesn't re-attempt fusion (infinite recursion).
    bool inFusionFallback_ = false;
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

    // classdef method bodies that the VM compiler can't yet compile (e.g. a
    // super-call before that lands in the VM) — cached so dispatch falls back
    // to the TW hook without re-attempting the compile every call.
    std::unordered_set<std::string> uncompilableClassMethods_;

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
    // preloadReserved: when first allocating a register for a reserved name
    // (pi/eps/i/j/…), emit a LOAD_CONST of the engine's value. Correct for a
    // *read* of the constant; must be false when allocating for a parameter
    // or assignment target named like a constant (e.g. a param `i`), whose
    // value comes from the call/assignment — otherwise the pre-load clobbers
    // the argument with the imaginary unit.
    uint8_t varRegLookup(const std::string &name, bool preloadReserved = true);
    void markAssigned(const std::string &name) { chunk_.assignedVars.insert(name); }
    // Pre-import global variables before compiling AST
    void preImportGlobals(const ASTNode *ast);
    void collectAllIdentifiers(const ASTNode *node, std::unordered_set<std::string> &out);
    // Scan AST for names that get ASSIGNED somewhere — preImportGlobals
    // pre-allocates a low slot for each so vars cluster at the bottom
    // and don't fragment the slot range under temp pressure.
    void collectAssignedNames(const ASTNode *node, std::vector<std::string> &out);
    // Cluster every variable assigned in `body` at a low, contiguous register
    // slot BEFORE compiling the body. Without this, a local's slot is whatever
    // temp happened to be free at its first assignment (high when the RHS is a
    // deep expression) — fragmenting the slot range, creeping maxVarReg_ (the
    // statement-boundary temp-release floor) upward, and exhausting the 256-slot
    // chunk on functions that don't need that many slots. Run for BOTH the
    // top-level script (via preImportGlobals) and every user function
    // (compileFunction) so they allocate identically.
    void preallocateAssignedVars(const ASTNode *body);
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
    // compileNode is the single-value entry: it compiles the node, then (when the node
    // is a brace cell-index that may yield a comma-separated list) emits COLLAPSE on the
    // result register, so operands / conditions / index values / assignment RHS never
    // see a CSL. Splice contexts (call args, [..], {..}, multi-assign) call
    // compileNodeExpand instead, which leaves a CSL for the consumer to flatten.
    // Mirrors the TreeWalker execNode / execNodeExpand split. See csl_first_class.md.
    uint8_t compileNode(const ASTNode *node);
    uint8_t compileNodeExpand(const ASTNode *node);
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

    // Lvalue-store cores shared by single-target ASSIGN and multi-output
    // MULTI_ASSIGN. When `valReg >= 0` the value to store is taken from
    // that register (multi-assign output); otherwise `rhsNode` is
    // compiled in-place exactly as the single-assign path always did.
    uint8_t compileFieldStore(const ASTNode *lhs, const ASTNode *rhsNode,
                              int inValReg, bool suppress);
    uint8_t compileCellStore(const ASTNode *lhs, const ASTNode *rhsNode,
                             int inValReg, bool suppress);
    uint8_t compileIndexStore(const ASTNode *lhs, const ASTNode *rhsNode,
                              int inValReg, bool suppress);
    // Dispatch an output register into any lvalue target (multi-assign).
    void compileStoreLValue(const ASTNode *target, uint8_t valReg, bool suppress);

    // True when an lvalue mixes accessor kinds or is rooted at a non-
    // identifier object beyond what the single-level fast compilers
    // handle (`s.x(2)`, `d(i).a.b`, `c{i}(2)`, `c{i}.f`, …). Such lvalues
    // are compiled by compileLValueStore via a get/set write-back chain.
    bool needsGeneralLValuePath(const ASTNode *lhs) const;
    // General compound-lvalue store: flatten the accessor chain, load
    // intermediate containers (get-or-create), apply the final write, and
    // write each modified container back up to the root variable. Sources
    // the value from `inValReg` when ≥ 0, else compiles `rhsNode`.
    uint8_t compileLValueStore(const ASTNode *lhs, const ASTNode *rhsNode,
                               int inValReg, bool suppress);
    // Superclass call inside a classdef body: `callNode` is the enclosing
    // CALL, `superRef` its SUPERCLASS_REF head. Emits CALL_SUPER_CTOR
    // (`obj@Base(args)`, lhs a local) or CALL_SUPER_METHOD
    // (`method@Base(obj,…)`). explicitOutBase ≥ 0 routes outputs to that
    // register block (multi-assign); otherwise a temp is allocated. Returns
    // the first output register.
    uint8_t compileSuperCall(const ASTNode *callNode, const ASTNode *superRef,
                             int explicitOutBase, uint8_t nout);
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
    // Element-wise fusion: if `node` matches a registered idiom (and we're not
    // already compiling a fallback, and there's register headroom), emit
    // FUSE_EWISE + fallback and return the result register; else return -1 so
    // the caller proceeds with the normal compile. Shared by every node type
    // that can root a fused idiom (CALL for max/min clamp, BINARY_OP for affine
    // & friends), so adding a binary-rooted idiom needs no new core hook.
    int tryCompileFused(const ASTNode *node);
    // Emit FUSE_EWISE for the matched idiom + the normally compiled idiom as the
    // runtime fallback (patched dst + skip). `ruleIdx` indexes
    // engine.fusionRules(); `operands` are the rule's operand sub-exprs.
    uint8_t compileFused(const ASTNode *node, size_t ruleIdx,
                         const std::vector<const ASTNode *> &operands);
    uint8_t compileCommandCall(const ASTNode *node);
    // Record per-CALL-site arg names into chunk_.callSiteArgNames so
    // inputname(k) inside the callee can introspect the call site.
    void recordCallArgNames(const ASTNode *callNode, size_t callInstrIdx);
    uint8_t compileFunctionDef(const ASTNode *node);
    uint8_t compileReturn(const ASTNode *node);
    // Emit a RET_VARARGOUT for the current chunk (last return is `varargout`):
    // gather the fixed leading outputs, then the dynamic varargout cell.
    void emitVarargoutReturn();
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