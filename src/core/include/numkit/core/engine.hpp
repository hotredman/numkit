// include/engine.hpp
#pragma once

#include <numkit/core/debugger.hpp>
#include <numkit/figure/figure_manager.hpp>
#include <numkit/value/object.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/fusion_rule.hpp>
#include <numkit/fs/vfs.hpp>
#include <numkit/fs/fs_context.hpp>
#include <numkit/ops/rng_context.hpp>
#include <numkit/core/vm.hpp>

#include <atomic>
#include <memory>
#include <memory_resource>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace numkit {

class TreeWalker;
class VM;
class Compiler;
class DebugFacade;       // grouped debugger handle (engine.debug()); defined below
struct ClassDefDesc;     // user-classdef descriptor; full type in engine.cpp
struct CallbackBuiltin;  // higher-order builtin driver; callback_builtin.hpp

class Engine
{
public:
    // Default ctor: uses std::pmr::get_default_resource() for all allocations.
    Engine();
    // Custom heap: caller-supplied memory_resource. Must outlive the Engine
    // and every Value/DataBuffer it produces. Pass a subclass of
    // std::pmr::memory_resource (or one of std::pmr's built-in resources).
    explicit Engine(std::pmr::memory_resource *mr);
    ~Engine();

    Engine(const Engine &) = delete;
    Engine &operator=(const Engine &) = delete;
    Engine(Engine &&) = delete;
    Engine &operator=(Engine &&) = delete;

    // The memory_resource used for persistent (output-Value) allocations.
    // Internal helpers and ScratchArena upstream-spill flow through here.
    std::pmr::memory_resource *resource() const noexcept { return mr_; }

    void registerBinaryOp(const std::string &op, BinaryOpFunc func);
    void registerUnaryOp(const std::string &op, UnaryOpFunc func);

    // Register a builtin function. Two forms:
    //   1-arg: registers in core (flat namespace, no prefix). The full
    //          name in the registry equals `name`.
    //   3-arg: registers under a namespace. `ns` is dot-separated
    //          (e.g. "signal.transforms"). The full name in the
    //          registry equals "ns.name". `ns=""` is equivalent to the
    //          1-arg form.
    // Both forms throw std::runtime_error on duplicate full names.
    // See namespace_design.md for the rules.
    void registerFunction(const std::string &name, ExternalFunc func);
    void registerFunction(const std::string &ns,
                          const std::string &name,
                          ExternalFunc func);

    // --- Implicit Imports ---
    // Persistent imports (like `import compat.*`) that survive `clear all`.
    void addImplicitImport(const Import &imp);
    void clearImplicitImports();

    // ── Class registry (object model — see object_model.md) ──────
    // Register a builtin (C++-backed) class. Later, user classdef
    // populates the same registry via an adapter. Throws on duplicate.
    void registerClass(BuiltinClass cls);
    // Look up a registered class by name (e.g. "containers.Map"), or
    // nullptr. Used by constructor / method / property dispatch.
    const BuiltinClass *findClass(const std::string &name) const;

    // ── State-machine callbacks (callback_builtin.hpp, vm_callbacks_plan.md) ──
    // Register a higher-order builtin (cellfun/arrayfun/…) that can drive its
    // user-code callbacks as pausable VM frames. Registered alongside the
    // ordinary synchronous external registration; the VM consults this first and
    // falls back to the synchronous builtin when the driver declines (builtin
    // handle / unsupported arg form). Returns the registered driver or nullptr.
    void registerCallbackBuiltin(const std::string &name, std::shared_ptr<CallbackBuiltin> cb);
    CallbackBuiltin *callbackBuiltin(const std::string &name) const;
    // True if `handle` (plain handle or {handle, captures…} closure cell) names
    // a USER function — i.e. there is a compiled body to step through — rather
    // than a builtin.
    bool isUserCodeHandle(const Value &handle) const;
    // Register every top-level `function` in `src` as a PERSISTENT user
    // function (userFuncs_ + VM compiled table) — the same path m-file loading
    // uses, so they survive `clear` and work on both backends. Used to install
    // `.m`-implemented builtins (e.g. fzero) whose callbacks must be pausable:
    // the `.m` body's f-calls compile to ordinary VM frames, debuggable for
    // free, no fiber / state machine. See vm_callbacks_plan.md.
    void registerBuiltinMSource(const std::string &src);

