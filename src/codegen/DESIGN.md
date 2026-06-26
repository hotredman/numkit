# numkit codegen — design

Status: **design + early implementation** (branch `feat/codegen`).
This document records the load-bearing decisions; the rationale lives
here so it is not lost in chat history.

## 1. Goal

Speed up numkit execution by **transpiling numkit code to C++ and
compiling it with an external compiler (AOT)** — *not* by building a
JIT. We reuse the existing runtime: the emitted C++ calls the same
`Value`, builtins, and `libs` the interpreter uses.

Why transpile rather than JIT: emitting C++ that calls the existing
runtime is an order of magnitude less work than an LLVM/JIT backend
(no IR, no codegen backend; the C++ compiler does the optimisation),
the output is debuggable, and it produces a real deployable artifact
(a MATLAB-Coder-style "compile my numkit program to a native lib").

Target deployment (BOTH are goals — clarified 2026-06):
- **Standalone AOT** — compile a call tree from an entry point into a
  native library/binary; no interpreter in that artifact. A `numkit build`
  driver is the product shape (§8 M4). Programs that call uncompiled
  library functions compile via the Value-ABI bridge (§6a).
- **Tiered acceleration** — compile hot functions and load them back into
  a LIVE numkit session (interpreter stays; hot paths go native). This is
  "codegen-as-plugin": the compiled artifact is loaded through the plugin
  mechanism (§6b).

Both share the runtime C-ABI (§6a/§6b). That C-ABI is ALSO the foundation
for two **in-scope sibling goals** (not codegen per se, but the same hub):
host **embedding** (C/Python/Rust/JS-via-WASM drive numkit) and a
**plugin system** (numkit loads third-party native extensions). The C-ABI
work therefore serves codegen-tiered + embedding + plugins at once.

## 2. The spine is type inference, not the emitter

The ~37× interpreter gap vs MATLAB's JIT is the cost of **boxing**:
every operation flows through a dynamically-typed `Value`. Naive
transpilation that emits `Value`-per-operation just moves that cost
into compiled code (~2-5×, not the prize). The win requires proving
types so a scalar can be emitted as an unboxed `double` instead of a
`Value`.

Therefore the project's real work is a **static type-inference pass**
(~70%); the C++ emitter is the comparatively easy backend (~30%). The
inference pass is also independently valuable (editor diagnostics:
type-instability, shape mismatch) and is the same analysis any future
JIT would need.

## 3. Type lattice (implemented — `type_lattice.hpp`)

`InferredType = Bottom | Concrete(dtype, shape) | Dynamic`, propagated
by a forward dataflow over the CFG; `join` is the least-upper-bound at
control-flow merges.

- **Bottom** — unreachable / identity of join.
- **Concrete(dtype, shape)** — a definite type. `dtype` is `ValueType`;
  `shape` is `Unknown | Scalar | KnownDims(r,c)` (2-D MVP).
- **Dynamic** — top; statically unknown → stays a boxed `Value`.
- `join`: Bottom is identity, Dynamic absorbing, differing-dtype
  Concretes collapse to Dynamic (type-instability → must box), same
  dtype keeps Concrete with `joinShape`.
- `isUnboxableScalar()` — the transpiler's green light (Concrete scalar
  of a primitive numeric dtype).
- **Const facet** (`ConstVal`) — see §4; shape often depends on argument
  *values* (the `n` in `linspace`/`zeros`), so the analysis tracks
  compile-time-known constants alongside types (SCCP-style).

## 4. How types are determined — transfer functions

Each builtin has a **type transfer function**: `(arg abstract types +
constants) → result abstract type`. `linspace` is the worked example:
- dtype: real endpoints → `double`; complex endpoint → `complex`;
  `single` → `single`; integer endpoints → error (MATLAB rejects).
- shape: always `1×N` row; `N` = 3rd-arg **value** if a known constant
  (→ `KnownDims(1,N)`), else 100 for the 2-arg form, else row-vector of
  unknown length (still an unboxed `double` buffer — a big win even
  without the length).

This is why a **const-value facet** is mandatory: shape depends on
values, not just types.

Managing ~2400 builtins:
- ~**10-15 parameterised templates** cover most: size-constructor
  (`zeros`/`ones`/`linspace`/`eye`/`:`), elementwise type-preserving
  (`+`/`.*`/`sin`/`abs`), reduction (`sum`/`mean`/`max`), shape-query
  (`size`/`numel`), cast (`double`/`int8`). A builtin just references
  its template.
- A tail of **bespoke** rules for the irregular ones (`fft` real→complex,
  `sort` multi-output, `linspace` complex promotion).
- **Unknown builtin → `Dynamic`** (boxed). Sound fallback; coverage
  determines how much unboxes, not correctness. Start with the hot
  ~50-100; grow.

Transfer rules are **extracted and validated against the real
implementation** (differential extraction): generate sample inputs of
the abstract types, run the actual numkit/MATLAB implementation, check
the observed `class`/`size`/`isreal` matches the rule's prediction.
Rules are pinned to ground truth, not guessed. (This already caught
that `linspace` errors on integer endpoints.)

## 5. Value representation — tiers

Chosen by the inferred type; bridges between tiers are cheap because
they reuse numkit's own `DataBuffer` (PMR-backed, COW, dims).

| inferred type | C++ representation | note |
|---|---|---|
| **scalar** (`isUnboxableScalar`) | `double` / `std::complex<double>` / `int32_t` … | **never `Value`** — this is the whole prize |
| **array** (Concrete dtype+shape) | a `Value` container + **unboxed `double*` access** in hot loops | one header per array, amortised; zero-copy bridge to lib calls |
| **Dynamic** | `Value` (full dynamic dispatch) | fallback |

Key facts that make this cheap:
- A **scalar `Value` does not allocate** — `Value` is a 16-byte
  `{ double scalar_; HeapObject* heap_; }` with `heap_ == nullptr`
  meaning "inline scalar" (small-value optimisation, like NaN-boxing).
  So crossing a `Value` boundary with a scalar is a two-word copy + a
  tag branch, not a malloc.
- The 37× problem is **per-operation** boxing, not per-array. One
  `Value` header around an N-element buffer is negligible; a `Value`
  scalar churned in a tight loop is fatal. Hence: scalars unboxed,
  arrays may stay `Value`.
- v2 optimisation: compile-time-known small non-escaping array →
  stack `double[N]`, skipping even the `Value` header.

Not `std::vector<double>` (wrong allocator, no dims, no COW, copy on
every lib bridge) and not raw `double*` (no ownership/lifetime/dims).

## 5a. Native-shape tiers in practice — runtime-dim 2-D + scalar math

The **array** tier above splits, by what inference can prove about the
shape, into concrete representations the emitter lowers WITHOUT the
runtime:

- **KnownDims 2-D** — both dims compile-time constants; `rowsVar`/`colsVar`
  are literals baked into the column-major index math.
- **Runtime-dim 2-D** — a rank-2 shape with ≥1 dim unknown until run time
  (e.g. `zeros(m,n)` with `m`,`n` params). Represented as an **`NDims`
  rank-2 `ndRuntimeLocal`**: a flat owned `std::vector<double>` (column-
  major) plus per-axis **companion size vars** `<name>_dN` hoisted at
  function entry (`= 0`) and set by the producer that builds the matrix.
  Indexing/`numel`/`size` read the companions.

