# todo: Reactive DAG Execution & @play Manifest Support for NumKit Play

*Kind:* feature / architecture · *Status:* open · *Surfaced:* 2026-09-12

> Lifecycle: open -> done. On completion, record the outcome in
> `dev-docs/memory/` (per the AGENTS.md project-memory protocol) and
> delete this file — the todo list holds open work only.

## Goal

Implement in-engine reactive execution (Directed Acyclic Graph / DAG) and UI manifest extraction (`@play`) in the C++ core (`numkit::core`) and WebAssembly layer (`src/wasm/`). This turns NumKit into a high-performance reactive engine capable of continuous 60–120 FPS re-evaluation on parameter changes, powering the standalone **NumKit Play** (`numkit-play` / `nkplay`) project and interactive gallery.

---

## Architectural Separation of Concerns

* **`numkit` (This repository — C++ Core & WASM)**:
  - Lexing & AST representation.
  - Manifest extraction from `%{ @play ... %}` block comments.
  - AST variable dependency analysis (`Reads` and `Writes` per statement).
  - Dependency Graph (DAG) construction, topological sort, dirty flag propagation.
  - Workspace caching and partial execution (`evalDirty()`).
  - WebAssembly bindings (`loadScript`, `setParamAndEval`, `getArrayView`).
* **`numkit-play` (Independent repository — Web App & Tooling)**:
  - Consumes `numkit` as Git submodule (`vendor/numkit`) or prebuilt WASM artifact.
  - `gallery/`: 67 atomic interactive scripts across Sonar/Acoustics, Antenna Arrays, Comms, Multipath, DSP, and Control.
  - Interactive parameter controls (Sliders, Log-Sliders, Dropdowns, Toggles, 2D XY-Pads).
  - High-performance 2D & Polar Canvas/WebGL rendering engine (zero-copy from WASM memory).
  - CodeMirror 6 editor with Bret Victor scrubbable number literals.
  - CLI runner `nkplay` (`npx nkplay script.m`).
  - Operational folders: `bugs/` and `notes/`.

---

## Technical Specification: NumKit Core Implementation

### 1. `@play` Manifest Extractor
* **Files:** `src/core/include/numkit/core/play_manifest.hpp`, `src/core/src/play_manifest.cpp`.
* **Behavior:**
  - Scans block comments `%{ ... %}` for the `@play` marker on the first inner line.
  - Extracts the subsequent JSON text.
  - Provides validation against JSON schema or parses into `PlayManifest` C++ struct.
  - Exposed via `Engine::getPlayManifestJson(const std::string &source)`.

### 2. AST Variable Dependency Analyzer
* **Files:** `src/core/include/numkit/core/dependency_graph.hpp`, `src/core/src/dependency_graph.cpp`.
* **Behavior:**
  - AST Visitor traversal over `ProgramNode` statements.
  - Collects symbol sets for each statement node:
    - `Writes(Node)`: LHS assignment variables (`x = ...`, `[a, b] = ...`).
    - `Reads(Node)`: identifiers in RHS expressions, function call arguments, index expressions.
  - Builds directed graph: an edge $Node_i \to Node_j$ is added when $Node_i$ writes a variable read by $Node_j$ ($i < j$).

### 3. Reactive DAG Runtime & Workspace Caching
* **Files:** `src/core/include/numkit/core/reactive_runtime.hpp`, `src/core/src/reactive_runtime.cpp`.
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

- [ ] Unit tests for `@play` comment extraction pass under `numkit_gtest`.
- [ ] AST dependency analyzer correctly tracks reads/writes across scalar, matrix, and multi-output assignments.
- [ ] Reactive runtime evaluates dirty nodes without re-executing clean preamble nodes.
- [ ] WASM exports allow setting scalar values and running partial evaluation in $< 1\text{ ms}$.
- [ ] Zero regressions in existing test suite (`DualEngineTest`).