    // Register a user `classdef` (parsed CLASSDEF_DEF node) as a BuiltinClass
    // via the adapter: generic property get/set over ObjectState.props,
    // default-init + user constructor on construct, and method hooks that
    // run the method bodies. Re-running a `classdef` for an already-registered
    // class REPLACES it wholesale (see unregisterClassDef) so REPL / IDE edits
    // take effect. `qualifiedName` (e.g. "geo.Vec") names a class loaded from a
    // `+pkg/Name.m` file: the source node carries only the leaf (`Vec`), so the
    // package qualification — which lives in the path — is threaded in here and
    // becomes the registry identity (class()/isa()/registry keys all use it).
    // Empty (the default) → use the node's own name (inline / unpackaged).
    void registerClassDef(const ASTNode *classdef, const std::string &qualifiedName = "");
    // `clear classes` / `clear all`: remove every USER classdef so the next
    // reference re-loads (file classes) or errors as undefined (inline ones).
    // Built-in classes (containers.Map, …) are not in classDefs_ and survive.
    void clearClassDefs();
    // Run a classdef method body (args already include `self` first) /
    // constructor body (with `obj` seeded to the default instance) on the
    // TreeWalker, regardless of the active backend.
    std::vector<Value> invokeClassMethod(const UserFunction &uf, Span<const Value> args,
                                         size_t nout);
    Value invokeClassCtor(const UserFunction &ctor, const Value &seed,
                          Span<const Value> args);
    // Compile a classdef method/constructor body into the VM's global compiled
    // table (idempotent), returning its chunk — so the VM can run the body as
    // a native frame. See vm_callbacks_plan.md.
    const BytecodeChunk *ensureClassMethodChunk(const UserFunction &uf);
    // Enforce a classdef method's declared access from the current context
    // (no-op for a public method). Used by the VM frame-dispatch path, which
    // bypasses the C++ method hook that would otherwise run the check.
    void enforceMethodAccess(const std::string &className, const std::string &method);
    // A classdef property's `get.Prop` / `set.Prop` accessor UserFunction, or
    // nullptr when the property has none (plain stored property) / the class is
    // not a classdef. Used by the VM FIELD_GET/FIELD_SET opcode handlers to run
    // an accessor body as a same-stack VM frame (pausable) instead of the C++
    // propGet/propSet hook. enforceProp{Get,Set}Access enforces the property's
    // GetAccess/SetAccess from the current context before the accessor runs
    // (the access half that the propGet/propSet hook would otherwise perform).
    const UserFunction *classGetter(const std::string &className,
                                    const std::string &prop) const;
    const UserFunction *classSetter(const std::string &className,
                                    const std::string &prop) const;
    void enforcePropGetAccess(const std::string &className, const std::string &prop);
    void enforcePropSetAccess(const std::string &className, const std::string &prop);
    // Resolve a classdef operator overload (binary / unary) to its compiled
    // method chunk for an in-bytecode VM frame-push. Returns nullptr — caller
    // uses the numeric fast path or the C++ slow path (`tryObject*Op`) — when no
    // operand is an object, the class has no such overload, the overload is a
    // synthetic non-UserFunction (enum eq/ne), or the body can't VM-compile. On
    // success sets ownerClassOut to the dominant object's class and enforces the
    // operator method's access. The method's parameters ARE the operands:
    // binary args = [lhs, rhs], unary args = [operand]. P4, vm_callbacks_plan.md.
    const BytecodeChunk *resolveBinaryOpChunk(const std::string &op, const Value &lhs,
                                              const Value &rhs, std::string &ownerClassOut);
    const BytecodeChunk *resolveUnaryOpChunk(const std::string &op, const Value &operand,
                                             std::string &ownerClassOut);
    // Resolve a classdef `subsref`/`subsasgn` overload to its compiled method
    // chunk for an in-bytecode VM frame-push, marshaling the call args (subsref:
    // [self, substruct]; subsasgn: [self, substruct, value]). `idx` is the flat
    // subscript list; for subsasgn `idxAndVal` is [subscripts…, value] (value
    // last). Returns nullptr — caller uses the C++ slow path / builtin
    // indexing — when the class has no such overload or the body can't compile.
    // On success sets ownerClassOut to the object's class. P4, vm_callbacks_plan.md.
    const BytecodeChunk *resolveSubsrefChunk(const Value &self, Span<const Value> idx,
                                             std::string &ownerClassOut,
                                             std::vector<Value> &argsOut);
    const BytecodeChunk *resolveSubsasgnChunk(const Value &self, Span<const Value> idxAndVal,
                                              std::string &ownerClassOut,
                                              std::vector<Value> &argsOut);
    // Superclass-qualified calls from inside a classdef body
    // (`obj@Base(args)` / `method@Base(obj, args)`). superConstruct runs
    // Base's constructor with `seed` as the partially-built object and
    // returns the base-initialised object; superMethod runs Base's named
    // method (args already include `self` first). Both throw if Base is not
    // a registered classdef. When Base defines no constructor, superConstruct
    // returns `seed` unchanged (a base with only default property init).
    Value superConstruct(const std::string &base, const Value &seed,
                         Span<const Value> args);
    std::vector<Value> superMethod(const std::string &base, const std::string &method,
                                   Span<const Value> args, size_t nout);
    // classdef member-access context (private/protected/immutable). Push the
    // running method's/constructor's class on entry, pop on exit (RAII at the
    // call sites). `classCtxAllows` answers "may code in the current context
    // touch a member declared in `declClass`?": private (`privateOnly=true`)
    // needs an exact class match; protected also admits subclasses of
    // `declClass`. `classCtxInCtorOf` is true when the top frame is a
    // constructor of `declClass` (used to gate `SetAccess = immutable`).
    void pushClassCtx(std::string className, bool isCtor);
    void popClassCtx();
    bool classCtxAllows(const std::string &declClass, bool privateOnly) const;
    bool classCtxInCtorOf(const std::string &declClass) const;
    // Construct an object enforcing a non-public constructor's access against
    // the current context (private ctor → only from the declaring class;
    // protected → also subclasses). Used at the user-facing `ClassName(args)`
    // call sites; internal default-fill calls `cls->construct` directly so
    // object-array growth is never blocked. A public ctor (or a non-classdef
    // BuiltinClass) passes straight through.
    Value constructChecked(const BuiltinClass *cls, Span<const Value> args, CallContext &ctx);
    // Build a default-initialised instance of a classdef (abstract-class check
    // + every property at its declared default), WITHOUT running the user
    // constructor. This is the "seed" the constructor body receives bound to
    // its output variable. Throws if `className` is not a classdef.
    Value makeDefaultInstance(const std::string &className);
    // The classdef's user-defined constructor (the method named like the
    // class), or nullptr when the class declares none / is not a classdef.
    const UserFunction *classCtor(const std::string &className) const;
    // Enforce a non-public constructor's access against the current context
    // (the access half of constructChecked, without building the object). Used
    // by the VM ctor-frame path before pushing the constructor frame. No-op for
    // a public ctor or a non-classdef BuiltinClass.
    void enforceCtorAccess(const std::string &className);
    // MATLAB-style display text for an OBJECT value. `name` empty →
    // bare body (disp); otherwise the `name =\n\n<body>\n` form.
    std::string formatObjectDisplay(const std::string &name, const Value &obj) const;
    // Emit an OBJECT value to the engine output, honouring a class-defined
    // `display` (owns the whole output) or `disp` (owns the body; the default
    // `name =` header is added when `name` is non-empty) method on a scalar
    // object. Falls back to formatObjectDisplay otherwise. Shared by both
    // backends' display paths.
    void displayObject(const std::string &name, const Value &obj);
    // Operator overloading: when `lhs` or `rhs` is an OBJECT, dispatch the
    // binary operator `op` (the source token, e.g. "+", ".*", "==") to the
    // dominant object's class `ops` entry (MATLAB names: plus/times/eq/…).
    // The hook receives self = the dominant object and args = {lhs, rhs}.
    // Returns true (out set) when handled; false when neither side is an
    // object. Throws the MATLAB "Undefined operator … for type …" error
    // when an object operand has no matching overload. Shared by both
    // engines' binary-op slow paths.
    bool tryObjectBinaryOp(const std::string &op, const Value &lhs, const Value &rhs,
                           Environment *env, Value &out);
    // Unary counterpart: dispatch `op` ("-", "~", "'", ".'") to the
    // operand object's class `ops` (uminus/not/ctranspose/transpose). The
    // hook receives self = the operand and no args. Returns true (out set)
    // when handled; false when the operand is not an object. Throws the
    // MATLAB "Undefined operator" error when an object has no overload.
    bool tryObjectUnaryOp(const std::string &op, const Value &operand,
                          Environment *env, Value &out);
    // Value-level operator application with object-overload dispatch — the
    // counterpart to the AST (TreeWalker) and bytecode (VM) operator paths for
    // callers that hold Values but no AST node / bytecode operands: the codegen
    // Value-ABI bridge (nk_codegen_rt's Dynamic tier) and feval of an operator
    // handle. Mirrors the VM slow path exactly: tryObject*Op first (overloads),
    // then the registered numeric op; throws "Undefined … operator" when
    // neither applies. `op` is the source token ("+", ".*", "==", "-", "~",
    // "'", …). Short-circuit "&&"/"||" are NOT handled here (they are control
    // flow, lowered by the caller) — pass an eager logical op instead.
    Value applyBinaryOp(const std::string &op, const Value &lhs, const Value &rhs);
    Value applyUnaryOp(const std::string &op, const Value &operand);
    // Builtin object-array slice store: dst(subscripts) = val, default-
    // filling grown slots via the class's no-arg constructor. `perDim` holds
    // the resolved 0-based index list per subscript — one list → linear
    // store (shape-preserving / vector grow), several → N-D store (grid
    // grow). `val` is a scalar object (broadcast) or matches the target
    // count. Callers first verify dst is a valid target (empty/unset, or a
    // same-class object array). Centralises the store boilerplate shared by
    // TreeWalker and every VM INDEX_SET* opcode.
    void objectStoreSlice(Value &dst, const std::vector<std::vector<size_t>> &perDim,
                          const Value &val, Environment *env);
    // Read counterpart: if `self`'s class defines a custom subsref, dispatch
    // it with `args` and return true (result in `out`); otherwise return
    // false so the caller runs the builtin object-array indexing path.
    // Centralises the subsref check + call shared by the VM index opcodes.
    bool tryObjectSubsref(Value &self, Span<const Value> args, std::size_t nargout,
                          Value &out, Environment *env);

