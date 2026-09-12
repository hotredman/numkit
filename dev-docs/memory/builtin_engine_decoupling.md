# Builtin Library Engine Decoupling

## Problem & Context
During architectural audit of `nk::builtin`, it was discovered that while compute algorithms in `src/builtin/src/` were pure and Engine-free, `src/builtin/include/nk/builtin/iofun.hpp` and `builtin.hpp` contained forward declarations of `class Engine;` and declarations for Engine-coupled I/O (`fprintf(Engine&)`, `disp(Engine&)`, `fscanf(Engine&)`, `textscan(Engine&)`), and `src/builtin/src/builtin.cpp` implemented `BuiltinLibrary::install(Engine&)`.

Because `nk::builtin` represents the pure standard mathematical and data compute library, having `Engine` dependencies in its public headers and compute source tree violated the layering principle that compute libraries must stay completely Engine-free.

## Chosen Solution & Architecture
1. **Runtime I/O Header (`src/runtime/include/nk/runtime/io.hpp`)**:
   - Created `nk/runtime/io.hpp` under `namespace nk::runtime` declaring `disp(Engine&, ...)`, `fprintf(Engine&, ...)`, `fscanf(Engine&, ...)`, and `textscan(Engine&, ...)`.
   - Updated `print_io.cpp` and `scan_io.cpp` in `src/runtime/src/language/strings/` to implement these functions within `namespace nk::runtime`.
   - Updated registration adapters `lang_print_reg.cpp` and `lang_scan_reg.cpp` in `src/bundle/src/register/builtin/` to include `<nk/runtime/io.hpp>` and call `::nk::runtime::*`.

2. **Pure `builtin/iofun.hpp` and `builtin/builtin.hpp`**:
   - Removed `class Engine;` forward declaration and `Engine&` function signatures from `src/builtin/include/nk/builtin/iofun.hpp`.
   - `iofun.hpp` now exclusively exposes pure, engine-free I/O routines: `sprintf(...)`, `sscanf(...)`, `disp(const Value&, std::ostream&)`, `fprintf(std::ostream&, ...)`.
   - Updated `src/builtin/include/nk/builtin/builtin.hpp` to act as the pure master umbrella header including all domain headers (`ops.hpp`, `elfun.hpp`, `elmat.hpp`, `matfun.hpp`, `datafun.hpp`, `specfun.hpp`, `polyfun.hpp`, `strfun.hpp`, `timefun.hpp`, `datatypes.hpp`, `iofun.hpp`, `general.hpp`, `lang.hpp`, `scan_core.hpp`).

3. **Bundle Registration (`src/bundle/src/register/builtin/builtin_library.cpp`)**:
   - Declared `class BuiltinLibrary` in `src/bundle/include/nk/bundle/builtin_library.hpp`.
   - Moved `BuiltinLibrary::install(Engine &engine)` implementation into `src/bundle/src/register/builtin/builtin_library.cpp` (matching the architecture of all other toolbox libraries).
   - Removed `src/builtin/src/builtin.cpp` and updated `src/builtin/CMakeLists.txt`.

## Layering Verification
- `python tools/check_layering.py` passes cleanly.
- `nk::builtin` has 0 includes of `<nk/core/...>` or `<nk/core/engine.hpp>`.
