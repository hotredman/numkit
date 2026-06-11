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
| `runtime` (L2, core-aware) | eval/evalin/run + who/whos/clear/clearvars/exist/assignin/inputname/import + save/load + str2func/func2str/feval (function-handle/registry ops) | **core** + value/fs/ops (+matio for save) — **NOT** math/lang/io/toolboxes | ❌ (core-dependent, but toolbox-free) |
| `bundle` (L3) | installStandardLibrary + **all registration** (per-domain `register/*.cpp`) + **all core×compute glue** (callback/solver adapters + pausable continuations) + StandardEngine | everything | n/a |

`math` + `lang` are foundation toolboxes other toolboxes may depend on (e.g.
control→`math::roots`, stats→`linalg::eig`). The relocated `linalg` keeps
`numkit::linalg` (a second namespace hosted under the foundation tier).

**Dependency rules (acyclic DAG — no cross-deps).**
- compute (`math/lang/io/toolboxes`) → `value/ops/fs` only (+ `math`/`lang` as the
  foundation other toolboxes may use). **Never `core`, never `runtime`.**
- `core` → `value/ops/fs` only. Never a library.
- `runtime` → `core` + `value/fs/ops` (+matio). **Never a compute lib / toolbox** —
  `runtime` holds engine-operating functions, not toolbox algorithms.
- `bundle` → everything — the only "knows-all" layer; holds all registration AND
  the core×compute glue (callback/solver adapters, pausable continuations).
No sibling-toolbox cross-deps (except the math/lang foundation); no cycles.

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

**Group 2 — function-handle / registry ops (core-only, whole function):**
str2func, func2str, feval. Resolve/invoke via the engine; **no compute-lib dep**.

**Borderline, resolved by "pulls core, no compute-lib dep → runtime":**
- `containers.Map` → runtime (engine object system; standalone, touches no toolbox).
- `getenv`/`setenv` → runtime *unless* `core/branding::envGet` is relocated to a
  core-free util, in which case env moves to `lang`.

### NOT runtime — callback/solver adapters live in `bundle`

cellfun/arrayfun/structfun/bsxfun · splitapply/grouptransform/groupfilter/
groupsummary(fn)/accumarray · ode45/ode23/integral/fzero/fminsearch/nlinfit/
bootstrp — their **adapters** reach into BOTH the compute algorithm
(ops/math/lang/toolbox) AND the engine (callback via `FnHandle` + `LoopContinuation`).
The only layer allowed to depend on both is **bundle** (top) — putting these in
`runtime` would create a forbidden `runtime → toolboxes` cross-dependency. So:
- **algorithm** → `ops` kernel / `math` / `lang` / toolbox (core-free, `FnHandle`);
- **adapter + continuation** → `bundle/register/<domain>` (the core×compute glue).
`findgroups` is callback-free → core-free, in lang/math.

**Also NOT runtime:** `version_string` (build metadata); io file fns (core-free via
FsContext); all callback **algorithms** (core-free).

**Runtime's dependency rule (restated):** `runtime` depends ONLY on
core/fs/value/ops (+matio). If something would force `runtime → a compute lib`,
it is not runtime — it is bundle registration.

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
- **A. ✅ DONE (ddfb1e38)** `core-libs → runtime` rename: dir, ns
  `numkit::corelibs → numkit::runtime`, `NUMKIT_CORELIBS_* → NUMKIT_RUNTIME_*`.