    // ── Namespace introspection (used by resolver — Phase 6) ──────

    // Top-level namespaces in registration order. `compat` shows up
    // here once the first MATLAB-mirror library calls
    // registerFunction("compat", …).
    const std::vector<std::string> &namespaces() const { return namespaceOrder_; }

    // Multimap from leaf name (last segment) to full name
    // (incl. all promotions, aliases). Used by the resolver to
    // implement `import x.*` wildcard lookups in O(1).
    const std::unordered_multimap<std::string, std::string> &shortNameIndex() const
    {
        return shortNameIndex_;
    }

    // Runtime resolver: given a name (possibly short), looks it up considering
    // the active imports in `env` (and its parent chain). Returns nullptr
    // if not resolvable. `env` may be nullptr — then only direct (core or
    // fully-qualified) lookup is performed.
    //
    // Resolution order (per namespace_design.md Section 3):
    //   1. Direct: externalFuncs_.find(name)  (core / already qualified)
    //   2. Walk env→parent chain, for each scope's active imports:
    //      - wildcard `import a.b.*`     → try "a.b.<name>"
    //      - explicit `import a.b.c`     → if c == name, try "a.b.c"
    //      - alias    `import a.b as x`  → only resolves x.<name> (handled
    //                                       in dotted-name lookup, not here)
    const ExternalFunc *findExternal(const std::string &name,
                                      const Environment *env) const;

