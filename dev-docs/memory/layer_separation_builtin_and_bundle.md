# Memory: Complete Layer Separation between Compute (L2 `builtin`) and Registration (L3 `bundle`)

## Architectural Decision
We establish a strict boundary between the pure C++ standard library compute layer (`src/builtin/`, L2) and the engine registration / binding layer (`src/bundle/src/register/`, L3).

### 1. Layer Responsibilities
- **`src/builtin/` (L2 Compute Layer)**:
  - Contains 100% pure, engine-free C++ compute implementations under `namespace numkit::builtin`.
  - Headers in `src/builtin/include/numkit/builtin/*.hpp`.
  - Compute kernels in `src/builtin/src/<category>/...`.
  - Dependencies: L0 (`value`), L0.5 (`ops`), STL (`<memory_resource>`, `<complex>`, etc.).
  - Never includes `<numkit/core/engine.hpp>`, never references `CallContext`, never declares `register_<category>(Engine&)`.
  - Can be embedded directly into any C++ project without spinning up or linking the scripting runtime/engine.

- **`src/bundle/src/register/` (L3 Registration Layer)**:
  - Contains all engine adapters and registration glue:
    - `src/bundle/src/register/builtin/<category>_reg.cpp` for standard library builtins.
    - `src/bundle/src/register/<toolbox>/...` for toolboxes (`signal`, `image`, `stats`, etc.).
  - Adapts scripting engine `CallContext`, parses/validates `nargin`/`nargout`, forwards to `numkit::builtin::*` / `numkit::<toolbox>::*`, and registers functions via `Engine::registerFunction`.
  - `BuiltinLibrary::install(Engine &engine)` installs all standard library builtins into the engine.

### 2. Implementation for `ops`
- Removed `register_ops(Engine&)` and forward declaration of `Engine` from `src/builtin/include/numkit/builtin/ops.hpp`.
- Created `src/bundle/src/register/builtin/ops_reg.cpp` containing `register_ops(Engine &engine)` and `BuiltinLibrary::registerBinaryOps` / `BuiltinLibrary::registerUnaryOps`.
- Removed `src/builtin/src/ops.cpp` from `src/builtin/`.