The key that lets ONE lowering serve both is **`dimExpr(a, k)`** (emitter):
the rank-agnostic axis accessor returning a KnownDims matrix's
`rows`/`colsVar` literal or a runtime matrix's `ndDims[k]` companion. Any
2-D op written against `dimExpr` works in both tiers; the runtime variant
additionally assigns the destination's companions before filling.

Coverage of the runtime-dim 2-D tier (each a differential e2e vs the
interpreter; capstone composes them all):
- **construct** — `zeros/ones(m,n)`, `diag(v)`→diagonal matrix,
  `repmat(rowVec,p,q)`, `[r1;r2;…]` (vertcat of rows), `[c1 c2 …]`
  (horzcat of columns), `[A B]`/`[A;B]` (concat of matrices).
- **consume** — `A'` transpose, element-wise `A±B`/`A.*B`/`A./B` +
  scalar-broadcast `A±s`/`A*s`, `A*B` matmul, `A*x`/`r*A` matrix·vector,
  `A(:,j)`/`A(i,:)` slice read, `A(:)` flatten, `numel`/indexing.
- **mutate** — `A(i,j)=v` scalar write (`indexN_set`), `A(:,j)=col`
  contiguous column write, `A(i,:)=row` strided row write.

Order-dependent **reductions** (`sum`/`prod`/`sort`/…) and **complex-
capable math** (`sqrt`/`log`/`asin`) deliberately stay **bridged** — a
native re-implementation could diverge from the interpreter's exact
rounding/branch, which the soundness contract forbids (refuse or bridge,
never miscompile).