    // Returns the full qualified name for a bare leaf name (e.g. "fft" →
    // "signal.transforms.fft") — the namespace source that the bare-name
    // resolver found. Empty when the name doesn't resolve. Used by `which`.
    std::string bareNameSource(const std::string &name) const;

    // Compile-time check: is `name` a leaf name registered anywhere in the
    // namespace tree? Used by the compiler to decide whether an identifier
    // is a callable. Does NOT consider imports — that's a runtime concept.
    bool isKnownLeafName(const std::string &name) const
    {
        return externalFuncs_.count(name) > 0 || shortNameIndex_.count(name) > 0;
    }

    void setVariable(const std::string &name, Value val);
    Value *getVariable(const std::string &name);

    // Execute code — uses current backend (TreeWalker by default).
    // When `suppressTopLevelDisplay==true`, every top-level statement
    // in `code` is silenced (ans / lhs displays skipped). Side-effect
    // prints inside called functions (disp, fprintf, ...) still fire.
    // Used by the `eval` builtin when the outer caller captures the
    // return value — matches MATLAB:
    //   r = eval('a + b');   % no inner ans display
    //   eval('a + b');       % inner ans display proceeds normally
    Value eval(const std::string &code, bool suppressTopLevelDisplay = false);

    // Scoped variant: top-level imports and variable assignments inside
    // `code` are routed to `scope` instead of workspaceEnv. Used by
    // `eval`/`run` builtins called from inside a function, and by
    // `evalin`. scope=nullptr or scope==&workspaceEnv() falls back to
    // the no-scope overload (REPL-style persistence).
    Value eval(const std::string &code, Environment *scope,
               bool suppressTopLevelDisplay = false);

    // Safe execution — never throws, returns result + error diagnostics
    struct EvalResult {
        Value value;
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

    // Element-wise fusion (see fusion_rule.hpp). The standard library
    // registers concrete rules; both backends consult them when enabled.
    // Disabled instantly (no rebuild) via setFusion(false) / NUMKIT_FUSE=0.
    void addFusionRule(FusionRule rule) { fusionRules_.push_back(std::move(rule)); }
    const std::vector<FusionRule> &fusionRules() const { return fusionRules_; }
    void setFusion(bool on) { fusionEnabled_ = on; }
    bool fusionEnabled() const { return fusionEnabled_ && !fusionRules_.empty(); }
    // Fusion-fire telemetry: both backends call noteFusionHit() each time a rule's
    // execute() returns true. Lets tests PROVE a kernel actually fires (a parity
    // test alone can't — a silent decline gives the same fused==unfused result).
    void noteFusionHit() { ++fusionHits_; }
    size_t fusionHits() const { return fusionHits_; }
    void resetFusionHits() { fusionHits_ = 0; }

    using OutputFunc = std::function<void(const std::string &)>;
    void setOutputFunc(OutputFunc f);
    OutputFunc outputFunc() const { return outputFunc_; }
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

    // ── Public callback API for builtins ──────────────────────
    // Invoke a function-handle Value from C++. Routes through TW so
    // both built-in (externalFuncs_) and user/anonymous handles work
    // regardless of the active backend. `env` defaults to
    // workspaceEnv() when nullptr; pass the CallContext env if you
    // want the callee to see the caller's local scope.
    //
    // Single-output and multi-output forms; the multi-output form
    // returns whatever the handle produces (truncated/extended to
    // `nout` slots).
    Value callFunctionHandle(const Value &handle,
                              Span<const Value> args,
                              Environment *env = nullptr);
    std::vector<Value> callFunctionHandleMulti(const Value &handle,
                                                Span<const Value> args,
                                                size_t nout,
                                                Environment *env = nullptr);

    // Unified lookup — script-scope first, then workspace-scope, then
    // an m-file resolver pass over the configured search path
    // (script-origin → addPath_-list). Returns nullptr when the name
    // isn't found anywhere. May parse-and-cache an m-file as a
    // side-effect.
    //
    // `env` drives the import-walk fallback for unqualified names. Pass
    // the active scope (TW: caller's env; VM: workspaceEnv) so that
    // `import pkg.*` in scope can resolve bare `foo()` to `pkg.foo`.
    // Pass nullptr only for introspection paths that intentionally
    // bypass imports (e.g. `which`, `exist`).
    const UserFunction *lookupUserFunction(const std::string &name,
                                            const Environment *env);

