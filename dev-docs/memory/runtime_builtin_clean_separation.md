# Runtime & Builtin Clean Separation and Unified Installer Pattern

## Context & Problem
Previously, several naming and namespace inconsistencies existed:
1. Installer functions lacked a uniform pattern: `BuiltinLibrary::install(engine)` and `LinalgLibrary::install(engine)` were class-static methods, while `runtime::installRuntimeLibrary(engine)` and `installStandardLibrary(engine)` were free functions.
2. `src/runtime/` headers and source files incorrectly declared functions under `namespace nk::builtin` instead of `namespace nk::runtime`.
3. `src/bundle/src/register/builtin/datatypes_reg.cpp` mixed pure numeric `builtin` types with language runtime features (`cell`, `struct`, `containers`, `arrayfun`, OOP reflection, diagnostics, and environment access).

## Decisions & Solution
1. **Unified Library Installer Entry Points (`*Library::install`)**:
   - `RuntimeLibrary::install(Engine &engine)` in `src/runtime/include/nk/runtime/runtime.hpp` and `src/runtime/src/runtime.cpp`.
   - `BuiltinLibrary::install(Engine &engine)` in `src/bundle/include/nk/bundle/builtin_library.hpp`.
   - `StandardLibrary::install(Engine &engine)` in `src/bundle/include/nk/bundle/standard_library.hpp` and `src/bundle/src/standard_library.cpp`.
   - Backward-compatible inline forwarders `installRuntimeLibrary` and `installStandardLibrary` are provided.

2. **Clean `namespace nk::runtime` Across All Runtime Components**:
   - `language/cells/cell.hpp` & `cell.cpp`: `namespace nk::runtime`, `registerCellsRuntime(Engine &engine)`.
   - `language/structures/struct.hpp` & `struct.cpp`: `namespace nk::runtime`, `registerStructuresRuntime(Engine &engine)`.
   - `language/arrays/accum.hpp` & `accum.cpp`: `namespace nk::runtime`, `registerArraysRuntime(Engine &engine)`.
   - `language/commands/env.hpp` & `env.cpp`: `namespace nk::runtime`, `registerEnvRuntime(Engine &engine)`.
   - `programming/errors/diagnostics.hpp` & `diagnostics.cpp`: `namespace nk::runtime`, `registerDiagnosticsRuntime(Engine &engine)`.
   - `containers.hpp` & `containers.cpp`: `namespace nk::runtime` (with `containers::*` and compatibility alias).
   - `language/reflection.cpp`: [NEW] OOP introspection and reflection builtins registered via `registerReflectionRuntime(Engine &engine)`.

3. **Cleaned `datatypes_reg.cpp` in `builtin`**:
   - `datatypes_reg.cpp` now strictly registers only pure numeric/array types, conversions, limits, and array shape predicates.
   - Removed all runtime types (`cell`, `struct`, `arrayfun`, `class`, `isobject`, etc.) and decoupled `datatypes_reg.cpp` from `runtime`.

## Verification
- `python tools/check_layering.py` verified 0 layering violations.
- Built Release and passed unit tests.
