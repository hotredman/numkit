# Pausable callbacks — running user code under a builtin on the VM

When a builtin/hook calls back into user code (a classdef method, a function
handle, `cellfun(@f,…)`, `fzero(@obj,…)`, …), that user code must run on the
**bytecode VM** to be debuggable: the debugger (`DebugSession`, breakpoints,
stepping) lives only on the VM — the TreeWalker has none. This document is the
**decision guide** for making such callbacks run on the VM (and pause under the
debugger). For the chronological build log + commit history see
[`../VM_CALLBACKS_PLAN.md`](../VM_CALLBACKS_PLAN.md).

Everything here is **VM-backend** behaviour. Under the TreeWalker backend the
callbacks still run correctly; there is simply no debugger to pause them. All
three mechanisms below are pure C++/bytecode — **no fiber / separate stack /
Asyncify** — so they compile and run on every preset, **including WebAssembly**.

---

## The decision rule

**One question decides it: where is the callback invoked from, and what shape is
the driver?**

```
Is the callback invoked from a VM opcode
  (classdef method/ctor/super/get·set/operator/obj(...);  h(x) handle variable)?
│
├─ YES → (1) IN-BYTECODE FRAME-PUSH         ── always; strictly the best.
│
└─ NO — it's a C++ higher-order builtin looping over user callbacks.
        What shape is the driver loop?
        │
        ├─ Flat iteration over a collection (N known up front, calls
        │  independent, simple accumulate) — and/or there is a builtin-handle
        │  fast path worth keeping
        │        → (2) C++ STATE MACHINE (LoopContinuation)
        │
        └─ Adaptive / recursive algorithm (next call depends on previous
           results; objective/integrand/RHS is ALWAYS user code, no fast path)
                 → (3) EMBEDDED `.m` WRAPPER
```

Confirming factor (it almost always agrees with the shape):

| builtin-handle fast path? | mechanism |
|---|---|
| **yes** — `cellfun(@sin,c)` has a fast C++ path | state machine (keeps the fast path; the machine kicks in only for user-code handles) |
| **no** — objective is always user code | `.m` wrapper (free to take over) |

If neither (2) nor (3) is applied, the callback falls back to **`callReentrant`**
(runs on the VM, a breakpoint *fires* but cannot *suspend* across the C++
boundary — see Gotchas). That is the default for genuinely single-shot
C++-initiated calls (`disp(obj)` from the display path, a `sort` comparator) and
for not-yet-converted builtins.

One-liner: **opcode → frame-push; flat loop / has fast path → state machine;
adaptive algorithm / objective always user code → `.m`.**

---

## The three mechanisms

### 1. In-bytecode frame-push  *(classdef + `h(x)`)*

The call site is a VM opcode, so the handler resolves the target's compiled
chunk and pushes a normal frame (`pushCallFrame` + `goto enter_frame`). It
becomes just another frame on `frames_` — pausable/steppable for free, no
save/restore.

- **Entry points (`core/src/vm.cpp`):** `CALL` / `CALL_METHOD` /
  `CALL_METHOD_MULTI` (methods), `CALL` ctor branch + `CALL_SUPER_CTOR` /
  `CALL_SUPER_METHOD` (ctor/super), `FIELD_GET` / `FIELD_SET` (get·set
  accessors), `ADD`…`OR` / `NEG`…`TRANSPOSE` (operators), `INDEX_GET` /
  `INDEX_GET_2D` / `INDEX_GET_ND` / `INDEX_SET` / `CALL_INDIRECT`
  (subsref/subsasgn and `h(x)`).
- **Helpers:** `VM::tryBinaryOpFrame` / `tryUnaryOpFrame`,
  `VM::tryObjectSubsrefFrame` / `tryObjectSubsasgnFrame`,
  `pushCallFrame(..., ctorSeed)`; resolution shared with the C++ hooks via
  `Engine::resolveBinaryOpChunk` / `resolveUnaryOpChunk` /
  `resolveSubsrefChunk` / `resolveSubsasgnChunk` / `classGetter` / `classSetter`
  / `ensureClassMethodChunk`, with access enforced by
  `enforceMethodAccess` / `enforcePropGetAccess` / `enforcePropSetAccess`.
- **Pausable:** **yes**, fully (pause + resume + step).

### 2. C++ state machine — `LoopContinuation`  *(flat higher-order builtins)*