- **B. ✅ DONE (B0/B1/B1c/B2a)** FsContext: B0 branding→fs (41a0dbd7); B1 extracted
  `fs::FsContext` + Engine delegates (f23ee5fc); B2a io VFS-wrappers (csv/extras/
  paths + Engine::fsContext()) take `FsContext&` not `Engine&` (f2a172c6, 5df57ba7,
  f24c8928). **B1c (da51aadd): the fopen-family file-handle table** (struct
  OpenFile + openFiles_/nextFid_/lastFopenError_ + openFile/closeFile/
  closeAllFiles/findFile/openFileIds) **moved out of Engine onto FsContext** —
  STL-only, reuses FsContext::resolvePath + VirtualFS r/w; Engine keeps inline
  forwarders + `using OpenFile = FsContext::OpenFile` so consumers compile
  unchanged. **All VFS / file-handle / cwd / script-origin STATE now lives in
  FsContext** (Engine retains only the legitimately-Engine `mPath_` m-file
  search path); the line-39 criterion ("stateful fopen-family Engine-free iff
  its handle table lives in FsContext") is now MET. `type`/`fileio` builtin
  *signatures*: B2b (io TUs truly core-free = file builtins take `FsContext&`,
  move in-TU `_reg` adapters to bundle — the integral pattern). **✅ F-for-io
  DONE — every io *compute* TU is now core-free:**
  - `fileio.cpp` (4115b8fe): 11 file builtins take `FsContext& + mr`,
    `NK_FILEIO_REG` adapters → `bundle/src/register/io/fileio_reg.cpp`.
  - `csv.cpp` + `paths.cpp` (d8908431): in-TU `_reg` detail blocks →
    `bundle/src/register/io/{csv,paths}_reg.cpp`.
  - `extras.cpp` (948e6240): the five VFS readers stay (core-free), their `_reg`
    → `bundle/src/register/io/extras_reg.cpp`; `type` (needs `engine.outputText`)
    split into its own TU `toolboxes/io/src/text/type.cpp`; the shared
    `slurpFile` promoted to `numkit::io` + declared in extras.hpp.

  `IoLibrary::install` was **unchanged throughout** — it already forward-declares
  + registers `io::detail::*_reg` by name, and the definitions now resolve to the
  bundle TUs (or type.cpp) at link, with no toolbox→bundle include. **Only core
  includes left in io/src:** the `library.cpp` installer (registration ABI) +
  `type.cpp` — both legitimately Engine-coupled. The io toolbox compute is fully
  Engine-free / reusable via the core-free C++ API + FsContext.
- **C. `builtin → math + lang`** out of `toolboxes/` (decision LOCKED 2026-06-09:
  full rename, ns `numkit::builtin → numkit::{math,lang}`, headers
  `<numkit/builtin/...>` → `<numkit/{math,lang}/...>`). **Per-TU map** (mirrors the
  existing `builtin/src/{language,math}/` sub-trees): `math/` subtree
  (arithmetic·complex·discrete·exp_log·geom·interp·permutations·poly·random·special·
  trig) → **`math`**; `language/` subtree (strings·arrays-manip·types·bitwise·
  operators) → **`lang`**; `env` → **`lang`** (core-free since B0). Core-coupled tail
  (cell·struct·accum·group·integration·containers registration) → **`runtime`** whole.
  containers `containers::` compute → `lang`. **SAFE INCREMENTAL STRATEGY** — callers
  reference `numkit::builtin::` 185× across 78 files and include the umbrella
  `<numkit/builtin/library.hpp>` (NOT specific headers): (1) keep `library.hpp` as a
  FORWARDING umbrella that re-includes the relocated headers; (2) add transitional
  `namespace numkit::builtin { using namespace math; using namespace lang; }` shim so
  ALL 185 call-sites compile UNCHANGED during the move; (3) migrate call-sites
  `builtin::`→`math::`/`lang::` in a later mechanical pass; (4) drop the shim. **Phases:**
  C1 create `math` lib (git-mv math/ .cpp+headers, ns→`numkit::math`, shims, CMake, green)
  · C2 `lang` lib (same) · C3 core-coupled tail → `runtime` · C4 migrate the 185
  call-sites + drop shims · C5 registration → bundle (overlaps **F**). Each phase = its
  own green commit. **Invasive — touches every toolbox; coordinate. Multi-session.**

  **STATUS (2026-06-10).** **C0 ✅** — shared engine-free helpers (`helpers` /
  `rows_helpers` / `poly_helpers`) → `ops`, re-export shims left at the old
  `builtin/src` paths. **C1 ✅** — all **11 math areas** relocated into `math/`
  (geom, complex, discrete, interp, poly, permutations, random, trig, exp_log,
  special, arithmetic). DEVIATION chosen for trivially-green per-area commits:
  this was a **relocate-only** pass — files `git-mv`'d into `math/{src,include}`,
  but the **namespace stays `numkit::builtin`** and forwarding stubs sit at every
  old header path; the `numkit::builtin → numkit::math` rename + stub removal is
  folded into **C4** (one mechanical pass over the relocated dirs). Key enablers:
  (a) `toolboxes/builtin/src` is already on the `numkit` PRIVATE -I, so relocated
  files' relative helper includes still resolve through the shims; (b) **SIMD
  prerequisite** (commit `fa5ae9f9`): `math/src` added to that PRIVATE -I (Highway
  `HWY_TARGET_INCLUDE` root) + the math-level shared helper `_unary_hint.hpp` moved
  to `math/src/` (stub in builtin/src); each relocated `*_highway.cpp` had its
  `HWY_TARGET_INCLUDE` de-prefixed (`"math/<area>/…"` → `"<area>/…"`); (c)
  arithmetic additionally needed forwarding stubs at
  `builtin/src/math/arithmetic/{cumsum,var_reduction}.hpp` for cross-toolbox
  consumers (builtin matrix, stats descriptive). `group/` + `integration/` stay in
  builtin (core-coupled → C3). Commits: geom `1fd555e5` … arithmetic `b8f9a126`.
  **C2 ✅** — the `lang` library now holds all five core-free language areas:
  bitwise (`3c95afab`), strings (`becd493d`), operators (`a2c6c709`), arrays
  {matrix/manip/nd_manip} (`b96a7e40`), types {+casts SIMD} (`3f5da7b2`). Same
  relocate-only recipe; the casts SIMD backend needed the same prerequisite as
  math (lang/src on the numkit PRIVATE -I + casts_highway HWY_TARGET_INCLUDE
  de-prefix), mirroring trig. Core-COUPLED language areas stay in builtin →
  **C3**: cells/cell, structures/struct, datatypes/containers, arrays/accum, and
  commands/env (env's compute pulls core/engine, so it is NOT core-free despite
  the §E note). **Next: C3** (core-coupled tail → runtime), then C4 (the single
  ns-rename numkit::builtin → numkit::math / numkit::lang over all relocated dirs
  + include-path cleanup + drop ALL forwarding stubs/shims), C5 (registration →
  bundle = F), then D / F / G / H.

  **C3 ✅** — the runtime layer now holds all 7 core-coupled areas relocated from
  builtin: commands/env (`d3bde36f`), arrays/accum (`da95cbae`), datatypes/
  containers + math/group + math/integration (`a1021996`; the math-level
  `_callback_helpers.hpp` moved alongside integration), cells/cell +
  structures/struct (`820784ef`; the vm/callback_builtin pair). All clean relocates
  — library.cpp keeps registering their `*_reg` / `register*` symbols, which compile
  into the single numkit target; `_handlefn_helpers.hpp` (cell/struct-only) stays in
  builtin/src, reachable via the global -I. builtin/ now holds only library.cpp +
  programming/diagnostics + version_string + the `_reg` adapters + the forwarding
  stubs/shims. **Next: C4** — the single mechanical ns-rename (numkit::builtin →
  numkit::math / numkit::lang over the relocated math/ + lang/ dirs) + ~185
  call-site retarget + include-path cleanup + drop ALL stubs/shims. Open question
  for C4: whether the runtime-relocated areas keep ns numkit::builtin or move to a
  runtime ns — decide (or surface) when C4 starts.

  **C4 ✅ COMPLETE (2026-06-10, 16/16 areas, HEAD `3dc4225d`, branch
  refactor/layering, +139 vs main).** All relocated compute renamed
  numkit::builtin → **numkit::math** (11: geom `77738e34`, complex+permutations
  `abf50916`, interp+random `f1f3d27b`, discrete `82e365e2`, poly `df06d976`,
  SIMD cluster trig/exp_log/special/arithmetic `d63cbcc2`) / **numkit::lang**
  (5: bitwise+operators `80a27b37`, arrays `bc3881cf`, strings `b032a94a`, types
  `3dc4225d`). Verified: no `^namespace numkit::builtin {` remains in math/src or
  lang/src compute — only the `*_reg` adapters keep numkit::builtin, by design.
  desktop-fast 11657 pass / 1 skip / 0 fail throughout; layering guard green at
  every commit.
  **Deviations from the original C4 plan (kept the relocate-only spirit):** stubs
  and the ~185 call-site retarget were NOT dropped — instead builtin-side
  reachability is preserved by a 2-shim mechanism: (1) an umbrella shim in
  library.hpp `namespace numkit::builtin { using namespace numkit::math; using
  namespace numkit::lang; }`, and (2) a per-stub `using namespace numkit::math|lang`
  in each forwarding stub. `_reg` adapters stay numkit::builtin (registration via
  the monolith). **Two area classes emerged:** (A) self-contained areas renamed by
  a plain robust-sed; (B) areas whose compute reached C0 ops-helpers via `detail::`
  (collectRowsByIndex / rowLexCmp* / resolveDim / applyAlongDim / outShapeForDim* /
  firstNonSingletonDim / polyRoots* / polyExpand* / Complex) — those refs were
  retargeted `detail::X → numkit::ops::X` before renaming. The SIMD-math areas
  (trig/exp_log/special/arithmetic) are interdependent (shared `_unary_hint.hpp`
  3-arg hint overloads; trig→arithmetic hypot; arrays→arithmetic cumsumScan) so
  they renamed together as one keystone commit. Forced **cross-consumer fixups**
  (symbol moved out of numkit::builtin): stats descriptive `using
  ::numkit::builtin::varianceTwoPass` → `::numkit::math::`; lang/types isnan/isinf/
  isfinite backend fwd-declared in numkit::math::detail; lang/arrays anyOf/allOf
  (defined in arithmetic) declared numkit::math in matrix.hpp; io_helpers (shared
  with the io toolbox) stays numkit::builtin::detail so scan qualifies its refs.
  **C4c ✅ DONE (2026-06-10, HEAD `ca9c86ae`, +143).** All shims removed, all
  call-sites qualified. Step 1 (`2a922f36`): dropped the library.hpp umbrella shim;
  replaced its broad effect with localized `using namespace numkit::math|lang;` in the
  25 _reg adapters that call their area's relocated compute. Step 2a (`25dc0466`):
  bulk-qualified ~584 `builtin::X` / `numkit::builtin::X` cross-toolbox refs to
  numkit::math::X / numkit::lang::X via a fn→ns map built from the public headers (60
  math + 103 lang fns; sed with optional-numkit::-prefix handling to avoid double
  qualification); 20 genuinely-builtin names left untouched. Step 2b (`ca9c86ae`):
  removed all 24 per-stub re-export shims; fixed the unqualified-via-shim stragglers the
  bulk pass couldn't see — sharedEngine/rngMutex (numkit::ops, re-exported via
  numkit::math) requalified, and localized using-directives added where a math/builtin/
  runtime TU calls a lang fn (reductions_reg/polynomials_reg reshape/poly_of_matrix,
  programming/diagnostics formatOnce, runtime cells/struct). Forwarding stubs remain as
  pure #include forwards (dropped wholesale in the later include-path migration). 11657
  pass / 1 skip / 0 fail and layering guard green at every step. **H ✅ DONE**
  (`d3d25e65`) — the layering guard now pins math/lang compute too. **C5 ✅ DONE**
  (999117bd→5a5b5949) — all 223 `*_reg.cpp` relocated to bundle/src/register/. **G ✅ DONE**
  (5dc385ca + c4f9e1f5) — monolith split into per-layer OBJECT libs, 12/12 buildable presets
  lib-green (incl. WASM). **C6 ✅ DONE** (C6a `6f3080ca` → C6d-6 `91d634ee`,
  +169 vs main) — `toolboxes/builtin` **fully dissolved**: pure-forward +
  re-export stub headers retired (consumers retargeted to
  `<numkit/ops|math|lang/...>`, re-exported dim-infra requalified to
  `numkit::ops::`); the `BuiltinLibrary` umbrella relocated to bundle
  (`builtin_library.{hpp,cpp}`, ~280 includers swept); engine-coupled string
  I/O (`disp`/`fprintf`/`fscanf`/`textscan`) + `_handlefn_helpers` +
  `programming/errors/diagnostics` → `runtime`; `io_helpers` → `ops`
  (`numkit::ops`); `version_string` → bundle; tests/benchmarks re-homed under
  `tests/builtin` + `benchmarks/builtin`; the `builtin/src` shared-helper `-I`
  bridge removed. `numkit::builtin` now survives ONLY as the namespace on the
  bundle `*_reg` adapters + the runtime-relocated cell/struct/diagnostics/
  handlefn TUs (the cell/struct precedent). **The Phase-3-A layering refactor
  (C0…C6 + H + G) is COMPLETE.**