    // Const variant — never triggers the m-file resolver. Use for
    // pure introspection / "is this CURRENTLY known" checks.
    const UserFunction *lookupUserFunctionLocal(const std::string &name) const;

    // Clear workspace-scope user functions. Script-local functions
    // live in a separate bucket (see beginScript/endScript) and are
    // untouched — MATLAB treats file-scoped functions as lexically
    // part of the script, not the workspace.
    void clearUserFunctions();

    // Adopt a UserFunction parsed/compiled by another component (the
    // compiler emitting FUNCTION_DEF chunks, the m-file resolver, the
    // anonymous-function machinery). When `scriptScope` is true the
    // entry lands in the script-local bucket cleared by endScript;
    // otherwise it lives in the workspace bucket. Single funnel for
    // the writes that previously went straight into the private maps
    // via friendship.
    void adoptUserFunction(const std::string &name,
                            UserFunction uf,
                            bool scriptScope = false);

    // Base-workspace global membership is the single set workspaceEnv_->globals_
    // (see Environment::globalNames). The compiler reads it to seed each new
    // top-level chunk's globalNames list — split-mode execution otherwise loses
    // the declaration between statements.

    // ── M-file path registry (Phase 9) ─────────────────────────────
    // Directories searched (in order) by lookupUserFunction's m-file
    // resolver pass. Paths are VFS-resolvable strings (e.g.
    // "native:/scripts", "local:/work", or just a relative path).
    void addPath(const std::string &dir);
    void rmPath(const std::string &dir);
    const std::vector<std::string> &path() const { return mPath_; }

    // Drop all cached m-file entries — next lookup re-scans the disk.
    // Powers MATLAB's `rehash` builtin.
    void rehashMFiles();

    // Check all cached m-files against disk/VFS and evict any that have been
    // modified or deleted, so subsequent calls re-parse the latest source.
    // Evicted file CLASS bases with live dependents are reloaded through the
    // same cascade as rehashMFiles (an inline subclass must re-merge the
    // edited base, not keep the stale snapshot).
    void refreshStaleMFiles();

    // Reload each evicted file-class base that a surviving (inline) subclass
    // still derives from; registerClassDef's cascade re-merges the dependents.
    void reloadEvictedClassBases_(const std::vector<std::string> &bases);

    // Mark the entry/exit of a top-level script or function
    // evaluation. While a script is active, any FUNCTION_DEF it
    // defines compiles into the script-scope buckets instead of the
    // workspace. Nesting is supported via internal save stacks, so
    // recursive eval() calls don't lose their outer scope.
    //
    // The AST pointer must outlive the matching endScript() — the
    // caller (eval, DebugSession) owns lifetime.
    void beginScript(const ASTNode *ast);
    void endScript();

    // Copy the current script-scope user/compiled function buckets
    // into the workspace-scope ones. Used by eval() at script exit
    // so that REPL-ish multi-statement pastes (`function f()...end;
    // f();` on one go) leave `f` callable from a later eval —
    // preserving the engine's REPL persistence contract. Debug
    // sessions don't call this, so file-scoped helpers in a .m
    // script don't leak into base workspace.
    void promoteScriptLocalsToWorkspace();

    // --- Debugger API — grouped behind engine.debug() (see DebugFacade) ──
    // The debugger accessors live off the public Engine surface (private,
    // reached through the DebugFacade handle) to bound the class's public
    // method count — architecture-review risk #3.
    DebugFacade debug();

private:
    friend class DebugFacade;
    void setDebugObserver(std::shared_ptr<DebugObserver> observer);
    DebugObserver *debugObserver() const { return debugObserver_.get(); }
    BreakpointManager &breakpointManager() { return breakpointManager_; }
    const BreakpointManager &breakpointManager() const { return breakpointManager_; }
    DebugController *debugController() { return debugController_.get(); }
    const DebugController *debugController() const { return debugController_.get(); }

    // Resume paused debug execution with the given action.
    // Returns Paused on next breakpoint/step, Completed if execution finishes.
    // Throws DebugStopException if Stop is requested, Error on runtime errors.
    ExecStatus debugResume(DebugAction action);

public:

    // Reinstall built-in constants (pi, eps, inf, etc.) into constantsEnv.
    // Called after clear to restore the standard environment. Also
    // re-installs any constants the host has registered via
    // registerConstant() so `clear all` doesn't wipe them.
    void reinstallConstants();

    // Register a host-level constant — visible to every script as if it
    // were `pi`/`eps`/etc. Does not appear in `whos` or the debug
    // Workspace panel, can be shadowed by `name = …` and un-shadowed by
    // `clear name`, and survives `clear all`.
    void registerConstant(const std::string &name, Value val);

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

    // ── Frame stack introspection (for assignin / evalin / inputname) ──
    //
    // Walk the user-function call stack from a builtin currently being
    // executed. n=0 = the immediate caller's scope (the function or
    // top-level script that contains the call site). n=1 = its caller.
    // Walking off the top of the stack returns &workspaceEnv().
    //
    // Backend-aware: VM walks frames_, lazy-allocating frame.env on the
    // way; TW walks its own activeFrames_ stack maintained via RAII
    // guards around callUserFunction.
    Environment *callerEnv(int n = 0);

