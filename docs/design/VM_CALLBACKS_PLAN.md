# VM-native callbacks — make VM a complete parallel engine

Status: **planned**, on `core-dev`. Owner: CORE.

> **Adding a pausable callback?** The decision rule (frame-push vs state machine
> vs `.m` wrapper), the mechanisms, and step-by-step recipes live in the guide
> [`docs/CALLBACK_PAUSABILITY.md`](docs/CALLBACK_PAUSABILITY.md). This file is the
> chronological build log + rationale.

## Problem

VM and TW must be two **independently complete** backends. Today they are
not: every path where **C++ calls back into user code** runs on the
TreeWalker, *regardless of the active backend*:

- `Engine::callFunctionHandleMulti` → `treeWalker_->callHandleMultiPublic`
  (function handles: `cellfun`, `arrayfun`, `feval`, `@f`) — `engine.cpp` ~1669.
- `Engine::invokeClassMethod` / `invokeClassCtor` → `treeWalker_->runClassMethod`
  / `runClassCtor` — classdef methods, constructors, `get.`/`set.` accessors,
  operator methods, `subsref`/`subsasgn`.

The debugger lives on the **VM** (`VM::startExecution`/`resumeExecution`,
pause/resume, breakpoints). So any user code reached through these C++ hooks
**cannot be debugged**, and OOP / higher-order code never gets VM speed under
the VM backend. This is the gap to close.

## Two call shapes (different mechanisms)

1. **In-bytecode calls** — `obj.m()`, `m(obj)`, `[a,b]=obj.m()` appear as
   `CALL` / `CALL_METHOD` / `CALL_METHOD_MULTI` opcodes inside a running chunk.
   Clean fix: resolve the method's compiled chunk and
   `pushCallFrame(...) + goto enter_frame` (a normal VM frame on `frames_`),
   instead of invoking the `BuiltinClass` C++ hook. No re-entrancy needed;
   fully debuggable (it is just another frame).

2. **C++-initiated calls** — a builtin/hook *called by the VM* needs to run
   user code mid-instruction: property accessors (`FIELD_GET`→`propGet`),
   operator methods (`tryObjectBinaryOp`), `subsref`/`subsasgn`
   (`INDEX_GET`/`INDEX_SET`), and function handles (`cellfun` et al.). These
   need a **re-entrant VM call from C++**: run a chunk to completion on a
   nested region of `frames_`, return the result, let the outer loop resume.

## Key mechanisms (validated in code)

- `Compiler::registerFunctionAs(name, funcDef)` compiles a `funcDef` AST into a
  `BytecodeChunk` and binds it (`compiler.cpp` ~3567). Re-entrant compile is
  already exercised by nested `function` defs.
  - **Caveat:** it uses `scriptDepth_` → registers `scriptLocal` when inside a
    script compile, which is wrong for classdef methods (they must persist with
    the class across evals). Need a **global** registration path (or promote).
- `VM::pushCallFrame(chunk, args*, nargs, destReg, nargout, isMulti, outBase, nout)`
  does a defensive copy of `args` (`vm.cpp` ~2379) — so a temporary
  `[self, args…]` buffer is fine for `obj.m()` where receiver and args are not
  contiguous. For `m(obj, …)` the args are already contiguous (`self == args[0]`).
- `enter_frame:` label (`vm.cpp` ~315) — the run loop re-loads frame/ip/R from
  `frames_.back()` each entry; need to confirm full re-entrancy for nested runs.
- `VM::execute(chunk, args, nargs)` — run-to-completion entry (legacy);
  `startExecution`/`resumeExecution` — debug-aware. Re-entrant nesting on
  `frames_` is the open design question for shape (2).

## Sub-problems to solve

1. **Compile scope** — classdef method/ctor chunks must be **global**
   (persist with the class), not script-local. Add a forced-global compile.
2. **Inherited-method chunk naming** — a method's chunk lives under its
   *declaring* class (`uf.name == "Decl>method"`). Dispatch has the *object's*
   class. Store, per class, `methodChunk[mname] = uf.name` (own + inherited,
   built post-merge) so dispatch can `findCompiledFunc` it.
2. **Discriminator** — classdef method (bytecode frame) vs native builtin-class
   method (`containers.Map`, C++ hook). `methodChunk` map present ⇒ classdef.
3. **Constructor seed** — the VM ctor frame must pre-bind the output var to the
   default instance (TW does this in `runClassCtor`).
4. **super-calls** — `SUPERCLASS_REF` is TW-only (the VM compiler rejects it);
   compile it to a `CALL` into `Base>member`.