- **D. Solver split — `ode`/`optim`/`nlinfit`/`integral` ✅ ALL DONE.** Pattern: the Engine-free FnHandle kernel stays
  in the toolbox (now core-free); the Engine adapter (`@handle`→`FnHandle`
  bridge) + embedded-.m pausable wrapper + (where the whole toolbox is solvers)
  the `install` relocate to `bundle/src/register/<tb>/`.
  - ode (ode45/ode23, `80051215`) + optim (fzero/fminbnd/fminsearch, `49329af7`)
    — kernels were already FnHandle-parameterized → pure relocation.
  - nlinfit (stats) — kernel took `Engine *`; FnHandle-decoupled it (`8f9831f4`,
    LM/CI behaviour validated by NlinfitTest) then moved its adapter to
    bundle/register/stats/regress (`353372a2`). nlinfit kernel now core-free.
  - integral (`4559df16`) — its Gauss-Kronrod kernel was already FnHandle-
    decoupled, but integration.cpp was a 1001-line mixed TU in runtime whose
    `*_reg` adapters reached into file-local anon helpers (dispatch logic, not
    thin wrappers), so a first carve was reverted. Done cleanly on the retry:
    the calculus (gradient/del2/cumtrapz/trapz) + integral kernel + the FnHandle
    callback wrappers moved to `math/` (numkit::math, core-free); the lower-level
    dispatch helpers the adapters need (gradientND / toDoubleCopy /
    cumtrapzMatrix{Rows,Cols}±C / trapzImpl) + the GK node tables were lifted
    from the anon namespace into `numkit::math::detail` via a new
    `integration_detail.hpp` (cumtrapzDim joined the public header); the Engine
    adapters + the embedded-.m wrapper + registerIntegralM went to
    `bundle/register/math`, keeping numkit::builtin::detail (zero churn in
    builtin_library.cpp) and pulling the compute in via explicit using-decls.
    **runtime lost integration.cpp + _callback_helpers.hpp entirely** — the
    first concrete shrink of the runtime layer that D set out to achieve.