The builtin's C++ for-loop is removed; the builtin becomes a resumable state
machine that returns to the dispatch loop between callbacks, so each callback is
an ordinary VM frame. See [`core/include/numkit/core/callback_builtin.hpp`](../core/include/numkit/core/callback_builtin.hpp).

- **Foundation (`core`):** `VmContinuation` (resumable native computation),
  `LoopContinuation` (ready-made "apply handle to N items, collect, pack":
  `handle`, `n`, `makeArgs(i)`, `pack(results)`, `dest`), `CallbackBuiltin`
  (`tryStart` → a continuation or `nullptr`). `CallFrame::cont`; `popCallFrame`
  routes a callback frame's return into `cont->step(...)` (single choke point,
  all RET variants); `VM::startContinuation` / `pushCallbackFrame`;
  `Engine::registerCallbackBuiltin` / `callbackBuiltin` / `isUserCodeHandle`.
  The VM `CALL` dispatch consults `callbackBuiltin(name)` before the synchronous
  external path.
- **Pausable:** **yes** (each callback is a same-stack frame). No save/restore.
- **Fast path preserved:** `tryStart` returns `nullptr` for a builtin handle (or
  an unsupported arg form) → the synchronous C++ builtin runs unchanged.

### 3. Embedded `.m` wrapper  *(adaptive numerical solvers)*

The user-facing builtin is re-implemented in `.m`. Its f-calls compile to
ordinary bytecode (`CALL` / `CALL_INDIRECT`) → pausable for free; recursion and
adaptive loops are expressed naturally (no hand-written CPS, no state to
serialise). The C++ `Value foo(FnHandle,…)` API is **retained** as the
synchronous path for embedders.

- **Foundation (`core`):** `Engine::registerBuiltinMSource(src)` parses embedded
  `.m` source and registers each top-level `function` **persistently**
  (`userFuncs_` + the VM compiled table via `registerFunctionAs`) — the same path
  m-file loading uses, so the functions survive `clear` and dispatch on both
  backends.
- **Heavy non-`f` numerics** stay in C++ as a small primitive the `.m` calls
  (e.g. `__gk15_nodes` returns the Gauss-Kronrod abscissae/weights, no f-calls).
- **Pausable:** **yes** (the objective/integrand/RHS runs as bytecode frames).

### (fallback) `callReentrant`

A re-entrant C++→VM call (`VM::callReentrant`, save/restore of the outer VM
state). Used by `Engine::invokeClassMethod` / `invokeClassCtor` /
`callFunctionHandleMulti` when the active backend is the VM and none of the
above applies. Runs the body **on the VM**, but a debugger **pause cannot
suspend across the C++ boundary** — the breakpoint fires and surfaces as
`DebugStopException`. Prefer (1)–(3); leave on `callReentrant` only when there is
no opcode to push from and the function is single-shot or not worth converting.

---

## How to add a pausable callback

### A flat higher-order builtin → state machine

Register a `CallbackBuiltin` alongside the synchronous builtin; build a
`LoopContinuation` for the user-code path, return `nullptr` otherwise.

```cpp
struct MyfunCallbackBuiltin : CallbackBuiltin {
    std::shared_ptr<VmContinuation> tryStart(Span<const Value> args, std::size_t nargout,
                                             Value *dest, Engine &eng) override {
        if (args.size() < 2 || nargout > 1)       return nullptr; // sync reports arity
        if (!eng.isUserCodeHandle(args[0]))        return nullptr; // builtin handle → sync fast path
        if (!args[1].isWhatever())                 return nullptr; // type error → sync
        auto in = std::make_shared<...>(/* capture inputs by value */);
        auto cont = std::make_shared<LoopContinuation>();
        cont->handle = args[0];
        cont->n      = /* element count */;
        cont->dest   = dest;
        cont->makeArgs = [in](std::size_t i){ return std::vector<Value>{ /* args for call i */ }; };
        cont->pack     = [/*shape, mr*/](std::vector<Value> &r){ return /* build the one output */; };
        cont->results.reserve(cont->n);
        return cont;
    }
};
void registerMyfunCallbackBuiltin(Engine &e) {                 // call from the lib install
    e.registerCallbackBuiltin("myfun", std::make_shared<MyfunCallbackBuiltin>());
}
```

Keep it registered *next to* `registerFunction("myfun", &myfun_reg)` — the VM
consults the callback driver first; the synchronous `myfun_reg` is the fallback.

### An adaptive numerical solver → `.m` wrapper

