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

Target deployment: **AOT** — compile a call tree from an entry point
into a standalone native library/binary. There is **no interpreter in
the AOT runtime**. (A tiered / embedded-interpreter mode is an optional
later variant; see §6.)

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
- **M2** — emitter + interop: emit C++ for one fully-typed function
  end-to-end (biquad), call the runtime for the rest, compile, measure.
- **M3** — broaden: transfer-function DB for the hot ~50-100, shape
  inference, complex/more dtypes, more control flow.
- **M4** — product: `numkit build` (AOT a project); optionally an
  emscripten→wasm target.

## 9. Layering

`src/codegen` is an **L2 analysis/codegen pass**, architectural sibling
of `src/scriptgraph`: pure C++ over a parsed AST, core-coupled
(parser/AST), **registers no engine builtin**. Compiled into
`numkit_toolboxes_obj`; tests into `numkit_gtest`.
