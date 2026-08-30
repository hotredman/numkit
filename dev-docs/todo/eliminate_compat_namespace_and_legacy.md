# todo: Eliminate compat.* namespace, --compat flag, and associated legacy boilerplate

*Kind:* refactor / tech-debt · *Status:* open · *Surfaced:* 2026-08-30

> Lifecycle: open -> done. On completion, record the outcome in
> `dev-docs/memory/` (per the AGENTS.md project-memory protocol) and
> delete this file — the todo list holds open work only.

## Goal
Following the activation of the **Bare-Name Resolver** (`dev-docs/todo/bare_name_resolver.md`), completely eliminate the artificial `compat.*` namespace, remove all ~655 duplicate `engine.registerFunction("compat", ...)` registrations, deprecate/remove the `--compat` CLI flag, and clean up leftover `import compat.*;` boilerplate across tests, scripts, and documentation.

## Problem & Background
Before the bare-name resolver, toolbox functions (`signal.butter`, `stats.mean`, `linalg.qr`, etc.) were unreachable without an explicit namespace prefix. To allow MATLAB-like usage, the engine duplicated every toolbox function into a flat namespace named `compat` and required scripts to execute `import compat.*;` or run with the `--compat` flag.

With the bare-name resolver enabled, the engine automatically resolves unqualified names across all registered toolboxes. The `compat` namespace is now 100% obsolete dead code that wastes memory and adds confusion.

## Inventory of Legacy to Remove

### 1. Dual-Registration in Toolbox Registration Hubs
Remove the redundant `engine.registerFunction("compat", name, fn);` line from the `reg()` lambda across all registration files:
- `src/bundle/src/register/signal/signal_library.cpp`
- `src/bundle/src/register/stats/stats_library.cpp`
- `src/bundle/src/register/linalg/linalg_library.cpp`
- `src/bundle/src/register/io/io_library.cpp`
- `src/bundle/src/register/audio/audio_library.cpp`
- `src/bundle/src/register/image/image_library.cpp`
- `src/bundle/src/register/control/control_library.cpp`
- `src/bundle/src/register/wavelet/wavelet_library.cpp`
- `src/bundle/src/register/optim/optim_library.cpp`
- `src/bundle/src/register/geometry/geometry_library.cpp`

### 2. CLI and REPL Legacy
- **`apps/numkit/main.cpp` & `apps/numkit/numkit_repl.cpp`**:
  - Remove `--compat` CLI flag and `addImplicitImport({{"compat"}, true, ""})`.
  - Print deprecation note if `--compat` is passed or remove silently.
- **WASM Bridge (`wasm/src/repl_bindings.cpp`)**:
  - Clean up `repl_set_compat_mode()` (make it a no-op or remove).

### 3. Test Suites & Fixtures (`tests/gtest/` & `src/**/tests/`)
- Remove `void SetUp() override { engine.eval("import compat.*;"); }` boilerplate from Google Test fixtures where bare functions now resolve directly.

### 4. Corpus Examples & Smoke Scripts
- Remove `import compat.*;` from:
  - `examples/**/*.m`
  - `src/**/tests/smoke/*_smoke.m`

### 5. Documentation & Guidelines
- Update `AGENTS.md` and `README.md` (remove instructions requiring `import compat.*` or `--compat`).
- Update `bugs/README.md` and `dev-docs/handbook/` rules.

## Acceptance Criteria
- [ ] No `engine.registerFunction("compat", ...)` remains anywhere in the C++ codebase.
- [ ] Memory footprint reduced (elimination of ~655 duplicate function entry pointers).
- [ ] `numkit_repl` and `npx numkit` execute all toolbox functions bare without `--compat`.
- [ ] All 13,153+ gtest unit tests and 710 smoke tests pass cleanly with zero `import compat.*;` calls.
- [ ] Documentation updated to reflect true zero-boilerplate MATLAB parity.