Port the algorithm to `.m`; expose a C++ primitive for the heavy non-`f` parts;
register the `.m` instead of the C++ external (the VM/TW resolve a user function
before an external builtin). Keep the C++ `Value foo(...)` API.

```cpp
static const char *kMyfunM = R"NKM(
function r = myfun(fn, a, b, opt, optval)   % fixed optional params, NOT varargin
  if ~(strcmp(class(fn),'function_handle') || iscell(fn))
    error('numkit:myfun:fnType', 'myfun: 1st arg must be a function handle');
  end
  ...                       % f-calls here run as bytecode → pausable
end
)NKM";
void registerMyfunM(Engine &e) { e.registerBuiltinMSource(kMyfunM); }
```

Match the C++ behaviour (errors, options, edge cases) so existing tests pass;
numerical results need only agree by the tests' tolerance (mirror the C++
accumulation order if you also want bit-identity).

### A classdef dispatch path → in-bytecode frame-push

These are all done; follow the existing handlers in `core/src/vm.cpp` (resolve
the chunk via the `Engine::resolve*`/`classGetter` helpers, `pushCallFrame` +
`goto enter_frame`, enforce access first).

---

## Gotchas

- **`callReentrant` cannot suspend** across the C++ boundary — breakpoints fire
  but the session cannot pause/resume there. This is inherent to C++-initiated
  re-entrancy, not a bug; it's why (1)–(3) are preferred.
- **`varargin` is not bound** when a `.m` function is called with no extra args.
  Embedded `.m` wrappers use **fixed optional params** + `nargin` guards
  (`function r = f(a, b, opt, optval)`), not `varargin`.
- **A closure handle is a cell** `{handle, captures…}`. In `.m`, guard with
  `strcmp(class(fn),'function_handle') || iscell(fn)`; `fn(x)` calls it correctly
  (CALL_INDIRECT unwraps the closure).
- **Keep the C++ `Value foo(FnHandle,…)` API synchronous** — only the
  registered, user-facing function gets the pausable treatment.
- **State-machine fast path:** a `CallbackBuiltin::tryStart` MUST return
  `nullptr` for builtin handles / unsupported forms so the synchronous builtin
  (and its fast paths) stays in charge.
- **A `.m` chunk has a 255-register limit** (register VM). A big solver body
  (e.g. DOPRI5: ~30 Butcher coefficients + `k1…k7` + per-stage temporaries)
  overflows it and the VM compile fails. **Split the heavy arithmetic into a
  helper `function`** — each `.m` function is its own chunk with its own
  budget (ode45 factors the RK step into `nk_dopri5_step` and the dense
  interpolant into `nk_ode_dense`). This is also cleaner: the Butcher step is a
  natural unit.
- **`registerBuiltinMSource` swallows a VM-compile failure silently** — on a
  failed `registerFunctionAs` it keeps only the TreeWalker `userFuncs_`
  registration (intended m-file fallback). So a too-big / malformed wrapper
  registers on the TW but is **"undefined function" on the VM**, with no error
  at install time. **Always add a VM-level pause-proof test** (`DebugSession`
  break inside the callback) for every embedded `.m` wrapper — it is what
  surfaces a silent VM-registration failure. A quick pre-build check: extract
  the source and run it through `numkit_smoke.exe`, which compiles `.m` at
  runtime and reports `register exhaustion` loudly.

---

## Inventory

**Pausable — in-bytecode frame-push (classdef + handle variable):** instance
methods (all call forms), constructors, super-calls (calling body), `get.Prop` /
`set.Prop`, operator overloads, `subsref` / `subsasgn`, `h(x)`.

**Pausable — state machine (`LoopContinuation`):** `cellfun`, `arrayfun`,
`structfun`, `feval`, `splitapply`, `bsxfun`, `bootstrp`, `nlfilter`, `makelut`.

**Pausable — embedded `.m` wrapper:** `fzero`, `integral`, `ode45`, `ode23`,
`fminsearch`.

**On the VM, not suspendable (`callReentrant`):**
- single-shot C++-initiated: `disp`/`display` from the display path, super-call
  *base* targets, C++-initiated construction (object-array growth).
- **not yet converted (follow-up):** `nlinfit`
  (→ `.m` wrapper); `grouptransform`/`groupfilter`/`groupsummary`, `pulstran`,
  `fplot`/`fsurf`/`fcontour`/`fmesh` (bespoke per-function — see
  VM_CALLBACKS_PLAN.md for why each is not a clean fit).