5. **Re-entrancy** (shape 2) — a safe nested VM run for C++-initiated callbacks;
   `invokeClassMethod`/`invokeClassCtor`/`callFunctionHandleMulti` become
   backend-aware.
6. **Access context must become frame-associated.** Today the member-access
   stack (`Engine::classCtx_`) is pushed/popped by a synchronous C++
   `ClassCtxGuard` around the TW call (`invokeClassMethod`). A VM frame enters
   via `goto enter_frame` and unwinds later via `RET`, so a C++ RAII guard
   can't bracket it. The frame must carry its `ownerClass`: push `classCtx_`
   in `pushCallFrame` (for a classdef-method frame) and pop in the `RET`
   handler. Without this, private/protected access checks inside a VM-executed
   method body see an empty context and wrongly deny.
7. **Debugger** — stepping/breakpoints must work inside these frames.
8. **Coexistence** — TW backend unchanged; native builtin-class methods keep
   the C++ hook.

## Progress

- **P1a (done, `258b014f`)** — foundation: `ensureClassMethodCompiled` (global
  chunk), `BuiltinClass::methodFns`, `Engine::ensureClassMethodChunk`. Inert.
- **P1b (done, `09699913`)** — **function-form** `m(obj)` runs as a VM frame;
  frame-associated access context (`CallFrame::ownerClass`, read on demand);
  graceful fallback to the hook when a body can't VM-compile yet; access
  enforced via `Engine::enforceMethodAccess`. **Proven**: a breakpoint inside
  `go(obj)` pauses under the VM debugger (DebugSessionTest).
- **P1c (done, `3b8c360c`)** — dotted `obj.m()` + `[a,b]=obj.m()` run as VM
  frames ([self,args] buffer). Multi-output super-call guard added
  (compileMultiAssign rejects SUPERCLASS_REF → hook fallback). Proven:
  BreakInsideClassdefMethodDotted pauses under the VM. **Phase 1 complete** —
  every classdef method-call form (function-form, dotted, multi) is VM-native
  and debuggable; native builtin-class methods keep the hook; bodies with
  super-calls fall back to the TW hook until P2.
- **P2 (done)** — **constructor + super-calls VM-native** (in-bytecode).
  - Super-calls: `SUPERCLASS_REF` now compiles (`Compiler::compileSuperCall`) to
    two new opcodes — `CALL_SUPER_CTOR` (`obj = obj@Base(args)`, lhs is a var) and
    `CALL_SUPER_METHOD` (`method@Base(obj,…)`, lhs is a method name), routed from
    both `compileCall` (single-output) and `compileMultiAssign` (multi-output).
    The throw-guard + uncompilable-fallback for super-calls is gone, so a method
    body containing a super-call now VM-compiles and runs as a frame.
  - Constructor: the VM `CALL` handler, when it resolves a class name, enforces
    ctor access (`Engine::enforceCtorAccess`), then — if the class has a
    compilable user ctor — pushes a VM frame for the ctor body, seeding the
    output variable with the default instance (`Engine::makeDefaultInstance`,
    `Engine::classCtor`, `pushCallFrame(..., ctorSeed)`). No user ctor / not
    VM-compilable → the C++ path (default-fill or TW hook). Covers both
    `ClassName(args)` and the bare-name `x = ClassName` (both compile to `CALL`).
  - The super-call sub-target (the *base* ctor/method) still delegates to TW via
    `superConstruct`/`superMethod` (a C++-initiated call → P3); the *calling*
    body runs on the VM and is debuggable.
  - **Proven**: `DebugSessionTest.BreakInsideClassdefSuperMethod` (breakpoint on
    the line after a super-method call pauses) and
    `BreakInsideClassdefConstructor` (breakpoint inside a ctor body pauses; the
    seeded output var is modified and returned). Full suite 10923 green.