    // Number of user-function frames between the running builtin and
    // the top-level. 0 means the builtin was called directly from a
    // top-level script (caller is workspaceEnv).
    int callerDepth() const;

    // Variable assignment in the n-th caller's scope. Writes the value
    // such that the caller can subsequently read it back. In VM mode,
    // also performs write-through to the caller's frame register if the
    // name is statically allocated (chunk.varMap hit). Used by assignin.
    void assignToCaller(int n, const std::string &name, Value val);

    // Returns the source-text name of the k-th input arg as written at
    // the call site of the currently-running user function (1-indexed,
    // matching MATLAB). Empty string if the arg was not a bare
    // identifier (literal, expression, etc.). Throws when invoked from
    // outside any user-function call, or when k is out of range.
    // Backed by per-CALL-site metadata recorded by the compiler / TW
    // call sites.
    std::string inputName(int k);

    // ── Virtual filesystem ────────────────────────────────────
    //
    // Registry of named filesystems ("native", "temporary", "local", …).
    // A native FS is pre-registered in the Engine constructor; the IDE
    // installs "temporary" / "local" via CallbackFS hooks at startup.
    //
    // resolvePath() picks the right backend by this order of precedence:
    //   1. explicit prefix in the path ("temporary:/foo", "local:/foo",
    //      "native:/foo") — wins over everything
    //   2. env var NUMKIT_FS, if it names a registered backend
    //   3. the current script's origin (set by the IDE before eval)
    //   4. "native" if registered
    // Relative paths are joined with `cwd_` when set; otherwise with
    // NUMKIT_CWD as a host-runtime fallback. cwd_ takes precedence —
    // once `cd` / setCwd writes to it, the env var is ignored.
    void registerVirtualFS(std::unique_ptr<VirtualFS> fs);
    VirtualFS *findVirtualFS(const std::string &name) const;

    // Each frame carries (fsName, scriptDir):
    //   * fsName     — VFS that the script came from. Used by resolvePath
    //                  as the implicit FS for relative paths inside the
    //                  script.
    //   * scriptDir  — directory containing the script. Used by
    //                  resolveMFile_ as the implicit search dir, so
    //                  sibling .m files resolve without addpath.
    // 1-arg overload pushes an empty scriptDir — kept for callers that
    // only know the FS (IDE running an unsaved buffer, tests).
    void pushScriptOrigin(const std::string &fsName);
    void pushScriptOrigin(const std::string &fsName, const std::string &scriptDir);
    void popScriptOrigin();
    const std::string *currentScriptOrigin() const;   // fsName, may be null
    const std::string *currentScriptDir() const;      // dir, may be null/empty

    // Engine-owned current working directory.
    //   * `cd` / setCwd write here — canonical when non-empty.
    //   * pwd reports this (with backend-cwd fallback when empty).
    //   * resolvePath consults this first, then NUMKIT_CWD env var.
    // The two-tier model lets hosts seed cwd via `setenv NUMKIT_CWD`
    // without having to call setCwd, while still letting in-engine
    // `cd` calls take precedence once they happen.
    const std::string &cwd() const { return fsCtx_.cwd(); }
    void setCwd(const std::string &p) { fsCtx_.setCwd(p); }

    // The path-resolution result type lives on FsContext (fs/, L0); aliased
    // here so existing callers keep writing Engine::ResolvedPath.
    using ResolvedPath = FsContext::ResolvedPath;
    ResolvedPath resolvePath(const std::string &userPath) const;

    // Direct handle to the filesystem session (VFS registry + script-origin
    // stack + cwd + resolver). Lets core-free toolbox code (io codecs, …)
    // resolve paths through fs::FsContext WITHOUT depending on Engine — the
    // Engine-free C++ API surface. Registration adapters pass engine.fsContext().
    FsContext &fsContext() noexcept { return fsCtx_; }
    const FsContext &fsContext() const noexcept { return fsCtx_; }

    // The session RNG stream (rand/randn/randi/randperm + rng() control + every
    // toolbox *rnd sampler draw from this). Registration adapters pass
    // engine.rng() to the ops generators / RngContext&-taking samplers.
    ops::RngContext &rng() noexcept { return rng_; }
    const ops::RngContext &rng() const noexcept { return rng_; }

