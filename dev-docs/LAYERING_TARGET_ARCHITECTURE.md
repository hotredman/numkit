# Layering — Target Architecture (Phase 3-A spec)

Status: **agreed 2026-06-08**, not yet executed. This is the end-state the
layering refactor converges on. Read alongside COORDINATION.md (territories)
and the layering memory note.

---

## 0. The invariant (the one rule)

> **`core` ⊥ everything else. Of the libraries, only `runtime` depends on `core`.**
> Registration is *bundle* plumbing, not a library dependency.

Concretely: the **compute** of `fs / ops / io / math / lang / toolboxes` never
`#include <numkit/core/...>`. `core` (the engine) and the compute libraries are
**mutually independent** — both sit on `value/fs/ops` and don't know about each
other. They meet in exactly two places: `runtime` (the eval/workspace bridge)
and registration (bundle).

This is the second half of the original cycle-break: `core → libs` is already
broken (core is library-agnostic, guard-enforced); this breaks `libs → core`
for all pure compute.

**Corollary — the Engine-free C++ API (the product payoff).** `core-free ⟺
callable from C++ without the interpreter`. So the entire core-free surface —
`value / ops / fs / math / lang / io / toolboxes` — *is* the Engine-free C++ API.
An embedder links it and calls `math::roots`, `signal::fft`, `io::csvread(fs, …)`
without spinning up an Engine. Specifics:
- **io is in the C++ API**, two tiers: (1) pure codecs `*FromString`/`*ToString`
  (Engine-free **and** fs-free — bytes↔Value); (2) file functions
  (`csvread(FsContext&, path, mr)`, …) — Engine-free, over `fs`/VFS.
- **save/load**: the codec (`saveMatToString`/`loadMatFromString`, name↔Value↔bytes)
  is Engine-free C++ API; the bare `save`/`load` bind the workspace (`Environment`
  = core) so they live in `runtime`, not the Engine-free API.
- **callbacks/solvers**: the algorithm (in `ops`/toolbox, `FnHandle`-parameterized)
  is in the C++ API; only the @f→FnHandle adapter needs the Engine.
- **`runtime`** is the *only* library not in the Engine-free C++ API — it *is* the
  engine binding (eval/workspace/registry).
- stateful `fopen`-family: Engine-free iff its file-handle table lives in
  `FsContext` (fs-state), not the Engine.

---

## 1. Target diagram

```
                value / fs / ops            ← L0/L0.5, core-free, shared foundation
               ↙                    ↘
   core (engine: parser/AST/VM/         io · math · lang · toolboxes
        Engine/Environment/CallContext)   ← ALL core-free compute
   ← value/fs/ops only                    ← value/fs/ops only (+ math/lang as a
                                              foundation other toolboxes may use)
               ↘                    ↙
        runtime  (eval/workspace/save-load + str2func/func2str
                  + callback engine-adapters + pausable harnesses)
                  ← the ONLY core-dependent library
               ↓
        bundle   (installStandardLibrary + ALL registration + StandardEngine)
```

Real library targets today: `numkit_value`, `numkit_fs`, `numkit_ops`, and one
monolith `numkit` (= core + everything above). Phase-3-A splits the monolith so
the diagram's edges become **link** edges, not just include-discipline.

---

## 2. Layers

| Layer | Contents | Depends on | core-free? |
|-------|----------|-----------|------------|
| `value` (L0) | Value substrate, FnHandle, scratch | — | ✅ (separate lib) |
| `fs` (L0) | VFS abstraction **+ FsContext** (path/cwd/script-origin resolution) | — | ✅ (separate lib) |
| `ops` (L0.5) | **the kernel home** — raw numerical kernels: binary_ops/reductions/compare/fft/rng/la_solve, AND any new numerical primitive incl. **solver kernels** (RK step, Brent, Nelder-Mead, Levenberg-Marquardt — `FnHandle`-parameterized) carved out of ode/optim | value(/fs) | ✅ (separate lib) |
| `core` (L1) | engine: parser/AST/TreeWalker/bytecode VM/Engine/Environment/CallContext | value/fs/ops | ✅ library-agnostic |
| `math` (L2) | poly/special/interp/geom/trig/exp_log/complex/arithmetic/discrete/permutations. ns `numkit::math` | value/ops/fs | ✅ |
| `lang` (L2) | strings/arrays/types/bitwise/operators/handles/datatypes/cells/structs (data ops). ns `numkit::lang` | value/ops/fs | ✅ |
| `io` (L2) | file_io/text/paths/spreadsheets. ns `numkit::io` | **fs (FsContext)** + value/ops | ✅ (after FsContext) |
| toolboxes (L2) | signal/stats/image/comm/control/audio/wavelet/graphics/linalg/optim/ode | value/ops/fs + math/lang | ✅ |
| `runtime` (L2, core-aware) | eval/evalin/run + who/whos/clear/clearvars/exist/assignin/inputname/import + save/load + str2func/func2str + callback engine-adapters + pausable harnesses | **core** + value/fs/ops + compute libs (for the algorithms it adapts) | ❌ (by design — THE core-dependent library) |
| `bundle` (L3) | installStandardLibrary + **all registration** (per-domain `register/*.cpp`) + StandardEngine | everything | n/a |

