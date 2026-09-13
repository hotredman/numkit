# todo: Reactive DAG Execution & @play Manifest Support for NK Play

*Kind:* feature / architecture · *Status:* open · *Surfaced:* 2026-09-12

> Lifecycle: open -> done. On completion, record the outcome in
> `dev-docs/memory/` (per the AGENTS.md project-memory protocol) and
> delete this file — the todo list holds open work only.

## Goal

Implement in-engine reactive execution (Directed Acyclic Graph / DAG) and UI manifest extraction in the C++ core (`nk::core`) and WebAssembly layer (`src/wasm/`), powering the standalone **NK Play** (`nk-play` / `nkplay`) project and interactive gallery.

## DECISIONS (2026-09-12, user session)

- **Manifest marker is `%{ @play ... %}`** (user decision 2026-09-12 —
  CANONICAL). The `%{#ui` form seen in an early gallery prototype is
  accepted as a legacy ALIAS so existing scripts keep running; drop the
  alias once the gallery is migrated. Engine extracts the block + the
  control `var` names + defaults (that is all the reactive side needs);
  the WEB side parses the full JSON for UI construction.
- **Figures render via the figure WIRE** (the same JSON the IDE
  consumes) — and the Play renderer is PURE SVELTE 5, NO React island
  (user decision 2026-09-12, "no React islands — straight to Svelte").
  The Svelte plot renderer is the SUCCESSOR of the React one: identical
  engine-owned wire JSON (the parity-verified contract), shared
  framework-free JS helpers (plotTicks, colormaps, pure adapters), chart
  types added portion by portion. The React renderer stays in the IDE
  (frozen legacy) until the Svelte one reaches coverage, then the IDE
  flips. `evalDirty()` emits changed figures only (per-figure modified
  flags); `getArrayView` stays as the secondary raw-data lane (with a
  buffer generation counter).
- **Split core API**: `setParam` + `evalDirty` at the core;
  `setParamAndEval` is a thin wasm-level convenience wrapper.
- **Wildcard contract**: statically unresolvable constructs (eval,
  top-level control flow, clear) get Reads=* and dirty the tail;
  provable equivalence property `evalDirty() == full re-run` is the
  acceptance gate for the runtime (portion B).
- FPS budget ladder: JS rAF-coalescing → changed-figure emission →
  (only if benchmarks demand) dataset-level delta emission.
- **Play is the growth line of the NEW IDE** (user decision
  2026-09-12): converge, never rewrite — the React IDE stays the
  stable product (fixes + wire-side features only), new front-end
  work lands in `play/`. Retirement criteria + rationale: the
  Strategy section of
  `dev-docs/handbook/svelte_architecture.md` and
  `dev-docs/memory/play_new_ide_decision.md`. This also supersedes
  the "nk-play independent repository" idea above: Play lives in
  `play/` in THIS repo.

---

## Architectural Separation of Concerns

* **`nk` (This repository — C++ Core & WASM)**:
  - Lexing & AST representation.
  - Manifest extraction from `%{ @play ... %}` block comments.
  - AST variable dependency analysis (`Reads` and `Writes` per statement).
  - Dependency Graph (DAG) construction, topological sort, dirty flag propagation.
  - Workspace caching and partial execution (`evalDirty()`).
  - WebAssembly bindings (`loadScript`, `setParamAndEval`, `getArrayView`).
* **`nk-play` (Independent repository — Web App & Tooling)**:
  - Consumes `nk` as Git submodule (`vendor/nk`) or prebuilt WASM artifact.
  - `gallery/`: 67 atomic interactive scripts across Sonar/Acoustics, Antenna Arrays, Comms, Multipath, DSP, and Control.
  - Interactive parameter controls (Sliders, Log-Sliders, Dropdowns, Toggles, 2D XY-Pads).
  - High-performance 2D & Polar Canvas/WebGL rendering engine (zero-copy from WASM memory).
  - CodeMirror 6 editor with Bret Victor scrubbable number literals.
  - CLI runner `nkplay` (`npx nkplay script.m`).
  - Operational folders: `bugs/` and `notes/`.