    // ── MATLAB-style file descriptor table ────────────────────
    //
    // The fopen / fclose / fprintf(fid, …) state + machinery now live on
    // FsContext (fs/, L0) so the stateful fopen-family is Engine-free; the
    // methods below forward to fsCtx_. `OpenFile` is aliased here so existing
    // callers keep writing `Engine::OpenFile`. fids 0/1/2 are still routed to
    // outputText() by the fprintf builtin, not by this table.
    using OpenFile = FsContext::OpenFile;
    int openFile(const std::string &userPath, const std::string &mode)
    { return fsCtx_.openFile(userPath, mode); }
    bool closeFile(int fid) { return fsCtx_.closeFile(fid); }
    void closeAllFiles() { fsCtx_.closeAllFiles(); }
    OpenFile *findFile(int fid) { return fsCtx_.findFile(fid); }
    std::vector<int> openFileIds() const { return fsCtx_.openFileIds(); }
    const std::string &lastFopenError() const { return fsCtx_.lastFopenError(); }

private:
    std::pmr::memory_resource *mr_;  // not owned; caller-supplied or get_default_resource()
    std::unique_ptr<Environment> globalsEnv_;     // MATLAB 'global' variables — shared across functions
    std::unique_ptr<Environment> constantsEnv_;  // pi, eps, inf, etc. — parent for all scopes
    std::unique_ptr<Environment> workspaceEnv_;  // top-level workspace (base workspace)

    std::unordered_map<std::string, BinaryOpFunc> binaryOps_;
    std::unordered_map<std::string, UnaryOpFunc> unaryOps_;
    std::unordered_map<std::string, ExternalFunc> externalFuncs_;

    // Object-model class registry (object_model.md). Keyed by class name.
    std::unordered_map<std::string, BuiltinClass> classes_;
    // Parsed user classdef descriptors (property defaults, ctor + method
    // UserFunctions), kept for inheritance merges. Full type in engine.cpp.
    std::unordered_map<std::string, std::shared_ptr<ClassDefDesc>> classDefs_;
    // Cloned source AST per registered classdef, so a base-class redefinition
    // can re-merge its (transitive) subclasses from scratch. Keyed by the
    // class's registry name (qualified for packaged classes).
    std::unordered_map<std::string, std::shared_ptr<const ASTNode>> classDefAst_;
    // Re-entrancy guard for reregisterDerivedClasses: while a cascade is
    // re-registering subclasses, their own registerClassDef calls must not each
    // launch another (redundant, overlapping) cascade.
    bool suppressDependentCascade_ = false;
    // Higher-order builtins that can run callbacks as pausable VM frames
    // (state-machine callbacks). Keyed by builtin name; consulted by the VM
    // before the synchronous external path. Full type in callback_builtin.hpp.
    std::unordered_map<std::string, std::shared_ptr<CallbackBuiltin>> callbackBuiltins_;
    std::unordered_map<std::string, UserFunction> userFuncs_;

    // Class-execution context stack for classdef member-access enforcement
    // (private / protected / immutable). Each frame is the class whose
    // method or constructor body is currently running; push/pop happens in
    // invokeClassMethod / invokeClassCtor. An access check compares the
    // member's declaring class against the top frame. Empty == script scope
    // (only public members reachable).
    struct ClassCtxFrame
    {
        std::string className;
        bool isCtor;
    };
    std::vector<ClassCtxFrame> classCtx_;
    // Currently-executing class (TW callback guard, else VM top method frame;
    // empty == script scope). Backs classCtxAllows / classCtxInCtorOf.
    std::string currentClassCtx_() const;

    // Auxiliary indices into externalFuncs_, populated by registerFunction.
    // The full-name keys in externalFuncs_ are authoritative; these indices
    // exist purely to accelerate namespace-aware lookups (Phase 6).
    std::unordered_multimap<std::string, std::string> shortNameIndex_;
    std::vector<std::string> namespaceOrder_;
    std::unordered_set<std::string> namespaceSet_;

    // Bare-name resolver memoization cache (bare_name_resolver.md): maps
    // a bare leaf name to its resolved full qualified name (e.g. "fft" →
    // "signal.transforms.fft"). Populated on first resolution; cleared
    // when a new namespace is registered or a user function with the
    // same name is defined/deleted (the binding changes).
    mutable std::unordered_map<std::string, std::string> bareNameCache_;

    // Internal helper used by both registerFunction overloads.
    // Throws std::runtime_error if `fullName` already registered.
    void registerFunctionImpl_(const std::string &fullName,
                               const std::string &leafName,
                               ExternalFunc func);

    // Evict a previously-registered classdef so a redefinition fully replaces
    // it (REPL / IDE re-run of `classdef Name … end`). Removes the registry
    // class, the descriptor, the `Name.static` / `Name.CONST` qualified
    // externals, and the compiled method chunks. Called by registerClassDef
    // when `Name` is already registered, instead of the old idempotent skip.
    void unregisterClassDef(const std::string &name);
    // Remove the bare constructor external `externalFuncs_[name]` that
    // resolveMFile_ registers for a class loaded from `Name.m`. Guarded by
    // mFileCache_ membership so an inline `classdef sum` can never erase the
    // core `sum` builtin that merely shares the name.
    void dropFileClassCtorExternal_(const std::string &name);
    // After `base` is (re)registered, re-register every class that transitively
    // derives from it (parent-first), so subclasses — which hold a snapshot of
    // the base's methods/props — pick up the new definition. Guarded by
    // suppressDependentCascade_ against re-entrant fan-out.
    void reregisterDerivedClasses(const std::string &base);

    OutputFunc outputFunc_;
    FigureManager figureManager_;

    // Tic/toc timer
    TimePoint ticBase_{};
    bool ticCalled_ = false;

