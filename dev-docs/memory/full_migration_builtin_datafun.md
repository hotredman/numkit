# Full Migration to Pure C++ Builtin: Data Analysis & Reductions (`datafun`)

## Context
As part of the layer separation refactoring (Layer L2 Compute vs Layer L3 Registration), data analysis, reduction operators, random number distributions, and set operations (`datafun`) were migrated to pure, engine-free C++ in `src/builtin/` under `namespace numkit::builtin`.

## Architectural Decision & Changes
1. **Public Engine-Free C++ Header**:
   - `include/numkit/builtin/datafun.hpp` defines the public C++ API with comprehensive Doxygen documentation, parameter descriptions, exceptions, and `@see` links.
   - Decoupled from `Engine`, `CallContext`, and the VM.

2. **Compute Implementation (`src/builtin/src/datafun/`)**:
   - `reductions.cpp`: `sum`, `prod`, `mean`, `max`, `min`.
   - `random.cpp`: `rand`, `randn`, `randi`, `randperm`.
   - `sets.cpp`: `unique`, `ismember`, `union_set`, `intersect`, `setdiff`, `setxor`.

3. **Engine Registration (`src/bundle/src/register/builtin/`)**:
   - `datafun_reg.cpp` registers all data analysis and reduction builtins into `Engine` under `namespace numkit::bundle::builtin`.
   - Old monolithic `src/builtin/src/datafun.cpp` was deleted.

## Verification & Results
- Built with `desktop-fast` preset (MSVC).
- Layer validation: `python tools/check_layering.py` passed with 0 violations.
- Test suite: `numkit_gtest.exe --gtest_filter=*Datafun*:*Builtin*` passed 695/695 tests (100% green).