**Scalar / elementwise math** lowers natively when it is total on ℝ and
bit-identical to numkit's own scalar implementation: the `unaryMathStd`
(sin/cos/tan/atan/sinh/cosh/tanh/exp/expm1/floor/ceil/round/fix/abs/asinh/
erf/erfc/**gammaln**) and `binaryMathStd` (atan2/hypot/**rem**) maps, plus
the no-`std`-fn scalar scalings **deg2rad/rad2deg** (numkit's exact
constant inlined). `mod` is intentionally left bridged — it is the
Dynamic-tier exemplar across the test suite and scalar `mod` is correct
via the bridge.

## 6. ABI / boundaries

AOT model: **no interpreter at runtime.** The artifact's entry point is
a typed signature, callable from C / Python / host:

```cpp
// f.m:  function y = f(a,b); y = a+b; end   (all inferred scalar double)
double f(double a, double b) { return a + b; }   // that's the whole thing
```

`Value` still appears, but **not** because of an interpreter — only at:
1. **calls to uncompiled library functions** (`fft`, `adapthisteq`, …
   take `Value`; some have raw-buffer entries usable unboxed);
2. **dynamic-fallback** values inference could not type;
3. as the **array container** (array args/returns are `Value`-typed —
   cheap, not interpreter overhead).

Inter-function calls inside the compiled region use **specialised
unboxed signatures** + inlining (the C++ compiler erases the boundary);
the residual cost of a `Value`-typed inner call is not allocation but
**optimisation opacity** (the compiler can't fuse/vectorise across it).
Specialised signatures are a **v2** optimisation — v1 transpiles a
whole region / inlines, so there are no inner boundaries to optimise.

A boxed `Value`-ABI thunk (`Value f_boxed(const Value&…, mr)`) is
**only** needed for the optional **tiered / embedded-interpreter** mode
(interpreter live, swaps in compiled hot functions). Not part of the
AOT v1.

## 6a. The Value-ABI bridge (calling the runtime)

The bridge makes §6's points 1–2 concrete: how compiled code calls an
**uncompiled** builtin/library function (`fft`, `sort`, `adapthisteq`, …)
and how a **Dynamic** value lives. It is the realisation of the §5 Dynamic
tier: *a Dynamic value IS a `numkit::Value`, and an operation it can't
unbox dispatches to the runtime.* It is NOT a new wall — it is the slow,
always-correct fallback beside the unboxed fast path.

**Two emission modes (the key decision).**
- **self-contained** (today): no bridge. An uncompiled builtin / Dynamic
  value is REFUSED. The artifact depends only on the C++ stdlib — what
  makes the e2e gate trivial and hot kernels deployable standalone.
- **bridged** (opt-in): the artifact `#include`s numkit headers and links
  `libnumkit`. An uncompiled builtin / Dynamic op lowers to a runtime call.
  Pay this only when the program needs library functions.
  Mode is a flag; self-contained stays the default for the hot-kernel path.
  (Equivalently: the artifact links numkit iff any bridge call was emitted.)

**The clean boundary — a separate bridge runtime with an OPAQUE C ABI.**
The generated artifact must NOT depend on numkit's C++ Value layout or its
transitive dependency graph. So the bridge is its own shared library,
`nk_codegen_rt`, which internally owns numkit (`Value` + a `StandardEngine`)
and exposes a minimal, stable, opaque C ABI:

```c
typedef struct nk_val_s* nk_val;                 // opaque — Value never leaks
nk_val nk_box_scalar(double v);
nk_val nk_box_array (const double* p, size_t len);   // copies at the boundary
nk_val nk_call(const char* name, const nk_val* args, size_t n,
               size_t nargout, nk_val* extra_outs);   // -> first result
double nk_unbox_scalar(nk_val);
void   nk_unbox_array (nk_val, double* out, size_t len);
size_t nk_numel(nk_val);
void   nk_retain(nk_val);  void nk_release(nk_val);
```

Grounded on the runtime's existing name-dispatch (`callFunctionHandleMulti`
/ `feval`); the `StandardEngine` that holds the builtin registry is a
function-local static INSIDE `nk_codegen_rt` — encapsulated, not a global
the generated code touches. A handle is a heap `numkit::Value*` cast to
`nk_val`; boxing a scalar/array copies in; results are owned handles.

**Generated code stays clean** — it #includes no numkit header. The codegen
PRELUDE provides a tiny RAII wrapper `nk_rt::val` over `nk_val`
(auto-`nk_release` in its destructor, plus box/call/unbox helpers), so the
emitted body is leak-free and ergonomic and the bridged artifact links only
`nk_codegen_rt` via the C ABI.

Why this over "use `numkit::Value` directly + link `libnumkit`": the opaque
C ABI is the minimal stable FFI boundary — the artifact is decoupled from
numkit's header, binary layout, and dep graph (no hand-rolled transitive
static-lib capture); the engine is an implementation detail; lifetimes are
RAII. One import lib, one C ABI.

**Box / unbox.** scalar → `nk_box_scalar` (a tiny heap handle; boundary
only, never a hot loop). array `(p,len)` → `nk_box_array` (copies);
result array → `nk_unbox_array` into a local buffer. Dynamic result →
kept as an `nk_rt::val` (the Dynamic tier). Copies live only AT the boundary
(an uncompiled call); a zero-copy view is a later optimisation.

**Dynamic tier (scalar) — realised (A1/A2/A5).** In bridged mode an un-typeable
SCALAR local is an `nk_rt::val` (owned RAII handle, copy = deep clone via
`nk_clone`); its arithmetic / comparison / unary ops dispatch to the runtime
through `nk_rt::binop` / `unop` (`Engine::applyBinaryOp` — overload-aware), and
an un-typeable builtin result stays boxed (`call_dyn`) — interpreter-speed at
those sites (exactly Dynamic's assigned cost) while every *typed* value stays
unboxed. A Dynamic `if` / `while` condition reduces to MATLAB truthiness
(`val::truth`) — a non-poisoning sink that lets an un-typeable value steer
*typed* branches. A Dynamic VALUE also crosses the function boundary BOXED — a
boxed `nk_val` RETURN (A3.5) and an `nk_rt::val` by-value PARAM — so an
un-typeable value flows fully in and out, not only via its typed consequences.
Covered: scalars and 1-D/2-D/N-D real arrays (+ complex 1-D) — arithmetic /
comparison / unary ops, un-typeable calls (Dynamic args A3 + boxed result),
boxed param/return, `if`/`while` conditions, `z(i)` indexing (`nk_index`
resolves the index/call ambiguity exactly as the interpreter), and shape-
preserving array boxing (`nk_box_matrix` / `nk_box_array_nd`, column-major).
**Remaining (marginal, all sound-refused → the function falls back to the
interpreter):** complex 2-D / N-D Dynamic arrays, and multi-subscript Dynamic
indexing. Multi-output reuses `nargout`/extra_outs.

**Cleanliness / soundness.** No-kludge litmus: delete the bridge and bridged
mode is gone; uncompiled builtins are refused (self-contained), still
correct. The bridge only ENABLES more programs; it never changes a typed
value's semantics. Correctness ⊥ the bridge.

**Build / link.** `nk_codegen_rt` is built by CMake (which resolves numkit +
Highway/matio/zlib normally). A bridged artifact links ONLY
`nk_codegen_rt` (one import lib) via the C ABI — a short, stable link line,
so the e2e is robust again. Skip the e2e cleanly if `nk_codegen_rt` was not
built.

**v1 scope:** opt-in bridged mode; an uncompiled builtin CALL lowers to
`nk_call` (single output); box scalars + 1-D arrays; result scalar / array /
Dynamic. **Deferred:** multi-output bridged calls; Dynamic call *arguments*
(A3) and Dynamic *arrays* (A4) — scalar Value-arith dispatch (`a+b` on
Dynamics) is now done (A1/A2/A5); a boxed (`nk_val`) param/return ABI; object
boxing across the bridge; zero-copy array views; 2-D array box/unbox.

**Build plan (bricks):**
1. ✅ `nk_codegen_rt` — the opaque C ABI (box/call/unbox/numel/release) over
   an encapsulated default `StandardEngine`; gtests check `nk_call("sin",…)`,
   sum/sort round-trips. (Built into the test binary; the standalone shared
   lib for AOT artifacts is brick ③.)
2. ✅ bridged emission (SCALAR) — opt-in `BridgeOptions`; `nk_rt::bridge_scalar`
   helper in the prelude; `emitBuiltinCall` emits a C-ABI call for an
   un-lowerable builtin ONLY when inference proves the result is a real scalar
   (Contract 2), else still throws. Demo: `sign` (registry-typed scalar, no std
   form). Off ⇒ TU stays stdlib-only (the litmus). Array-valued bridging +
   Dynamic locals (an array result needs an owned-buffer storage kind) = a
   later layer.
3. ✅ aot link — `nk_codegen_rt` built as a standalone shared lib (CMake DLL
   target) EMBEDDING the runtime + EXPORTING only the C-ABI (NK_RT_API:
   dllexport when NK_RT_BUILDING_DLL, dllimport when NK_RT_USE_DLL, else
   plain — so the static-into-gtest compile is unchanged). `CompileOptions`
   {includeDirs, defines, linkLibs} threads through the AOT harness; a bridged
   compile adds `/I<bridge>`, `/DNK_RT_USE_DLL`, and links the import lib.
4. ✅ e2e (CodegenBridge) — `y = sign(x)` (un-lowerable) compiles bridged,
   links nk_codegen_rt, the runtime DLL is copied beside the artifact, it
   RUNS, and -1/0/1 match the interpreter. The opaque handle design keeps all
   Value alloc/free inside the DLL (no cross-module heap / CRT issue).

Array layer ✅: a bridged call whose result inference proves an array, assigned
to the output, fills the out-param via `nk_rt::bridge_into`; a bridged array
LOCAL resizes an owned `std::vector` via `nk_rt::bridge_to_vec`; elementwise
array arithmetic (`y = x + w.*2`) lowers to a fill loop over owned/out-param
storage (box scalar/array-var args -> nk_call -> unbox). Demos: `y = sin(x)`
(e2e CodegenBridge.BridgedArrayResult / BridgedArrayLocal). **Scalar Dynamic
tier done (A1/A2/A5); remaining: Dynamic ARRAYS + indexing (A4), Dynamic call
args (A3), and a boxed param/return ABI.**

## 6b. Embedding C-ABI + plugin system

The value-marshaling C-ABI of §6a is not codegen-specific — it is **the
numkit runtime C boundary**, serving THREE roles over one interface
(marshal `Value`s across a stable C boundary + dispatch by name):
1. **embedding** — a host (C / Python / Rust / JS-via-WASM) drives numkit;
2. **codegen bridge** — a compiled artifact calls the runtime (§6a);
3. **plugins** — numkit calls externally-supplied functions.

So the bridge runtime is promoted to a proper runtime C-ABI (working name
`numkit` C-API; today's `nk_codegen_rt` is its first slice). One boundary,
name dispatch — NOT a C-ABI per toolbox, NOT for internal C++ layers.

**Surface.** engine lifecycle (or a default) + `nk_eval(code)` +
`nk_call(name, args, nargout)` + value marshal (box/unbox scalar / array /
complex / string / …, numel/size/class) + **error translation**.

**Exception translation (mandatory; today's gap).** A C++ exception MUST
NOT cross an `extern "C"` frame (UB). Every C-ABI entry wraps in
try/catch and surfaces failure via an out `nk_error*` (code + message);
no throw escapes. (The current `nk_codegen_rt::nk_call` lacks this — a
MATLAB `error(...)` inside a bridged builtin would throw across the C
frame. First brick fixes it.)

**Ownership / allocator.** Values cross only as opaque handles built by the
C-ABI; a consumer (plugin/host) never `new`/`delete`s a `Value` itself, so
the PMR/allocator stays consistent. box -> owned handle; release frees.

**Plugins (the inverse direction).** A plugin is a shared library that:
- exports `nk_plugin_register(nk_registry)` and reports the ABI version it
  was built against (`nk_plugin_abi_version()`);
- registers functions of the stable C signature
  `void fn(const nk_val* args, size_t nargs, size_t nargout, nk_val* outs, nk_error*)`.
numkit `dlopen`/`LoadLibrary`s it, version-checks, and adds its functions
to the builtin name-dispatch — so they are callable identically from MATLAB
code, the embedding API, and the codegen bridge. **ABI versioning is then a
hard commitment**: the C-ABI is frozen + versioned; plugins ship against a
version and are rejected on mismatch.

**codegen-as-plugin (the payoff / tiered mode).** A codegen artifact that
also exports `nk_plugin_register` IS a plugin: numkit loads its own
AOT-compiled functions and swaps them in for the interpreter on hot paths
— exactly §6/§7's tiered mode. Plugins are the loading mechanism for
codegen output.

**Native vs WASM.** Native (.dll/.so) is feasible and natural on this
C-ABI — that is the path. **WASM plugins are a separate, hard problem**
(no in-browser `dlopen`; needs Emscripten `MAIN_MODULE`/`SIDE_MODULE` +
shared memory/table) and stay deferred; the WASM target remains statically
linked. (WASM plugins' one upside — sandboxing — is why one might revisit
them later; native plugins run in-process with full trust, like MEX, and
can crash the host. Documented, accepted for a source-available tool.)

**Build plan (bricks):**
1. ✅ error translation — `nk_error` + try/catch in every C-ABI entry; a
   bridged/embedding call that errors sets `nk_error`, never throws across
   `extern "C"`. (Fixed the §6a gap.)
2. ✅ promote to the runtime C-API — `nk_eval` over the encapsulated engine
   (stateful workspace); the marshal + `nk_call` already existed. Embedding
   usable from C. (Multi-engine create/destroy = later; the default engine
   covers the bridge + tiered + first embedding.)
3. ✅ plugin ABI + loader — `nk_plugin.h` (`nk_plugin_register` /
   `nk_plugin_abi_version` + `nk_host_api` table); `nk_register_fn` adapts a
   plugin `nk_fn` onto `Engine::registerFunction` (NO core change — name
   dispatch already consults it); `nk_load_plugin` = `LoadLibrary`/`dlopen` +
   version check + idempotent per path. Model = host-API-passed-in (no
   symbol coupling, sidesteps the Windows import-lib problem).
4. ✅ plugin e2e — `sample_plugin.cpp` built as a real `.dll`, loaded,
   `nk_sample_triple(...)` dispatched through the engine; also callable from
   an `nk_eval`'d script.
5. ✅ codegen-as-plugin (tiered) — `emitScalarPlugin` emits the compiled
   function + `nk_plugin_register`; compiled to a `.dll`, loaded with
   `nk_load_plugin`, called native through engine dispatch, diffed against
   the interpreter (`nk_hot(3,4)=21`). Array layer ✅ (partial): a scalar-output
   fn may take double-VECTOR params — the wrapper unboxes them into buffers
   (e2e `nk_hotsum([1..5])=15`). An array OUTPUT still needs the output-size
   protocol (later).

## 7. Dynamic-feature policy (the compile wall)

`eval` / `evalin` / `evalc` / `assignin` / `feval(<non-constant
string>)` / `who` / `whos` / `exist` / `inputname` / dynamic field
`s.(expr)` are **incompatible with AOT** for two independent reasons:
(1) the executed code is known only at runtime; (2) they read/mutate a
workspace **by variable name**, which breaks the static-slot model and
invalidates inference downstream.

Policy (matches MATLAB Coder):
- **Detect** the eval-family (a fixed set of builtin names) during the
  inference pass.
- **Mark the enclosing function non-compilable**; emit a clear
  diagnostic ("`evalin` not supported for compilation, line N"). Leave
  it interpreted, or fail the build if it is on a hot path the user
  asked to compile. Do **not** pretend to compile it.
- **Contamination propagates**: `assignin('caller', …)` mutates the
  *caller's* workspace, so a function whose callee does that cannot
  trust its own SSA types either → treat conservatively (affected
  variables drop to `Dynamic`; if too many do, the function is not
  worth compiling).
- **Narrow win**: a **constant-string** `eval`/`feval` can be parsed and
  spliced at compile time (as if written inline) — only when the string
  is literal and does not escape to a workspace.
- **Totality option (later)**: embed the interpreter as a fallback
  runtime; eval-containing functions stay bytecode, bridged via the
  `Value`-ABI thunk (§6). Heavier artifact; only if real programs need
  it.

In practice eval-family lives in scripting/glue code, not the numeric
hot paths worth compiling — so "detect + diagnose + refuse" costs
almost nothing for the real use case.

## 7a. Object model policy (classes)

Same shape as §7: a **supported subset** with everything else detected and
refused with a diagnostic — never miscompiled. Locked decisions (from the
design discussion; rationale preserved here so it is not lost):

**Representation — object vs reference are separate.**
- **value class** (default) → a plain C++ `struct Foo { … }` held **by
  value**; assignment copies (MATLAB value semantics). A method that
  "mutates" is **value-in / value-out**: MATLAB `obj = m(obj,a)` →
  `Foo Foo__m(Foo self, …)` returning the modified copy. NOT an in-place
  member mutator.
- **handle class** (`< handle`) → object is still a plain `struct Foo`;
  the *variable* holds a reference wrapper **`nk_rt::handle<Foo>`** (a thin
  type over `std::shared_ptr<Foo>`). Methods mutate in place via `self->`.
  Identity `==` is pointer equality at the wrapper. We do **NOT** inherit
  from `shared_ptr`, and we do **NOT** put a mandatory polymorphic base on
  every class (that would tax every object with a vtable and kill the
  monomorphic-unboxing win). A polymorphic base is introduced **only**
  inside a real closed-world subclass hierarchy that is actually used
  polymorphically.
- lattice: a new concrete tier `Object(classId)` (carries class identity +
  whether handle), sitting beside scalar/array/dynamic. A monomorphic
  object unboxes to its struct exactly as a scalar unboxes to `double`.

**Dispatch.**
- **monomorphic** (the exact class is statically proven at the call site)
  → direct call `Foo__method(self,…)`, inlinable. This is the fast path
  and is achieved by **inference/devirtualization**, not by any base
  class.
- **polymorphic, closed-world** (all subclasses known at compile time) →
  later: a class-id **type-switch** (guarded monomorphization) — correct
  but pays a per-call guard and boxes at type-inconsistent merges. NOT v1.
- the speed prize requires monomorphism; vtables/type-switch are the
  slow-but-correct fallback, not a speedup.

**The real wall (narrow, not "all of OOP").** It is NOT "codegen can't see
the class file" — in a whole-program compile it sees them all. It is:
*an object whose class was not compiled into this C++ object model flows
into compiled dispatch.* Causes: a class left interpreted; a class that
post-dates the compiled artifact; a class named by a runtime string
outside the compiled set (`feval(name)`). These reduce to the §7 root
(runtime-decided) and are **refused** (or, with the §7 totality option,
bridged to the interpreter as a boxed `Value`).

**Refused in v1 (explicit diagnostic, never wrong code):** inheritance /
polymorphic dispatch; `get.`/`set.` accessors; `Dependent`/validation
properties; `subsref`/`subsasgn` and operator overloads; `dynamicprops`/
`addprop`; `metaclass`/reflection; `delete`/`isvalid`/events; cyclic
handle graphs (refuse on a detected back-reference, else the bare
`shared_ptr` would leak). `delete`/`isvalid` fidelity (a `ControlBlock`
with a valid-flag) is a deliberate later upgrade of the `handle<T>`
wrapper, scoped to when it is needed.

**v1 supported subset:** a single value class (and a single handle class
with bare `shared_ptr`, no `delete`), plain stored properties with
inferred field types, a constructor, and methods called **monomorphically**
(exact class known). Everything above → refuse.

## 8. Milestones

- **M0** — measure the prize. **DONE.** biquad scalar recurrence,
  Arrow Lake / desktop-fast Release / N=131072, ns/sample:
  transpiler-faithful unboxed-with-Value-I/O **1.56** | raw-array native
  1.59 | MATLAB JIT loop 2.70 | numkit filter() 5.85 | MATLAB filter()
  7.22 | **numkit VM loop 151.4** | TreeWalker 336.6. Verdict: the
  transpiled output is **~97× the VM** and **~1.7× faster than MATLAB's
  JIT** — transpile-to-C++ overshoots the JIT target, not just closes
  the gap. The Value-container array tier is **free** (1.56 vs 1.59 raw,
  within noise) — validates §5. (For vectorisable code the builtin
  already wins — filter() 5.85 < MATLAB 7.22 — so codegen's value is the
  scalar-loop / non-vectorisable / fused-custom path.) Bench:
  src/codegen/benchmarks/biquad_codegen_bench.cpp.
- **M1** — inference skeleton: type lattice ✅ → const facet ✅ →
  transfer-function interface + registry + first family (constructors:
  linspace/zeros/ones) + differential validator ✅ → straight-line
  inference driver (TypeEnv + inferExpr/inferStmt over the real AST,
  const propagation, scalar element access; control flow handled
  soundly = Dynamic) ✅ → **next:** CFG + dataflow with join/fixpoint
  for precise control flow; elementwise transfer family; entry-point
  type annotations.
  - elementwise transfer family ✅ — arithmetic (dtype promotion +
    broadcast), comparison/logical (-> logical), unary, real-preserving
    math (sin/cos/exp/…), abs (|complex| -> real). The biquad inner
    expression now types end-to-end to an unboxed scalar double.
  - **control flow ✅ (closes M1's inference skeleton)** — structured
    abstract interpretation over if/elseif/else, switch, for, while
    (MATLAB is structured — no explicit basic-block CFG needed). Env
    join at merges (a var defined on only one path -> Dynamic;
    type-unstable across branches -> Dynamic), fixpoint on loops
    (loop-carried scalars stay precise), for-loop variable from the
    range element (1:N -> scalar). Added RowVector/ColVector shapes.
    The whole biquad loop now types: loop var + carried state scalar
    double, output a double array.
- **M2** — emitter + interop: emit C++ for one fully-typed function
  end-to-end (biquad), call the runtime for the rest, compile, measure.
- **M3** — broaden: transfer-function DB for the hot ~50-100, shape
  inference, complex/more dtypes, more control flow.
- **M4** — product: `numkit build` (AOT a project); optionally an
  emscripten→wasm target. ✅ FIRST CUT: `numkit_codegen` CLI
  (src/codegen/tools) + a testable driver core (driver.{hpp,cpp}:
  `parseTypeSpec` MATLAB-Coder `-args` style + `transpileSource`). Emits the
  C++ TU (self-contained or `--bridge`); `--entry`/`--args`/`-o`. Compilation
  of the emitted TU is the user's toolchain (the e2e tests cover the bridged
  link). Next: package to a shared lib / exe with clean exports.

## 9. Layering

`src/codegen` is an **L2 analysis/codegen pass**, architectural sibling
of `src/scriptgraph`: pure C++ over a parsed AST, core-coupled
(parser/AST), **registers no engine builtin**. Compiled into
`numkit_toolboxes_obj`; tests into `numkit_gtest`.

## 10. Reliability architecture

Correctness of the generated code rests on **two contracts**; if both
hold, the output is correct, and everything else is optimisation.

**Contract 1 — inference soundness (over-approximation).** Every
transfer function returns a type `T` with the true runtime type ⊑ `T`.
Where it cannot prove a precise type over part of its domain, it returns
`Dynamic` (top) there. It NEVER under-approximates (claims a type more
specific than reality). Precision is claimed only with a closure
argument: `real+real` is always real (OK); `real^real` is NOT always
real ((-2)^0.5 is complex) → real only for a provably-integer exponent,
else `Dynamic`. Enforced by the differential validator generating inputs
across the domain **including adversarial corners** (negative / zero /
complex / Inf-NaN / empty) and checking `runtime ⊑ predicted`
(over-approx), not equality (`expectSound`).

**Contract 2 — lowering soundness (precondition ⟹ equivalence).** Every
fast (unboxed) emitter form has a precondition `P` and a lemma
`P ⟹ (fast code ≡ runtime semantics)`. The emitter emits the fast form
only when `P` is established; otherwise it emits the runtime call. MATLAB
indexing semantics are NOT reimplemented in the emitter — the fallback
reuses the engine's `Value::index` / `indexSet`.

**No-kludge litmus:** delete every optimisation analysis (bounds
elision, integer-loop promotion) and the system is still *correct*, just
slower (everything bounds-checked, loop variable a `double`, index via
`(size_t)n - 1`). Optimisations are pure additions whose absence is
always safe — correctness ⊥ optimisation.

**Pipeline:** AST → Inference (lattice + transfers, Contract 1) →
typed AST + facts → Analyses (produce *facts*: index ⊆ extent;
1:N unit-stride index-only — a fact only *enables* an optimisation, its
absence is the safe default) → Emitter (Contract 2: scalar→double,
array→Value+ptr, dynamic→Value; one index module: form by (arg types,
array, facts), no proven form → `nk_rt::index`) → `.cpp` → external
compiler. **Verification gate:** differential VALUE testing
(compile + run + diff vs the interpreter) + fuzz the index grammar; a
debug assert is never the release-correctness mechanism.

**RawBuffer ABI invariants (intentional, documented).** A few call-contract
points are deliberate divergences from MATLAB, consistent with the
performance-first "minimal validation" stance — the same stance under which an
out-of-bounds index throws `std::out_of_range` rather than auto-growing (the
buffer cannot grow):
- **Synthesised names: conforming and collision-free by construction.** Every
  emitter-synthesised block-scope name — the fill-loop counter (`_nk_i`), the
  bridged-call arg temp (`_nk_args`), and the `_len`/`_rows`/`_cols`/`_dN`
  companions of array params, outputs, and runtime-dim N-D locals — carries the
  `_nk_` prefix; the monomorphised function symbol is mangled `__`-free (escaped
  base, `_1` separator, letter-led). Two consequences. (1) **No UB:**
  `__`-anything and `_`+uppercase are reserved for any use ([lex.name]); `_nk_*`
  is `_`+lowercase, reserved only for names in the GLOBAL namespace — and these
  are all block-scope params/locals, so they are conforming; the function symbol
  is letter-led, so conforming at global scope too. (2) **No collision:** a
  MATLAB identifier can never begin with `_`, so a `_nk_*` name can never equal a
  user variable — the companion-vs-user clash is *structurally impossible*, no
  detection needed. The mangle base is escaped (`_`→`_0`) so even a function
  name like `a__b` mangles `__`-free.
- **User variable names with `__` are refused.** A user variable / parameter is
  emitted VERBATIM (synthesised names escape `__`, but user names stay readable
  as-is), and `__` anywhere is reserved ([lex.name]). A leading `_`+uppercase is
  impossible (a MATLAB identifier cannot begin with `_`), so `__` is the only
  reserved shape a user name can take — the emitter scans params + locals at
  function entry and refuses any name containing `__` with a clear message
  (rename it). A single or trailing `_` (`x_`, `a_b`) is legal and untouched.
  Fully accepting `__` names would require escaping every user identifier on
  emission, which would destroy the readable-output property — out of scope.
- **Size args / index conversion are UB-free.** Converting a *negative* `double`
  to `size_t` is UB ([conv.fpint]). Every `double`→`size_t` site guards against
  it: `zeros`/`ones` dims go through `nk_rt::dim()` (throws on negative *before*
  the cast), and every index helper (`index`/`index2`/`nd_off`/…) checks
  `< 1.0` *before* the cast. So a negative dim or index is a clean, deliberate
  `std::out_of_range`, never UB. This matches the **interpreter**, which also
  errors on a negative dim/index (codegen's contract is the interpreter, not
  MATLAB directly). A fractional dim truncates toward zero, as the interpreter
  does. NOTE: both numkit layers diverge from MATLAB here — MATLAB clamps a
  negative dim to 0 (`zeros(-1,3)`→`0×3`) and errors on a fractional dim; numkit
  (interpreter + codegen) errors on negative and truncates fractional. That is
  an *interpreter-level* gap (the interpreter bad-allocs on negative — arguably a
  core bug), tracked separately; codegen faithfully matches the interpreter.
- **Caller pre-sizes outputs.** An array out-param is caller-allocated; the
  caller passes companions matching the buffer it allocated (as for a 1-D
  `y = zeros(1,n)` output, and for const/runtime N-D outputs).

## 10a. Dynamic-tier coverage + sound-refusal catalog (structural frontier complete, 2026-06-26)

The Dynamic tier (§5, §6a; `Dynamic` is Contract 1's top) is the boxed-`nk_val`
escape hatch that lets an un-typeable value flow through compiled code while the
typed tiers stay fast. Its coverage is now **structurally complete** for the common
surface: every construct below either **compiles** (a typed tier, or the Dynamic
tier via the Value-ABI bridge) or is a **documented sound refusal** — detected at
emit time, the enclosing function tiered to the interpreter, **never miscompiled**
(Contract 2's fallback). Several refusals carry an `EXPECT_THROW` regression guard,
so a regression into silent mis-emission fails a test.

**Covered** (compiles natively or via the bridge):

| Construct | Tier / mechanism |
|---|---|
| scalar real / int / bool | unboxed C++ scalar (the 97× win) |
| 1-D / 2-D / N-D real array | `Value` container + hoisted data ptr |
| complex scalar / array | `std::complex<double>` (+ interleaved-re,im C-ABI box) |
| struct: scalar, ABI in/out, struct-arrays (SoA), const dynamic field | field-flatten `_nk_fld_*` + width-union STRUCT lattice |
| cell: literals 1-D+2-D, read `c{i}`/`c{i,j}`, store `c{i}=v`/`c{i,j}=v` (auto-grow), slice `c(i)` | `nk_box_cell[_2d]` / `nk_cell_get[_2d]` / `nk_cell_set[_2d]`; slice rides `index_dyn` |
| named function handle `@f` | `nk_make_handle`; call rides `index_dyn` → handle call |
| capture-free closure `@(x)expr` | free-var analysis + `make_closure` (interpreter eval of the func2str source) |
| formatting `num2str` / `sprintf` | bridged — interpreter owns the printf-style format; CHAR result via `bridge_to_vec_char` |
| growable 1-D local (`x=[]`, `x(end+1)=v`) | owned `std::vector` + `index_set_grow` |
| Dynamic value: binop/unop, condition-truth, indexing, call (typed+Dynamic+bridged), boxed return | `nk_rt::val` + `nk_binop`/`nk_unop`/`nk_truth`/`nk_index`/`call_dyn[v]` + boxed-`nk_val` return |
| multi-output `[a,b]=f(x)` (user fn, all outputs) | reference out-params |

**Sound refusals** (detected → interpreted fallback, never miscompiled):

| Construct | Why refused | Guard |
|---|---|---|
| CSL `c{:}` / `c{vec}` | multi-value (expands to N args/outputs) — not a single `nk_val` | `CellCommaListRefusedUnderBridge` |
| capturing closure `@(x) x+k` (k a local) | a compiled local can't bind through the runtime workspace `nk_eval` uses | `ClosureCapturingLocalRefusedUnderBridge` |
| nested anonymous function | inner param scope not modelled (v1) | (emit refusal) |
| recursion | monomorphiser breaks the recursive call to Dynamic; its boxed result would `call_dyn`-by-name an unresolvable compiled spec | `RecursiveCallRefusedUnderBridge` |
| eval-family (`eval`/`evalin`/`assignin`/…) | code known only at runtime; mutate a workspace by name (§7) | (inference refusal) |
| partial-nargout, object/array multi-output, closed-world polymorphism, 2-D `num2str` | each an under-specified or multi-value shape with no sound single-value lowering | (emit refusal) |

**Remaining codegen work is no longer autonomous-brick-sized.** It is either
(a) **multi-fire design** — CSL variadic expansion, a capture-binding protocol for
capturing closures, multi-output user functions, recursion via a `Bottom`-fixpoint
through the transfer layer — each needing a design decision, not a mechanical
brick; or (b) **additive transfer coverage** for niche builtins (low marginal
value — an un-transferred builtin already bridges or refuses soundly). The
structural frontier itself is closed.

## 11. Build plan

1. **Soundness foundation** ✅ — Contract 1 documented;
   validator strengthened to over-approximation + adversarial domain
   (`expectSound`); transfer audit fixed `power`/`mpower` (real^real →
   real only for integer exponent, else Dynamic).
2. **Index module** — decision ✅ (`IndexPlan` = LinearScalar /
   Subscript2D / Runtime; `planIndexRead`/`planIndexWrite` choose a fast
   form only for a typed buffer indexed by 1–2 scalar non-logical
   positions, with a matching unboxed scalar rhs for writes; everything
   else — N-D 3+ subscripts, logical/range/`end`, deletion, non-typed
   array, dtype-changing write — routes to Runtime). Emission ✅ (brick 3):
   `nk_rt::index`/`index_set` bounds-checked wrappers; LinearScalar read/
   write emitted; bounds checked by default (elision is brick 6).
3. **Emitter core** ✅ — typed AST → self-contained compilable `.cpp`
   (RawBuffer ABI). `emitFunction()`: decl-type prepass (inferStmt threads
   a DeclTypeRecorder so loop-body temporaries are typed at their
   definition site), hoisted scalar locals, scalar/control-flow/builtin/
   index emission. Any unsupported construct throws — never wrong code.
   The whole biquad function emits end-to-end (string-verified).
4. **AOT harness** ✅ — `aot::compileToExecutable` /
   `compileToSharedLibrary` shell out to the compiler captured at
   CMake-configure time (`aot_config.hpp`); MSVC via vcvars64.bat (from
   `CMAKE_GENERATOR_INSTANCE`) through a generated .bat. Degrades to
   Unavailable (caller skips) — toolchainless CI stays green.
5. **End-to-end differential gate** ✅ — transpile biquad.m → C++ →
   compile → RUN → diff the binary's output vs (A) the runtime's filter()
   (1e-9) and (B) the exact DF-I recurrence (1e-12). A second gate proves
   the bounds-checked fallback path runs correctly (the deletable form).
6. **Optimisations** ✅ — clean-index loop promotion (gated on
   numel-equality + clean-index-use + no post-loop use): `for k=1:numel(A)`
   indexing buffers of length numel(A) → 0-based `std::size_t` counter,
   unchecked `A[k]`. Deleting analyzeOptimizations() → the checked form,
   still correct (no-kludge litmus). Both forms e2e-verified.
7. **biquad end-to-end + re-measure** ✅ — `BM_Biquad_Codegen_Generated`
   generates biquad.m → C++ → compiles it `/O2` to a DLL → loads → times
   it. Arrow Lake / desktop-fast Release / N=131072: **generated 1.71
   ns/sample** vs M0 hand-written 1.55 vs VM 151.4 — the transpiler output
   is **~88× the VM** and faster than MATLAB's JIT loop (2.70). The ~10%
   gap to the hand-written M0 is exactly the `y = zeros(1,n)` zero-fill the
   source mandates (an extra streaming write the hand loop omits). The
   emitter's actual output achieves the M0 prize.

## 12. Build plan — interprocedural calls + classes

The monomorphizing interprocedural engine is the **shared prerequisite**:
`f(args)` and `obj.method(args)` both type by specialising a callee body
to its argument types. Build the engine first; classes layer on it. Same
discipline as §11 — each brick string-tested, then e2e (compile+run+diff),
then committed.

**Engine (unlocks multi-function programs; reused by methods):**
1. **Function table + monomorphizing return-type inference** ✅ — user
   functions registered as body-inferring transfers so `inferExpr` routes a
   user call exactly like a builtin; specialised per arg types; recursion →
   Dynamic (sound break). (monomorphize.{hpp,cpp})
2. **Call emission** ✅ — `emitProgram` emits the entry + every
   transitively-called specialisation, mangled by arg types (typeCode),
   fwd-decls + defs; a non-concrete / array / Dynamic interprocedural
   arg/result is refused (v1b scalar).
3. **e2e gate** ✅ — `f` calls `g` compiles, runs, `f(3)=g(3)+1=7`.

**Classes (value-class vertical slice; emitter brick 5 split 5a/5b):**
4. **Lattice `Object(classId)` + class table** ✅ — InferredType.classId +
   object()/isObject(); ClassInfo/ClassRegistry/buildClassInfo/collectClasses;
   §7a refusals loud. (classinfo.{hpp,cpp})
5. **Struct emission + field access** ✅ — emitClassStruct; FIELD_ACCESS
   read/write inference (classes threaded through inferExpr/inferStmt) +
   emission (`.`/`->`); object param (by value / handle wrapper), object
   return (by value), object local hoist. MATLAB-level construction
   deferred — the e2e/harness constructs in C++.
6. **Monomorphic method calls** ✅ — `obj.m(args)` (CALL with FIELD_ACCESS
   callee): registerClassMethods registers "Class::m" transfers; emitter
   emits `Class__m(self, args)` to the mangled specialisation (self by
   value / handle). Result scalar or object (value-in/out).
7. **value-class e2e gate** ✅ — a Rect with an area() method built in C++,
   run() calls obj.area() -> compiled, run, area = w*h = 12. Plus
   field-round-trip e2e (5b).

**Boundary work DONE:** #1 construction (1a default `Point{}` /
handle::make + 1b explicit ctor with obj-seeded output); #2a void/0-output
functions & methods (handle in-place mutators); #2b multi-output
`[a,b]=f()` (void + reference out-params + MULTI_ASSIGN); #3 array & object
interprocedural ARGUMENTS (`ptr,len`); handle-class e2e (getter + void
mutator). collectFunctions no longer leaks methods into the free-function
table.

**Deliberately deferred (low value / would harm cleanliness):**
- recursion precision — a Bottom-start fixpoint to type recursive returns
  instead of the current Dynamic break. Non-trivial; Dynamic is already
  sound (recursive fns are refused, not miscompiled). Low value for hot
  numeric code.
- early refusal of a non-constant property default — currently refused at
  emission (emitClassStruct, where emitScalarExpr lives). Moving it into
  buildClassInfo would invert layering (classinfo -> emitter); not worth it.

**More DONE:**
- 2-D matrix `A(i,j)` READ + local WRITE (column-major, `const T* A, size_t
  A_rows, size_t A_cols`; `index2`/`index2_set`; a 2-D local is an owned vector
  with compile-time dims).
- **Full N-D (rank ≥ 3), bricks N1-N7:** the `NDims` shape lattice; flat
  column-major owned storage + `indexN`/`indexN_set`; `size`/`ndims`/`numel`;
  N-D PARAMS (ptr + one `size_t` companion per dim, read-only) and N-D OUTPUTS
  (caller-allocated out-param + companions); const AND runtime dims
  (`zeros(m,n,p)` with variables → `_nk_<name>_dN` size_t dim vars).
- **`Value`-ABI bridge (§6):** compiled code calls uncompiled builtins/libs
  (sort/fft/…) and passes scalars + 1-D arrays as `Value` via `nk_codegen_rt`
  (scalar + array results, owned-vector locals, e2e + tiered-as-plugin). The
  bridged artifact links one import lib; the non-bridged path stays stdlib-only.
- All synthesised names UB-free + conforming (`_nk_` block-scope prefix, `__`-
  free letter-led mangle; user `__`-names refused); dim/index conversions guard
  against the negative/out-of-range `double`→`size_t` UB.
- **2-D matrix OUTPUT** (caller-allocated out-param, rows/cols companions,
  column-major + index2_set) — completes the array-output story (scalar / 1-D /
  2-D / N-D all return).
- **1-D `size(vec,dim)`** folded via a tracked `VecOrient` (the RawBuffer ABI
  erases row/col; the compile-time type supplies it).
- **Vectorisation: `__restrict` on the output pointer** (A1) — the one aliasing
  fact the C++ compiler needs to auto-vectorise the elementwise/fill loops; the
  output is a distinct caller buffer (ABI precondition). Inputs stay
  un-restricted (read-only aliasing `f(v,v)` safe).
- **`s = size(A)` no-dim** (B1) — a 1 x rank row filled with A's per-axis sizes
  (N-D companion / 2-D rows,cols / 1-D orientation). First array-result-from-a-
  builtin producer beyond zeros/ones; native, self-contained.
- **Complex pipeline (CX1-CX5), end-to-end:** `std::complex<double>` as a first-
  class scalar/array dtype. Scalar accessors (real/imag/angle/conj → std::real/
  imag/arg/conj, real-identity on real input); 1-D + 2-D + N-D complex arrays
  (the buffer ABI is dtype-agnostic — `std::complex<double>*` + the same
  companions); complex elementwise (the inline loops are already type-generic);
  and **complex bridged results** — `y = fft(x)` boxes a real input, runs fft in
  the runtime, and unboxes the complex result into a `std::complex<double>`
  out-param via `nk_box_complex_array`/`bridge_into_cx` (interleaved re,im
  C-ABI). fftTransfer infers 1-arg fft/ifft → COMPLEX shape-preserving;
  multi-arg (padded/dim) forms stay Dynamic so the runtime owns them. 2-D/N-D
  and complex-scalar bridged *args* are still refused (Contract 2 → Dynamic,
  never UB).

**Perf decision — direct `ops::` SIMD-kernel lowering tier: DEFERRED (measured).**
Considered calling raw-buffer `ops::` kernels (e.g. `timesLoop`) instead of the
inline loop. Measurement (A2) found it net-negative *today*: (1) `ops` exposes
no raw-buffer TRANSCENDENTAL kernels (no `sinLoop`/`expLoop` — vectorised
transcendentals live only behind the Value path / VM fusion), so the one case an
ops-call could win has nothing to call directly; (2) for ARITHMETIC the
`__restrict` auto-vectorised loop already matches the SIMD kernel, and a kernel
call adds `HWY_DYNAMIC_DISPATCH` indirect-call overhead (the documented small-N
crater). Revisit only if/when `ops` grows raw-buffer transcendental kernels;
until then the inline loop (small/mid N, self-contained) + Value-bridge (general)
cover it.

**More DONE (this batch):**
- **`[r,c] = size(A)`** — native two-output size (r = axis 0; c folds the
  trailing dims, so `[r,c]=size(rand(2,3,4))` = 2,12). `addMulti("size")` types
  the two scalars; `dimExpr(ArrayInfo,k)` (extracted, shared with the `s=size`
  producer + `size(A,dim)`) supplies each axis. v1 = the two-output idiom; other
  nargout refused (size is variadic, applyMulti is nargout-blind).
- **Transpose `y = x'` / `x.'` (scalar, 1-D, 2-D; real + complex).** Native,
  self-contained: a vector flips orientation (element copy); a matrix swaps dims
  (column-major index swap); ctranspose (`'`) conjugates a complex operand. The
  `transpose`/`ctranspose` transfer infers the shape (row↔col / dims swap /
  scalar; N-D → Dynamic). N-D and in-place (`y=y'`) refused.
- **The `*` operator — COMPLETE for the common shapes, native.** Scalar scaling
  (`s*X` / `X*s`, and `X/s` for `/`) → elementwise fill loop (`collectElementwise`
  accepts `*`/`/` only in their scalar forms). `C = A*B` (both 2-D) → column-major
  triple loop + shared-dim guard. `A*x` (matrix * col → col) and `r*A` (row *
  matrix → row) → double loop + guard. `r*c` (row * col → scalar) → a reduction
  into a scalar. All accumulate complex in `std::complex`. Deferred: outer
  product (`col*row` → matrix), in-place `C=C*B`.
- **2-D elementwise — native.** A 2-D dest with any number of 2-D array operands
  (+ scalars) → flat column-major loop over numel; the soundness guard compares
  BOTH dims (equal numel ≠ equal shape). Covers `scalar*MATRIX`, `A+s`, `A.*B`,
  `sin(A)`, …. Rank discipline: no implicit 1-D↔2-D broadcast; N-D refused.
- **Ops-kernel lowering tier (`ops` owns the kernels; codegen takes them).**
  OPT-IN `OpsKernelOptions` (parallel to `BridgeOptions`): a heavy array op with
  a matching `numkit::ops::` kernel emits that call + links the `nk_ops_kernels`
  SHARED facade (one import lib + DLL, modeled on `nk_codegen_rt`); the inline
  loop stays the self-contained default + the deletable fallback (litmus). The
  public facade is `numkit/ops/kernels.hpp` (`NK_OPS_API`). Routed: **matmul**
  (`matmulDouble` SIMD MulAdd / `matmulComplex` portable), **elementwise binary**
  (`plus/minus/times/rdivideDouble`), **transcendentals** (`sin/cos/tan/atan/
  sinh/cosh/tanh/exp/asinh/expm1Double` → the existing SIMD `fusedTransAffine`).
  Transcendentals are the real win (inline `std::sin` doesn't auto-vectorise on
  MSVC); cheap arithmetic ≈ inline (A3) so it's pure consistency there. **fft is
  NOT an ops kernel** — it's a 1149-line Value/pmr-coupled signal-toolbox
  algorithm (Bluestein, twiddles, dim handling) built on the ops butterflies, so
  it correctly stays on the Value-bridge (which calls the canonical impl; boxing
  amortized over O(n log n)). "No-bridge fft" would need duplicating that into
  `ops` (kludge/drift) or a separate signal-toolbox facade — deferred as a
  user-decision, not auto-done.

**Still later (each its own milestone), simplest first:**
- no-bridge fft (#4): only via a signal-toolbox raw-buffer facade wrapping
  `fftAlongDim` (NOT by duplicating it into `ops`) — modest benefit over the
  working bridge; a user-decision.
- `size(scalar-var)` (the no-dim row + `[r,c]` two-output forms are done).
- merge-gated: `nk_rt::dim` → clamp-negative-to-0 + error (to re-match the fixed
  interpreter once the core `zeros` fix lands and feat/codegen rebases; today it
  throws on negative, matching the OLD interpreter — self-consistent until then).
- N-D native elementwise (2-D done); outer product `col*row` (runtime-dim 2-D
  output).
- interprocedural array RETURN (a compiled callee returning an array — out-param
  threading at the call site).
- multi-output methods + partial-nargout + `~`-ignored outputs (v1: scalar
  outputs, all requested).
- closed-world polymorphism via class-id type-switch (§7a); `ControlBlock` for
  `delete`/`isvalid`.
- **Dynamic tier** ✅ (the generic coverage unlock — un-typeable operands boxed to
  `nk_rt::val`, ops dispatched to the runtime). DONE: value-op C-ABI + `Engine::
  applyBinaryOp`; scalars + 1-D double arrays; arithmetic/comparison/unary;
  un-typeable calls (Dynamic args + boxed result); boxed `nk_val` return + by-value
  Dynamic param (in/out symmetry); `if`/`while` conditions via `truth`; `z(i)`
  indexing (`nk_index` resolves index/call/subsref exactly as `VM::execCallIndirect`);
  1-D/2-D/N-D real + complex-1-D array operands boxed shape-preserving (`nk_box_matrix`
  / `nk_box_array_nd`). Marginal remainder (all SOUND-REFUSED → falls back to the
  interpreter): complex 2-D/N-D Dynamic arrays; multi-subscript Dynamic indexing.
- multi-output bridged calls; object boxing across the bridge; zero-copy array
  views; string/cell/struct pipelines (complex is done, CX1-CX5); 2-D/N-D
  elementwise (native, beyond bridged); recursion precision
  (Bottom-fixpoint; Dynamic is sound today); multi-engine embedding; plugin
  unload (needs core `Engine::unregisterFunction`); WASM plugins.