`math` + `lang` are foundation toolboxes other toolboxes may depend on (e.g.
control→`math::roots`, stats→`linalg::eig`). The relocated `linalg` keeps
`numkit::linalg` (a second namespace hosted under the foundation tier).

---

## 3. How compute stays core-free — the decoupling abstractions

For every way code currently reaches the Engine, provide an Engine-free
abstraction so the dependency points **down** (value/fs/ops), not up (core):

| Coupling | Engine-free abstraction | Lives in | Status |
|----------|------------------------|----------|--------|
| user callback | `FnHandle` (algorithm takes a FnHandle; the @f→FnHandle adapter is core-aware) | value (L0) | ode/optim ✅; rest TODO |
| VFS / path resolution | `FsContext` (current VFS + cwd + script-origin + search-path; `resolve(userPath)→{VFS*,path}`) | fs (L0) — **moved out of Engine** | TODO |
| file codec | `*FromString` / `*ToString` pure cores | the toolbox itself | csv/extras ✅; rest TODO |
| memory_resource | plain `mr` parameter | — | ✅ everywhere |
| registry reflection (`str2func`/`func2str`) | *none possible* — it IS engine state | → stays in `runtime` | intrinsic |

**Where a decoupled algorithm lands — the 3-way split (esp. solvers).** `ops` is
the **kernel home**: the raw numerical kernel (RK step, Brent, Nelder-Mead,
Levenberg-Marquardt — `FnHandle`-parameterized) goes to `ops`, alongside
`la_solve`/`fft` which are already numerical-algorithm kernels there — core-free.
The Value-level MATLAB-API wrapper (option parsing, output shaping) goes to its
toolbox (`ode`/`optim`/`math`) — core-free, passes the `FnHandle` through. The
engine adapter (@objfun→`FnHandle` + pausable harness) goes to `runtime`. So a
solver is a **3-way split**:

```
ops kernel (core-free, FnHandle) → toolbox Value-API (core-free) → runtime adapter (core-aware)
```

`ops` never depends on core. ode/optim toolboxes become thin once their kernels
move down to `ops`. The same applies to any numerical primitive currently buried
in a toolbox — its kernel belongs in `ops`.

---

## 4. `runtime` inventory (the full list)

Criterion: compute that **intrinsically needs the live Engine** (eval / workspace
/ registry / running a user callback).

**Group 1 — session & eval (whole function; already in core-libs→runtime):**
eval, evalin, run · who, whos, clear, clearvars, exist, assignin, inputname,
import · save, load.

**Group 2 — registry reflection (whole function; moves in):** str2func, func2str.

**Group 3 — callbacks: only the engine-adapter + pausable harness; the
core-free algorithm stays in math/lang/toolbox:**
- iterators: cellfun, arrayfun, structfun, feval, bsxfun
- grouping (the fn-taking ones): splitapply, grouptransform, groupfilter,
  groupsummary(with fn), accumarray  *(findgroups is callback-free → stays in lang/math)*
- solver adapters: ode45, ode23, integral, fzero, fminsearch, nlinfit, bootstrp/bootci

**Borderline, resolved by the rule (pulls core → runtime):**
- `containers.Map` → runtime (engine object system).
- `getenv`/`setenv` → runtime *unless* `core/branding::envGet` is relocated to a
  core-free util, in which case env moves to `lang`.

**NOT runtime:** `version_string` (build metadata, stays at build/bundle);
io file functions (core-free after FsContext); the callback **algorithms**
(core-free, in math/lang/toolbox).

---

## 5. Registration model — all in bundle, per-domain

Compute libraries export **pure functions only** — no `_reg`, no `library.cpp`,
no `install`. All registration lives in **bundle**, split by domain:

```
bundle/src/register/{math,lang,io,signal,stats,image,comm,control,
                     audio,wavelet,graphics,linalg,optim,ode,runtime}.cpp
```

Each `register/<domain>.cpp` `#include`s core + that domain's public headers,
holds the `*_reg` adapters + the domain's `install`. `installStandardLibrary`
calls them in order. Per-domain files keep merge conflicts local to each
session (core/ide/lib edit different files).