- **E.** str2func/func2str → runtime ✅ DONE (1338c31b — runtime/function_handles.cpp;
  feval stays in builtin until F because of its FevalCallbackBuiltin adapter).
  containers.Map: `containers::` compute → `lang`, registry hooks → bundle (lands
  with C3/F, not a plain runtime move). env → `lang` (core-free since B0; lands with C).
- **F. ✅ DONE (= C5, 2026-06-10, 999117bd→5a5b5949, +152 vs main).** All 223
  `*_reg.cpp` registration adapters relocated from the compute trees (math/lang/
  toolboxes) into `bundle/src/register/<domain>/<subarea>/` (subarea preserved to
  avoid basename collisions). bundle/CMakeLists GLOB_RECURSEs src/register/*.cpp
  (CONFIGURE_DEPENDS); each domain's compute CMakeLists drops its _reg lines; the 6
  not-previously-on-`-I` toolboxes (image/comm/control/audio/wavelet/linalg) got
  their src root added to the global -I so the relocated adapters' area-own
  ("<subarea>/X_detail.hpp") includes resolve. Zero _reg remain under math/lang/
  toolboxes — compute trees export pure functions only. library.cpp registration
  hub stays in builtin (forward-decls + registers the relocated symbols; still one
  monolith target — the target split is G). 11657 pass / 1 skip / 0 fail + layering
  guard green at every per-domain commit. `install`/library.cpp move to bundle is
  deferred to the target-split (G), where it matters.
- **G. ✅ DONE (2026-06-10, 5dc385ca + c4f9e1f5).** The `numkit` monolith is split
  into per-layer OBJECT libraries — numkit_{core,toolboxes,math,lang,runtime,bundle}_obj
  — that the final `numkit` aggregates via `$<TARGET_OBJECTS>` (identical object set,
  grouped). A shared `numkit_configure_layer()` helper applies the monolith compile
  config to each OBJECT lib (include roots + SIMD/threads/matio defs + usage-requirement
  links + the EMSCRIPTEN `-fexceptions` per OBJECT — the WASM exception-ABI migration).
  Fixed one pre-existing layering inversion surfaced by validation: ops/src/fft_portable.cpp
  reached up into signal's dsp_helpers.hpp for the scalar fftRadix2 butterfly (also pulling
  core) — moved the butterfly to ops/include/numkit/ops/fft_radix2.hpp (guard-invisible
  because it was a quoted include). **Validated lib-green on all 12 buildable presets:**
  desktop-fast (11657) · portable (11651) · desktop-fast-threads (11666) · bench ·
  bench-simd · bench-simd-threads · bench-clang · bench-simd-clang (desktop, `--target
  numkit`); browser · browser-threads · bench-wasm · bench-wasm-threads (WASM OBJECT split
  + `-fexceptions` confirmed). **apple-m** not buildable on the x86 host — validate on Apple
  hardware separately. **Known pre-existing follow-up (NOT G):** the bench `*_bench.cpp`
  executables call several post-C4-renamed functions with stale signatures/namespaces
  (`unique(mr,…)`, `ops::plusLoop`, `mtimes(mr,…)`, …); they only build on bench* presets
  and need an API-update pass — orthogonal to the split (the library is green everywhere).
- **H. ✅ DONE (2026-06-10, `d3d25e65`; toolbox pin `97932262`).** Extended
  check_layering.py to pin the math/ and lang/ compute layers: they must not
  include core/runtime/toolbox headers (the `*_reg.cpp` glue is exempt; `builtin`
  transitionally allowed). Locks in the C4 split. **Toolbox pin (97932262):** now
  also pins every `toolboxes/*` compute TU core/runtime-free — allowed = value/fs/
  ops/math/lang/builtin + sibling toolboxes; exempt = each `library.{cpp,hpp}`
  installer + `io/src/text/type.cpp` (engine.outputText) + the whole `graph`
  toolbox (AST/lowering infra). Locks in this session's "all toolbox compute
  Engine-free" result. Passes clean; negative-tested. (§6.1)

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
- `runtime` = the single core-dependent library, and it is **toolbox-free**:
  `runtime` depends ONLY on core/fs/value/ops, NEVER on a compute lib. Anything
  that would force `runtime → toolbox` (callback/solver adapters) is registration
  → `bundle`. No `runtime → toolboxes` cross-dependency.
- `env`/`containers.Map` classified by the rule "pulls core (no compute-lib dep) → runtime".
- **`ops` is the kernel home** (core-free, L0.5): raw numerical kernels live here
  (already `la_solve`/`fft`); solver kernels (RK/Brent/Nelder-Mead/LM) and any
  numerical primitive buried in a toolbox migrate **down into `ops`**, not into a
  toolbox or runtime. Solvers are a 3-way split: `ops` kernel → toolbox Value-API
  → `runtime` adapter. `ops` never depends on core.

---

## 10. Post-merge follow-ups (after `refactor/layering` lands on main)

Orthogonal to the A–H phases (all done); deferred out of the +188 layering branch
to keep its diff semantic + reviewable.

- **`src/` root restructure (DECIDED 2026-06-11, with the user).** The repo root
  mixes the 9 layer libraries (value / fs / ops / core / math / lang / runtime /
  toolboxes / bundle) with auxiliary content (brand / bugs / cmake / dev-docs /
  docs / examples / ide / scripts / third_party / tools). Move the **9 libs into
  `src/`** (Model 1, chosen over `cpp/`-holds-all-C++); `tests/ benchmarks/
  wasm/ ide/` + aux stay at root. **Pure mechanical `git mv`** — every
  `#include <numkit/...>` is logical (resolved through a `-I`), so NO source
  edits; only path refs change: root `CMakeLists.txt` (9× `add_subdirectory` +
  the ~6 absolute `${CMAKE_CURRENT_SOURCE_DIR}/<layer>/src` include roots +
  `bundle/src/version_string.cpp`), `tools/check_layering.py` LAYER_DIRS (+ the
  toolbox scan path + the `io/src/text/type.cpp` exempt path), `Doxyfile` INPUT,
  `scripts/build.sh|deploy.sh`, `.gitignore`. Per-layer CMakeLists are unchanged
  (all relative to `CMAKE_CURRENT_SOURCE_DIR`). **Its own coordinated branch** —
  9 top-level `git mv`s are a huge shared surface; freeze the ide/lib worktrees
  and land together (COORDINATION.md). Build is the detector.
- **bench API-rot** — `benchmarks/**/*_bench.cpp` call post-C4-renamed functions
  with stale signatures (`unique(mr,…)`, `ops::plusLoop`, `mtimes(mr,…)`); they
  build only on bench* presets (library is green everywhere). Needs an
  API-update pass. Orthogonal.
- **apple-m preset** — not buildable on the x86 host; validate on Apple hardware.
- Housekeeping: drop the stray root `tmp_split_sig.sh` + untracked `_scratch_cls/`.