- **P3 (done)** — **re-entrant VM-call-from-C++ foundation** + first consumer.
  - `VM::callReentrant(chunk, args, nargout, ownerClass, isCtor, ctorSeed)`:
    runs a compiled chunk as a fresh top-level frame and harvests all return
    values, then restores the outer VM state. Built on the same save/restore
    machinery as `execute()` — which is what makes it safe: `savePausedState`
    parks the `frames_` buffer and `restorePausedState` move-assigns it back, so
    the outer dispatch loop's `frame`/register references (which are NOT reloaded
    after a re-entrant builtin) stay valid. Unlike `execute()` it does not reset
    the debug controller, so callee frames nest on the live debug stack;
    `lastVarMap_` is snapshot/restored so the nested top-level export doesn't
    clobber the outer's pending workspace export.
  - First consumer: `Engine::callFunctionHandleMulti` routes user-function
    handles (named + anon; captures arrive as appended args) through
    `callReentrant` when the active backend is the VM — so `cellfun`/`arrayfun`/
    `feval` callbacks run on the VM, not the TreeWalker. Handle bodies invoked
    through a handle *variable* (`h(x)` → `CALL_INDIRECT`) were already
    same-stack VM frames (pausable); P3 covers the C++-initiated builtin path.
  - **Limitation (documented contract):** a debugger *pause* cannot suspend
    across the C++ re-entry boundary — a breakpoint reached inside a
    C++-initiated callback fires and then surfaces as `DebugStopException`
    (same as legacy `execute()`), it cannot freeze-and-resume the outer session.
    The pausable callback paths are the in-bytecode ones (`CALL_INDIRECT`, and
    P4's `FIELD_GET`/operator/`INDEX_*` opcode frame-pushes).
  - **Proven**: `DebugSessionTest.BreakInsideFunctionHandleCall` (breakpoint
    inside a handle-invoked function pauses on the VM); 166 handle/cellfun/
    arrayfun/feval dual-engine tests green on the VM path; full suite 10924.
- **P4a (done)** — **`get.Prop` accessors VM-native (in-bytecode, pausable).**
  The VM `FIELD_GET` opcode, for an object whose class defines a `get.Prop`
  accessor, runs the accessor body as a SAME-STACK VM frame (pushCallFrame +
  enter_frame, like P1) rather than the C++ `propGet` hook → `invokeClassMethod`
  → TreeWalker. No save/restore (fast) and fully pausable. A getter returns
  exactly one value, so the frame's destReg is the `FIELD_GET` destination.
  Property `GetAccess` is enforced (`Engine::enforcePropGetAccess`) before the
  frame is pushed; plain stored properties keep the fast `propGet` path. New
  seams: `Engine::classGetter`/`classSetter`/`enforcePropGetAccess`/
  `enforcePropSetAccess`. **Proven**: `DebugSessionTest.BreakInsideClassdefGetter`
  (breakpoint inside `get.scaled` pauses; `y == 21`); full suite 10925.
- **P4b (done)** — **`set.Prop` setters VM-native.** The VM `FIELD_SET` opcode,
  for an object whose class defines `set.Prop`, runs the setter body on the VM,
  enforcing `SetAccess` first. A setter that returns the object
  (`function obj = set.Prop(obj,val)` — value class, or a handle setter with an
  output) runs as a SAME-STACK frame (pausable, fast) with the result written
  back into the object register (destReg = the object). A no-output handle
  setter (`function set.Prop(obj,val)`) mutates shared state in place, so it
  runs through `callReentrant` (no caller-register write — an empty RET would
  otherwise clobber the object register; the handle's mutation survives
  save/restore because both the snapshot and the setter's arg copy hold the same
  `shared_ptr<ObjectState>`). **Proven**: `DebugSessionTest.BreakInsideClassdefSetter`
  (breakpoint inside `set.val` pauses; `y == 10`); full suite 10926.
- **P4c (done)** — **backend-aware `invokeClassMethod`/`invokeClassCtor`** — the
  net for every remaining C++-initiated classdef callback. Under the VM backend
  both route the body through `VM::callReentrant` (carrying the class context on
  the frame's `ownerClass`, so no `ClassCtxGuard`), falling back to the
  TreeWalker only if the body can't VM-compile. This single change moves onto
  the VM: operator methods (`a+b`, `-a`, `a==b` → `binarySlowPath`/
  `unarySlowPath` → `tryObject*Op` → `ops` lambda → `invokeClassMethod`),
  `subsref`/`subsasgn` (`obj(...)` read/assign), custom `disp`/`display`
  (`displayObject`), super-method/super-ctor *base targets* (`superMethod`/
  `superConstruct` — upgrades P2's TW delegation), and any method/ctor invoked
  from a builtin. **Proven**: operator/subsref/display/super-call dual-engine
  suites green on the VM path; full suite 10926.
  - **Pausability:** at P4c these were C++-initiated (reached from a builtin or
    an arithmetic opcode's slow path), so a breakpoint fired but could not
    suspend. **P4d (below) lifts operator overloads onto in-bytecode frame-pushes
    (pausable).** Indexing (`subsref`/`subsasgn`) is the remaining C++-initiated
    path until its `INDEX_GET`/`INDEX_SET` frame-push lands.
- **P4d (done)** — **operator overloads VM-native (in-bytecode, pausable).** The
  arithmetic/comparison/logical opcodes (`ADD`…`OR`) and the unary opcodes
  (`NEG`/`NOT`/`CTRANSPOSE`/`TRANSPOSE`), when an operand is an object whose
  class defines the operator method, push a SAME-STACK frame for that method
  body (`VM::tryBinaryOpFrame`/`tryUnaryOpFrame` → `goto enter_frame`) instead
  of falling to `binary/unarySlowPath` → `tryObject*Op` → `callReentrant`. No
  save/restore (fast) and fully pausable. The operator method's parameters ARE
  the operands (binary `[lhs,rhs]`, unary `[operand]`). Resolution is shared,
  not duplicated: `Engine::resolveBinaryOpChunk`/`resolveUnaryOpChunk` reuse the
  existing `operatorMethodName`/`unaryOperatorMethodName` table, `methodFns`
  registry, `ensureClassMethodChunk`, and `enforceMethodAccess`. Because they
  key on `methodFns` (real `UserFunction`s), they NATURALLY exclude synthetic
  enum `eq`/`ne` (lambda-only, no `UserFunction`) and any uncompilable body,
  which fall through to the unchanged slow path. The op→token map is factored
  into `binaryOpString`/`unaryOpString`, shared by the slow paths and the frame
  helpers. **Proven**: `DebugSessionTest.BreakInsideClassdefOperator` (breakpoint
  inside `plus` pauses; `y == 7`); operator/object/enum dual-engine suites
  green; full suite 10927.
- **P4e (done)** — **`subsref`/`subsasgn` overloads VM-native (in-bytecode,
  pausable).** The indexing opcodes that dispatch a classdef overload —
  `INDEX_GET`/`INDEX_GET_2D`/`INDEX_GET_ND` and `CALL_INDIRECT` (a known
  variable `obj(i)` compiles to `CALL_INDIRECT`) for `subsref`, and `INDEX_SET`
  for `subsasgn` — push a SAME-STACK frame for the overload body
  (`VM::tryObjectSubsrefFrame`/`tryObjectSubsasgnFrame` → `goto enter_frame`)
  instead of the C++ `tryObject*` hook → `callReentrant`. Shared, non-duplicated
  resolution: `Engine::resolveSubsrefChunk`/`resolveSubsasgnChunk` reuse
  `methodFns` + `ensureClassMethodChunk` and marshal the MATLAB substruct args
  (`subsref(obj,S)` / `subsasgn(obj,S,val)`) via the existing `buildSubsStruct`.
  `subsref` returns one value into the destination; `subsasgn` always returns
  the object (value class → written back into the object register; handle →
  mutates shared state), so no no-output complication. Frame-push is added only
  where the overload is already dispatched (subsasgn only at `INDEX_SET`, the
  pre-existing surface — no new 2-D/N-D subsasgn behaviour). **Proven**:
  `DebugSessionTest.BreakInsideClassdefSubsref` / `BreakInsideClassdefSubsasgn`
  (breakpoints inside the overloads pause; `y == 20` / `y == 99`); subsref/Ring
  dual-engine suites green; full suite 10929.
  - **Perf note:** `callReentrant` snapshots the live register stack per call.
    It only fires on the OBJECT slow path (numeric/scalar operators keep their
    fast inline path), so hot numeric code is unaffected; object-operator-heavy
    loops pay the snapshot — the opcode frame-push above would remove it.

## P1–P4 net result

Under the VM backend, **all** classdef user code now executes on the VM (no
TreeWalker hop): instance methods (all call forms), constructors, super-calls,
get/set accessors, operator overloads, `subsref`/`subsasgn`, custom
`disp`/`display`, and function-handle callbacks (`cellfun`/`arrayfun`/`feval`).

**Pausable under the debugger** (in-bytecode same-stack frames): instance
methods, constructors, super-calls (P1/P2), get/set accessors (P4a/P4b),
operator overloads (P4d), and `subsref`/`subsasgn` — both via the indexing
opcodes and the `obj(i)`/`CALL_INDIRECT` form (P4e). i.e. essentially every
classdef body reached from running bytecode.

**On the VM but not suspendable** (genuinely C++-initiated — reached from a
builtin, with no in-bytecode opcode to push from, so they use `callReentrant`;
a breakpoint fires but cannot freeze-and-resume across the C++ boundary):
`disp(obj)`/`display(obj)` invoked by the display path, a method/operator used
as a callback inside a builtin (e.g. a `sort` comparator), function-handle
callbacks driven by `cellfun`/`arrayfun`/`feval`, super-call *base targets*
(`superMethod`/`superConstruct`), and C++-initiated construction (object-array
growth). All still execute on the VM.

The TreeWalker backend is unchanged; native builtin-class methods
(containers.Map, …) keep their C++ hook.

## State-machine callbacks (suspendable higher-order builtins)

Closes the last gap: higher-order builtins (`cellfun`/`arrayfun`/…) that loop
over user callbacks used to drive that loop on the C++ stack and run each
callback via `callReentrant` — so a breakpoint inside a callback could fire but
not suspend (the pause can't unwind through the builtin's C++ for-loop). Rather
than a fiber (rejected: per-platform machinery + WASM/Asyncify cost), the
builtin is re-expressed as a resumable **state machine** that returns control to
the dispatch loop between callbacks, so each callback becomes an ordinary
(pausable) VM frame — case A. No fiber / separate stack / Asyncify; identical on
every preset including WASM.

- **Foundation (done, `60b7a64a`)** — `core/callback_builtin.hpp`:
  `VmContinuation` (resumable native computation; `step(vm, prevResult, self)`
  pushes the next callback frame or finalizes) and `CallbackBuiltin`
  (`tryStart` → a continuation, or nullptr to fall back to the synchronous
  builtin). `CallFrame::cont`; `popCallFrame` routes a callback frame's return
  into `cont->step` (single choke point, all RET variants); `startContinuation`
  + `pushCallbackFrame` (resolve user-code handle → chunk → frame + attach
  cont); CALL dispatch consults `Engine::callbackBuiltin(name)` before the
  synchronous external path. `Engine` registry + `isUserCodeHandle`. Inert
  until a consumer registers.
- **cellfun (done)** — `libs/builtin` `CellfunContinuation` +
  `CellfunCallbackBuiltin`, registered via `registerCellfunCallbackBuiltin`.
  Drives `cellfun(@userfunc, c [, 'UniformOutput', tf])` one element at a time
  as pausable frames; `pack()` mirrors the synchronous `cellfun` helper's output
  (uniform numeric/logical array, or cell). Builtin handles, multi-output, and
  unsupported arg forms fall back to the synchronous `cellfun` (tryStart →
  nullptr). TW backend unchanged (the continuation is consulted only by the VM
  dispatch loop). **Proven**: `DebugSessionTest.BreakInsideCellfunCallback`
  (breakpoint inside the callback pauses on each of the 3 elements and resumes;
  `y == [10 20 30]`); 48 cellfun dual-engine tests green; full suite 10930.
- **arrayfun (done)** — `libs/builtin` `ArrayfunContinuation` +
  `ArrayfunCallbackBuiltin` (in library.cpp, where arrayfun is defined),
  registered alongside the synchronous `arrayfun`. Drives
  `arrayfun(@userfunc, A[, B…][, 'UniformOutput', tf])` element-by-element as
  pausable frames (per-element scalar args via `elemAsDouble`, multiple input
  arrays supported); `pack()` mirrors the synchronous lambda (uniform → DOUBLE
  matrix, else cell, shaped like the first input). Builtin handles / multi-output
  / size-mismatch fall back to the synchronous arrayfun. **Proven**:
  `DebugSessionTest.BreakInsideArrayfunCallback` (pauses per element, resumes;
  `y == [101 102 103]`); full suite 10931.
- **structfun (done)** — `libs/builtin` `StructfunContinuation` +
  `StructfunCallbackBuiltin` (struct.cpp), registered via
  `registerStructfunCallbackBuiltin`. Drives `structfun(@userfunc, S[,
  'UniformOutput', tf])` field-by-field (in `structFields()` map order, matching
  the synchronous helper) as pausable frames; `pack()` → uniform column vector
  (DOUBLE/LOGICAL) or n×1 cell. Builtin handles / multi-output / non-scalar
  struct fall back to the synchronous structfun. **Proven**:
  `DebugSessionTest.BreakInsideStructfunCallback` (pauses per field, resumes;
  `y == [10; 12]`); full suite 10932.

- **LoopContinuation + feval (done, `ba88b2d3`)** — generic core helper for the
  "apply handle to N items, collect, pack" shape (handle, n, makeArgs, pack,
  dest); a consumer is two lambdas, not a struct. feval(@userfunc, args…)
  single-output → `n == 1` (one frame). Name/string handle, multi-output → sync.
  Proof: `DebugSessionTest.BreakInsideFevalCallback` (`y == 43`).
  - **cellfun/arrayfun/structfun refactored onto LoopContinuation** — the three
    original bespoke continuation structs were removed and rebuilt as
    `makeArgs`/`pack` lambdas, so EVERY consumer now drives off the single
    `LoopContinuation::step` (one loop-driver, no per-builtin duplication).
    Their suites + debugger pause-proofs stay green.
- **splitapply + bsxfun (done, `2c956815`)** — `libs/builtin`. splitapply per
  group (bucket → makeArgs slices → pack column); bsxfun single-shot (forwards
  whole arrays). accumarray takes no user handle (untouched). Proof:
  `BreakInsideSplitapplyCallback` (`y == [3;7]`), `BreakInsideBsxfunCallback`.
- **bootstrp (done, `5e6b46e6`)** — `libs/stats`. Each replicate's statistic as
  a pausable frame; sample drawn lazily in makeArgs so RNG order matches the sync
  path. Proof: `BreakInsideBootstrpCallback`.

**WASM validated** — the `browser` preset builds clean (292/292, links
`numkit_ide.js`) with the continuation mechanism. The state machine uses no
fiber / separate stack / Asyncify, so it compiles + runs on every preset
including WebAssembly — the property that made it the right choice over a fiber.

Pausable higher-order builtins (clean `LoopContinuation` fit, **done**): cellfun,
arrayfun, structfun, feval, splitapply, bsxfun (core / builtin), bootstrp
(stats), nlfilter, makelut (image). Genuinely single-shot C++-initiated calls
(`disp(obj)` from the display path, a `sort` comparator) have no loop to suspend
and stay on `callReentrant` (on the VM, breakpoints fire but cannot suspend).

**Remaining class-1 — NOT a clean `LoopContinuation` fit (each has a
per-function complication; deferred, would need bespoke work or a behaviour
trade-off, NOT the uniform pattern):**
- `grouptransform` / `groupfilter` / `groupsummary` (builtin) — per-group like
  splitapply, but the output is not a simple per-group column: transform splices
  each group's result back into the original row positions, filter selects a row
  subset, summary aggregates. The continuation would need a bespoke `pack` per
  variant (more than the shared helper gives).
- `pulstran` (signal waveform) — the per-pulse loop lives inside the `pulstran`
  *library function* (called with a cb), not the `_reg`; converting it means
  restructuring that library function, not just adding a `CallbackBuiltin`.
- `fplot` / `fsurf` / `fcontour` / `fmesh` (graphics) — sample the handle per
  grid point inside a `try/catch` that SKIPS points where `f(x)` throws. A
  continuation frame's exception propagates up and aborts the whole call, so the
  skip-on-error semantics can't be preserved without extra machinery; also a
  niche debug target (plotting).
These were assessed and left on `callReentrant` deliberately (the clean ones are
all done). Picking any up means accepting its specific trade-off.

## Embedded-`.m`-wrapper path (for adaptive numerical solvers)

The clean way to make the adaptive numerical solvers' user code (objective /
integrand / RHS — always user code, no builtin-handle fast path) pausable is NOT
a hand-written C++ state machine (which would have to serialise RK45 / Brent /
quadrature state, and replicate the algorithm's exact summation/eval order to
stay bit-identical). Instead, implement the user-facing builtin in **`.m`** —
then its f-calls compile to ordinary VM frames (CALL/CALL_INDIRECT), pausable
**for free**, recursion/iteration natural, and the C++ `Value …(FnHandle,…)` API
stays as the synchronous path for embedders. This is how real MATLAB ships these.

- **Foundation (done)** — `Engine::registerBuiltinMSource(src)`: parses embedded
  `.m` source and registers each top-level `function` PERSISTENTLY (userFuncs_ +
  VM compiled table via registerFunctionAs) — the same path m-file loading uses,
  so they survive `clear` and dispatch on both backends. Pure C++/bytecode → no
  fiber/Asyncify, WASM-safe by construction.
- **fzero (done)** — registered via `optim::registerFzeroM` (embedded `.m`
  faithful port of findBracket + Brent in `local/fzero.cpp`). The objective is
  called from `.m` → pausable. Plain + closure handles work (`fn(x)` →
  CALL_INDIRECT unwraps closures); errors/multi-output match the C++ behaviour.
  **Proven**: `DebugSessionTest.BreakInsideFzeroObjective` (breakpoint inside the
  objective pauses per Brent eval, `y ≈ 2`); all 20 fzero dual-engine tests
  green; full suite 10939. The C++ `Value fzero(...)` API is retained.
- **integral (done)** — registered via `builtin::registerIntegralM` (embedded
  `.m` adaptive Gauss-Kronrod recursion in `math/integration/integration.cpp`).
  A C++ primitive `__gk15_nodes` supplies the abscissae/weights (no f-calls);
  the `.m` does the per-node f-evaluations + recursion. The fused per-node K/G
  accumulation mirrors the C++ `gaussKronrod15` loop in node order, so results
  are bit-identical to the retained `Value integral(...)` API. `'AbsTol'` option
  + reversed/equal bounds + finite/positive-tol checks match the C++.
  **Proven**: `DebugSessionTest.BreakInsideIntegralIntegrand` (breakpoint inside
  the integrand pauses on each GK15 node, `y ≈ 1/3`); all 22 integral
  dual-engine tests green; full suite 10940.
  - **Note:** numkit does not bind `varargin` when no extra args are passed, so
    embedded `.m` builtins use fixed optional params (`function r = integral(fn,
    a, b, opt, optval)` + `nargin` guards) rather than `varargin`.
- **ode45 (done)** — registered via `ode::registerOde45M` (embedded `.m`
  Dormand-Prince 5(4) in `ode45.cpp`). The RHS `f(t,y)` is called from
  bytecode → pausable; the adaptive RK45 step controller + Shampine dense
  output are the natural `.m` algorithm. Stages are **vectorised**
  (`yc + dir*h*(a51*k1 + … + a54*k4)`, `sum((er./sc).^2)`) so they dispatch to
  the SIMD-backed kernels and are bit-identical to the retained `Value
  ode45(...)` API. Options (RelTol/AbsTol/MaxStep/InitialStep/Refine), the
  HNW initial-step heuristic, FSAL, tspan-vs-Refine emit, and all error
  ids/edge checks mirror the C++.
  - **Register-limit lesson (new gotcha, documented):** the all-inline body
    (~30 Butcher coeffs + `k1…k7` + temporaries) overflowed the **255-register
    VM chunk limit**. `registerBuiltinMSource` *silently* swallows the
    `registerFunctionAs` failure (TW-only fallback) → `ode45` was
    "undefined function" **on the VM** with no install-time error, while the
    older C++ external kept the Ode45Test suite green and masked it. Fix: split
    the heavy arithmetic into `nk_dopri5_step` (the RK step) + `nk_ode_dense`
    (the interpolant) — each `.m` `function` is its own chunk/budget. Caught by
    running the source through `numkit_smoke.exe` (compiles `.m` at runtime,
    reports `register exhaustion`) and by the pause-proof gtest.
  **Proven**: `DebugSessionTest.BreakInsideOde45Rhs` (breakpoint inside the RHS
  pauses on every DOPRI5 stage, `y(end) ≈ exp(-1)`); all 10 Ode45Test
  dual-engine tests green; full suite 10941 (0 regressions). The external
  `ode.solvers/compat.ode45` alias is dropped — the top-level `.m` user
  function shadows on both backends.
- **ode23 (done)** — registered via `ode::registerOde23M` (embedded `.m`
  Bogacki-Shampine 3(2) in `ode23.cpp`). Same shape as ode45 with the simpler
  4-stage FSAL pair, cubic-Hermite dense output (only `k1`/`k4`), default
  `Refine=1`, and the 1/3 step exponent. Split into `ode23` + `nk_bs23_step` +
  `nk_bs23_hermite` (register-limit lesson applied up front). Vectorised stages
  → SIMD, bit-identical to the retained `Value ode23(...)` API.
  **Proven**: `DebugSessionTest.BreakInsideOde23Rhs`; all 11 Ode23Test
  dual-engine tests green; full suite 10942 (0 regressions). External alias
  dropped (top-level `.m` shadows).
- **fminsearch (done)** — registered via `optim::registerFminsearchM` (embedded
  `.m` Nelder-Mead simplex in `local/fzero.cpp`). Faithful transcription of the
  C++ `nelderMead`: same reflection/expansion/contraction/shrink constants, dual
  TolFun+TolX convergence, simplex seeding (`1.05·xi` or `0.00025`),
  kMaxIter=500; the point is always passed to `fn` as a 1×n row (like the C++
  `evalVecToScalar`). Split into `fminsearch` + `nk_nelder_mead` + `nk_nm_eval`.
  Output mirrors x0 shape; `[x, fval, exitflag]` with `exitflag=1`. Rosenbrock
  from `[-1.2 1]` reaches `[1.00002 1.00004]` — exactly the C++/MATLAB value
  (bit-identical). **Proven**: `DebugSessionTest.BreakInsideFminsearchObjective`;
  all 6 Fminsearch dual-engine tests (TW+VM) green; full suite 10943 (0
  regressions). External `registerFunction("fminsearch", …)` dropped.
- **nlinfit (done)** — registered via `stats::registerNlinfitM` (embedded `.m`
  Levenberg-Marquardt in `regress/nlinfit.cpp`). The model `fun(beta, X)` is
  always user code → every residual + central-difference Jacobian evaluation
  runs as bytecode (pausable). Faithful transcription of the C++ LM loop (λ
  schedule 1e-3 ×0.1/×10, central-diff `h = 1e-7·max(|βj|,1)` with `*inv2h`,
  tolFun/tolX = 1e-10, maxIter = 200, final-Jacobian refresh, MSE = SSE/(n-p),
  CovB = MSE·inv(JᵀJ)). The LM linear step uses built-in `JᵀJ + λ·diag`, `\`,
  `inv` instead of the C++'s hand-rolled Gauss elimination — converges to the
  same least-squares minimum (tight tolerances), and `nlparci`/`nlpredci`
  (unchanged C++) produce valid CIs from the `.m` outputs. nlinfit consumes no
  RNG, so caller noise realisations are unchanged. Split into `nlinfit` +
  `nk_nlinfit_jac` + `nk_nlinfit_model`. **Proven**:
  `DebugSessionTest.BreakInsideNlinfitModel`; all 7 NlinfitTest tests green
  (incl. the randn-seeded recovery + nlparci/nlpredci); full suite 10944 (0
  regressions). External `stats.regress/compat.nlinfit` alias dropped.
- **Remaining callback-bearing (follow-up, lower priority):** `nlpredci` (calls
  the model `fun` — a clean `.m`-wrapper candidate: single-shot prediction +
  query-Jacobian, no iteration; left on `callReentrant` for now). The bespoke
  `grouptransform`/`groupfilter`/`groupsummary`, `pulstran`,
  `fplot`/`fsurf`/`fcontour`/`fmesh` remain per-function specials (see above).
  All adaptive numerical solvers from the original queue (fzero, integral,
  ode45, ode23, fminsearch, nlinfit) are now pausable `.m` wrappers.
  - **Found + filed** (task #49, pre-existing, NOT P1c): a parameter named
    `i`/`j` inside a VM function frame resolves to the imaginary unit instead
    of the parameter. General VM identifier-resolution bug — surfaced via a
    test method param named `i`.
  Original two prerequisites (both resolved above — neither was a P1c dispatch
  bug):
  1. **Multi-output super-call guard.** `[a,b]=m@Base(obj)` — the VM
     multi-assign compiler does NOT reject `SUPERCLASS_REF` (only single-output
     `compileCall` does), so it mis-compiles to a call of the base *name*
     ("VM: undefined function 'Shape'"). Guard it (throw → ensureCompiled
     catches → hook fallback) until P2 lands super-calls in the VM.
  2. **`obj.prop(i) = v` inside a VM *function frame* throws "Not a double
     array".** Works at script scope and via the TW hook, but a method body
     compiled to a chunk and run as a frame mis-handles the compound
     property-element assign. A real latent VM bug (any function with an
     object param doing `o.p(i)=v`), independent of classdef — fix before
     routing prop-assigning methods to frames.

## Phases (each = its own commit, full suite must stay green — 0 regressions)

- **P1. classdef instance methods VM-native** (in-bytecode). Compile bodies to
  global chunks; `methodChunk`/`methodFns` on `BuiltinClass`; `CALL`
  (function-form, **done**), `CALL_METHOD`, `CALL_METHOD_MULTI` enter a frame
  for classdef methods. Debugger test proving the body runs on the VM.
- **P2. constructor + super-calls VM-native.** Ctor frame with seed; compile
  `SUPERCLASS_REF`. **DONE** — see Progress above.
- **P3. re-entrant VM-call-from-C++ foundation.** Nested run on `frames_`;
  verify run-loop re-entrancy. **DONE** — `VM::callReentrant` + function-handle
  consumer; see Progress above.
- **P4. accessors / operators / subsref + function handles on VM.** Route the
  C++-initiated paths through P3; `invokeClassMethod`/`invokeClassCtor`/
  `callFunctionHandleMulti` backend-aware. Debugger through all of it.

## Invariants

- TW backend behaviour unchanged throughout.
- Native builtin-class methods (containers.Map, table, …) keep the C++ hook.
- Every phase: build all presets affected, run the full gtest suite, 0
  regressions, dual-engine tests for the new path (including a debugger test
  that a classdef method pauses/steps under the VM).