**Adapter audit (the real cost):** the `*_reg` adapters currently live
co-located in each module and some use the module's private `*_detail.hpp`
helpers. Before relocating to bundle, classify each:
- **pure** (calls only the public compute fn + Value/CallContext) → relocate
  trivially (e.g. poly's `_reg` is already pure);
- **detail-using** → either promote the helper to the public header, or rewrite
  the adapter to be pure.

---

## 6. Enforcement

1. **Layering guard** (`numkit_layering_check`): extend the existing check so
   `#include <numkit/core/...>` is **forbidden** in `fs/ops/io/math/lang/toolboxes`
   compute (it already forbids upward edges out of value/fs/ops/core). A commit
   that drags core into a compute layer fails the build.
2. **Target split** makes it physical: separate `.lib` per layer, where `core`
   is simply not linked into math/lang/io/toolboxes.

---

## 7. Execution sequence

Each step: build + full suite (baseline 11657 pass / 1 skip / 0 fail) + commit.
Steps A–F are monolith-internal (no link change → safe, build can't break from
re-layering); G is the risky enforcement step.

- **0. DONE:** value/fs/ops separate libs; core↔libs cycle broken; runtime layer
  (currently `core-libs`) holds eval/workspace/save-load; ode/optim FnHandle;
  csv/extras `*FromString`.
- **A. `core-libs → runtime`** rename: dir, ns `numkit::corelibs → numkit::runtime`,
  `NUMKIT_CORELIBS_* → NUMKIT_RUNTIME_*`. Small, mechanical.
- **B. FsContext:** extract path-resolution state+logic from `Engine` into a
  `fs/` type; Engine composes one; migrate io + save/load file-access onto it.
  → io becomes core-free. (core + fs + io territory)
- **C. `builtin → math + lang`** out of `toolboxes/`: move dirs, ns
  `numkit::builtin → numkit::{math,lang}`, retarget all callers. Core-coupled
  builtin TUs (cell/struct/accum/group/integration/containers/env) → `runtime`
  whole (for now). **Invasive — touches every toolbox; coordinate.**
- **D. FnHandle-ize remaining callbacks** (cellfun-family, group, integration,
  solvers): algorithm → math/lang/toolbox (core-free), adapter → runtime.
  Shrinks runtime to the irreducible.
- **E. str2func/func2str + containers.Map → runtime**; decide env (runtime vs
  relocate envGet → lang).
- **F. Registration → bundle:** after the adapter audit, relocate `*_reg` +
  `install` per-domain into `bundle/src/register/`. **Invasive — coordinate.**
- **G. Target split:** separate `.lib` per layer; **WASM/emscripten
  `-fexceptions` migration to the OBJECT libs**; validate ALL 12 presets + WASM.
- **H. Extend the layering guard** (§6.1).

Order is adjustable; A and B are the safest first real steps.

---

## 8. Risks & costs (honest)

- **Repo-wide namespace churn (C):** `builtin::` → `math::`/`lang::` touches
  every toolbox that calls builtin math (control 8 files, signal, audio, comm,
  stats, …). Build is the detector but the diff is large.
- **Adapter relocation (F):** hundreds of `_reg` files → bundle, plus the
  detail-helper audit (§5).
- **WASM `-fexceptions` (G):** THE technical risk — currently PRIVATE on the
  monolith; must migrate to the per-layer OBJECT libs without breaking the
  browser preset.
- **Coordination (COORDINATION.md):** steps C and F edit shared surface that
  the parallel ide/lib sessions also touch. They cannot run concurrently with
  active ide/lib edits — sync the sessions (freeze others / land on main
  together). **Merge to main only on explicit user command.**

---

## 9. Decisions locked (2026-06-08, with the user)

- Names: **`math`**, **`lang`**, **`runtime`** (was `core-libs`).
- `builtin` is dissolved — not a toolbox; → `math` + `lang` + (core-coupled tail) `runtime`.
- All compute is core-free; **registration lives in bundle** (per-domain files).
- Decoupling pattern: `FnHandle` (callbacks) / `FsContext` (VFS) / `*FromString`
  (codecs) / `mr` param. Registry reflection has no core-free form → runtime.
- `runtime` = the single core-dependent library.
- `env`/`containers.Map` classified by the rule "pulls core → runtime".
- **`ops` is the kernel home** (core-free, L0.5): raw numerical kernels live here
  (already `la_solve`/`fft`); solver kernels (RK/Brent/Nelder-Mead/LM) and any
  numerical primitive buried in a toolbox migrate **down into `ops`**, not into a
  toolbox or runtime. Solvers are a 3-way split: `ops` kernel → toolbox Value-API
  → `runtime` adapter. `ops` never depends on core.