---

## Technical Specification: NK Core Implementation

### 1. `@play` Manifest Extractor
* **Files:** `src/core/include/nk/core/play_manifest.hpp`, `src/core/src/play_manifest.cpp`.
* **Behavior:**
  - Scans block comments `%{ ... %}` for the `@play` marker on the first inner line.
  - Extracts the subsequent JSON text.
  - Provides validation against JSON schema or parses into `PlayManifest` C++ struct.
  - Exposed via `Engine::getPlayManifestJson(const std::string &source)`.

### 2. AST Variable Dependency Analyzer
* **Files:** `src/core/include/nk/core/dependency_graph.hpp`, `src/core/src/dependency_graph.cpp`.
* **Behavior:**
  - AST Visitor traversal over `ProgramNode` statements.
  - Collects symbol sets for each statement node:
    - `Writes(Node)`: LHS assignment variables (`x = ...`, `[a, b] = ...`).
    - `Reads(Node)`: identifiers in RHS expressions, function call arguments, index expressions.
  - Builds directed graph: an edge $Node_i \to Node_j$ is added when $Node_i$ writes a variable read by $Node_j$ ($i < j$).

### 3. Reactive DAG Runtime & Workspace Caching
* **Files:** `src/core/include/nk/core/reactive_runtime.hpp`, `src/core/src/reactive_runtime.cpp`.
* **Behavior:**
  - Preserves `Environment` / `Workspace` variable table across evaluation ticks.
  - `setParam(const std::string &varName, const Value &val)`:
    - Directly updates variable in workspace.
    - Marks all DAG nodes downstream of `varName` as `dirty`.
  - `evalDirty()`:
    - Runs topological sort on dirty nodes only.
    - Executes dirty statement nodes in order.
    - Clean/unaffected nodes (e.g. expensive matrix allocations in script preamble) are skipped entirely.

### 4. WebAssembly Export (`src/wasm/`)
* **Methods:**
  - `loadScript(scriptText)` $\to$ initializes reactive session, runs initial pass, returns `@play` JSON.
  - `setParamAndEval(name, value)` $\to$ triggers `evalDirty()` in microsecond budget.
  - `getArrayView(varName)` $\to$ returns direct pointer to `Float64Array` in Emscripten `HEAPF64`.

---

## Detailed Task Plan for the Backend Agent

1. **Step 1: `@play` Manifest Parser & Tests**
   - Implement `play_manifest.hpp` and `play_manifest.cpp`.
   - Add unit tests in `tests/gtest/play_manifest_test.cpp`.
   - Verify that non-manifest scripts cleanly return empty JSON.

2. **Step 2: AST Dependency Analyzer**
   - Implement `VariableUsageVisitor` traversing expressions and collecting symbol sets.
   - Wire tests asserting `Reads` and `Writes` for assignments, calls, matrix indexing.

3. **Step 3: DAG Engine & Invalidation**
   - Implement `DependencyGraph` class with topological sorting and dirty propagation.
   - Wire unit tests asserting that modifying parameter `theta0` executes only dependent beamforming lines, leaving independent lines untouched.

4. **Step 4: Engine Integration & Performance Benchmark**
   - Add `Engine::initReactiveSession` and `Engine::setParamAndEval`.
   - Benchmark re-evaluation time on a 50-element antenna array model (target: $< 0.1\text{ ms}$).

5. **Step 5: WASM Bindings**
   - Expose reactive methods via `EMSCRIPTEN_BINDINGS`.
   - Verify build with `scripts/windows/engine-build.cmd --wasm` (or `scripts/linux/engine-build.sh --wasm`).

---

## Acceptance Criteria

- [ ] Unit tests for `@play` comment extraction pass under `nk_gtest`.
- [ ] AST dependency analyzer correctly tracks reads/writes across scalar, matrix, and multi-output assignments.
- [ ] Reactive runtime evaluates dirty nodes without re-executing clean preamble nodes.
- [ ] WASM exports allow setting scalar values and running partial evaluation in $< 1\text{ ms}$.
- [ ] Zero regressions in existing test suite (`DualEngineTest`).
