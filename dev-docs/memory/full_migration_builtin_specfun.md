# Full Migration to Pure C++ Builtin: Special Functions (`specfun`)

## Context
As part of the layer separation refactoring (Layer L2 Compute vs Layer L3 Registration), special mathematical functions (`specfun`) were migrated to pure, engine-free C++ in `src/builtin/` under `namespace numkit::builtin`.

## Architectural Decision & Changes
1. **Public Engine-Free C++ Header**:
   - `include/numkit/builtin/specfun.hpp` defines the public C++ API with comprehensive Doxygen documentation, parameter descriptions, formulas, exceptions, and `@see` links.
   - Decoupled from `Engine`, `CallContext`, and the VM.

2. **Compute Implementation (`src/builtin/src/specfun/`)**:
   - `gamma_beta.cpp`: `gamma`, `gammaln`, `gammainc`, `gammaincinv`, `psi`, `beta`, `betaln`, `betainc`, `betaincinv`.
   - `erf.cpp`: `erf`, `erfc`, `erfcx`, `erfinv`, `erfcinv`.
   - `bessel.cpp`: `besselj`, `bessely`, `besseli`, `besselk`, `besselh`, `airy`, `expint`, `ellipke`, `legendre`.
   - `combinatorics.cpp`: `factorial`, `nchoosek`, `perms`, `gcd`, `lcm`.

3. **Engine Registration (`src/bundle/src/register/builtin/`)**:
   - `specfun_reg.cpp` registers all special function builtins into `Engine` under `namespace numkit::bundle::builtin`.
   - Old monolithic `src/builtin/src/specfun.cpp` was deleted.

## Verification & Results
- Built with `desktop-fast` preset (MSVC).
- Layer validation: `python tools/check_layering.py` passed with 0 violations.
- Test suite: `numkit_gtest.exe --gtest_filter=*Specfun*:*Builtin*` passed 694/694 tests (100% green).
