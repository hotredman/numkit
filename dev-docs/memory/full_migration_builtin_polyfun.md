# Full Migration to Pure C++ Builtin: Polynomials & Interpolation (`polyfun`)

## Context
As part of the layer separation refactoring (Layer L2 Compute vs Layer L3 Registration), polynomials, 1D/2D interpolation, piecewise curves (spline, pchip, ppform), and numerical trapezoidal integration (`polyfun`) were migrated to pure, engine-free C++ in `src/builtin/` under `namespace numkit::builtin`.

## Architectural Decision & Changes
1. **Public Engine-Free C++ Header**:
   - `include/numkit/builtin/polyfun.hpp` defines the public C++ API with comprehensive Doxygen documentation, parameter descriptions, formulas, exceptions, and `@see` links.
   - Decoupled from `Engine`, `CallContext`, and the VM.

2. **Compute Implementation (`src/builtin/src/polyfun/`)**:
   - `polynomials.cpp`: `roots`, `poly`, `polyval`, `polyder`, `polyint`, `polyfit`.
   - `interpolation.cpp`: `interp1`, `interp2`, `spline`, `pchip`, `mkpp`, `unmkpp`, `ppval`.
   - `integration.cpp`: `trapz`, `cumtrapz`.

3. **Engine Registration (`src/bundle/src/register/builtin/`)**:
   - `polyfun_reg.cpp` registers all polynomial, interpolation, and integration builtins into `Engine` under `namespace numkit::bundle::builtin`.
   - Old monolithic `src/builtin/src/polyfun.cpp` was deleted.

## Verification & Results
- Built with `desktop-fast` preset (MSVC).
- Layer validation: `python tools/check_layering.py` passed with 0 violations.
- Test suite: `numkit_gtest.exe --gtest_filter=*Polyfun*:*Builtin*` passed 694/694 tests (100% green).