    // Tracks whether clear/clear all was called during VM execution
    // so that export wipes workspaceEnv before writing back.
    bool clearAllCalled_ = false;

    // Host-registered constants — (name → value). Restored into
    // constantsEnv_ alongside the built-ins by reinstallConstants() so
    // `clear all` does not drop them. Name presence also feeds
    // isReservedName() so these behave the same as `pi`/`eps`/…
    std::unordered_map<std::string, Value> userConstants_;

    // Script-lexical user functions — peer to userFuncs_ but never
    // cleared by `clear all`. Managed by begin/endScript (which
    // drives both this map and the compiler's matching bucket) so
    // file-scoped functions survive a mid-script clear.
    std::unordered_map<std::string, UserFunction> scriptLocalUserFuncs_;
    std::vector<std::unordered_map<std::string, UserFunction>> savedScriptLocalUserFuncs_;

    // ── M-file resolver state (Phase 9) ───────────────────────────
    std::vector<std::string> mPath_;
    struct MFileCacheEntry
    {
        std::string fullPath;       // VFS-prefixed, the resolved location
        int64_t mtime = 0;          // 0 if backend can't supply mtime
        std::string contentHash;    // fallback when mtime is unavailable
        std::shared_ptr<const std::string> sourceCode;  // keeps AST live
    };
    std::unordered_map<std::string, MFileCacheEntry> mFileCache_;
    // Helper — returns nullptr if `name`.m can't be located in any of
    // (script-origin, mPath_-list) or fails to parse. On success caches
    // the parsed UserFunction in userFuncs_ and the compiled chunk in
    // the Compiler.
    const UserFunction *resolveMFile_(const std::string &name);

    // Debugger
    std::shared_ptr<DebugObserver> debugObserver_;
    BreakpointManager breakpointManager_;
    std::unique_ptr<DebugController> debugController_;  // created when observer is set

    // Virtual filesystem session: VFS registry + script-origin stack + cwd +
    // path resolver. Engine forwards its filesystem API to this (see the
    // delegating wrappers in engine.cpp). Owned by value — FsContext is
    // STL-only (fs/, L0).
    FsContext fsCtx_;

    // The session's random-number stream + rng() control. Owned by value —
    // RngContext is an ops (L0.5) type. Default-constructed = rng('default').
    // Each Engine has an independent, reproducible stream; registration
    // adapters pass engine.rng() to the ops generators / toolbox samplers, so
    // there is no process-global RNG and no mutex.
    ops::RngContext rng_;

public:
    void markClearAll() { clearAllCalled_ = true; }
    void restoreImplicitImports(Environment *env) const {
        if (!env) return;
        for (const auto &imp : implicitImports_) {
            env->pushImport(imp);
        }
    }

private:
    std::unique_ptr<TreeWalker> treeWalker_;
    std::unique_ptr<Compiler> compiler_;

public:
    Compiler *compilerPtr() { return compiler_.get(); }

private:
    std::unique_ptr<VM> vm_;
    Backend backend_ = Backend::VM;

    // Element-wise fusion: registered idiom→kernel rules + on/off gate.
    // Default reflects NUMKIT_FUSE (set in the ctor); the registry is empty
    // until the standard library installs rules, so a bare Engine is unaffected.
    std::vector<FusionRule> fusionRules_;
    bool fusionEnabled_ = true;
    size_t fusionHits_ = 0;  // count of rule.execute()==true (fire telemetry)

    // Sync VM's exported variables to workspaceEnv (called after execute, even on error)
    void syncVMToWorkspace();

    // Sync VM's exported variables to an arbitrary scope. Falls back
    // to syncVMToWorkspace() when scope is null or equals workspaceEnv.
    // VM mode also performs write-through to the scope-owning frame's
    // register slots so subsequent register-based reads in the caller
    // pick up the values.
    void syncVMToScope(Environment *scope);

    // Compile one AST subtree as a VM chunk, run it, sync registers to
    // workspaceEnv. eval() calls this once per top-level statement when
    // splitting a multi-statement script for MATLAB-parity semantics.
    Value runOneChunk(const ASTNode *ast, std::shared_ptr<const std::string> src);

    std::vector<Import> implicitImports_;

    friend class TreeWalker;
    friend class VM;
    friend class DebugSession;
};

// Thin grouped handle over the Engine's debugger surface (architecture-review
// risk #3): keeps these accessors off the public Engine API. Obtain via
// engine.debug(); forwards to the (now-private) Engine debugger methods.
class DebugFacade
{
public:
    explicit DebugFacade(Engine &e) : e_(e) {}
    void setObserver(std::shared_ptr<DebugObserver> observer) { e_.setDebugObserver(std::move(observer)); }
    DebugObserver *observer() const { return e_.debugObserver(); }
    BreakpointManager &breakpoints() { return e_.breakpointManager(); }
    const BreakpointManager &breakpoints() const { return e_.breakpointManager(); }
    DebugController *controller() { return e_.debugController(); }
    const DebugController *controller() const { return e_.debugController(); }
    ExecStatus resume(DebugAction action) { return e_.debugResume(action); }

private:
    Engine &e_;
};

inline DebugFacade Engine::debug() { return DebugFacade(*this); }

} // namespace numkit