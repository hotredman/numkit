# Full Migration to Pure C++ Builtin: Matrix Functions (`matfun`)

## Context
As part of the layer separation refactoring (Layer L2 Compute vs Layer L3 Registration), matrix and integer division functions (`matfun`) were migrated to pure, engine-free C++ in `src/builtin/` under `namespace numkit::builtin`.

## Architectural Decision & Changes
1. **Public Engine-Free C++ Header**:
   - `include/numkit/builtin/matfun.hpp` defines the public C++ API with comprehensive Doxygen documentation, parameter descriptions, rounding modes, exceptions, and `@see` links.
   - Decoupled from `Engine`, `CallContext`, and the VM.

2. **Compute Implementation (`src/builtin/src/matfun/`)**:
   - `matfun.cpp`: Implements `idivide(a, b, mode, mr)` with full support for integer type compatibility rules and rounding modes (`fix`, `floor`, `ceil`, `round`).

3. **Engine Registration (`src/bundle/src/register/builtin/`)**:
   - `matfun_reg.cpp` registers `idivide` into `Engine` under `namespace numkit::bundle::builtin`.
   - Old monolithic `src/builtin/src/matfun.cpp` was deleted.

## Verification & Results
- Built with `desktop-fast` preset (MSVC).
- Layer validation: `python tools/check_layering.py` passed with 0 violations.
- Test suite: `numkit_gtest.exe --gtest_filter=*Matfun*:*Builtin*` passed 695/695 tests (100% green).
