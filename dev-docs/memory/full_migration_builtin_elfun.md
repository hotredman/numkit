# Full Migration to Pure C++ Builtin: Elementary Functions (`elfun`)

## Context
As part of the architecture refactoring (Layer L2 vs L3 separation), elementary functions (`elfun`) were completely migrated from legacy scattered locations into `src/builtin/` as pure engine-free C++ under `namespace numkit::builtin`.

## Architectural Decision & Changes
1. **Public Engine-Free C++ Header**:
   - `include/numkit/builtin/elfun.hpp` defines the public C++ API with comprehensive Doxygen documentation, mathematical formulas, parameter descriptions, exceptions, and `@see` links.
   - Return structures `PolarPair`, `CylTriple`, `CartPair`, `CartTriple`, `SphTriple` provide strongly typed outputs for coordinate transformations.
   - All functions are completely decoupled from `Engine`, `CallContext`, and the VM.

2. **Compute Implementation (`src/builtin/src/elfun/`)**:
   - `trigonometry.cpp`: Degree/radian conversions, angle wrapping (`wrapToPi`, `wrapTo2Pi`, `wrapTo180`, `wrapTo360`), and coordinate transforms (`cart2pol`, `pol2cart`, `cart2sph`, `sph2cart`).
   - `exponents.cpp`: `pow2`, `nextpow2`, `cbrt`, `nthroot`, `pow`, `realpow`.
   - `complex.cpp`: `real`, `imag`, `conj`, `angle`, `complex`, `isreal`, `unwrap`.
   - `rounding.cpp`: `floor`, `ceil`, `round`, `roundN`, `fix`, `sign`, `subplus`, `mod`, `rem`.
   - Dynamic Highway SIMD & portable backends:
     - `trig_highway.cpp`, `trig_portable.cpp`
     - `trig_recip_highway.cpp`, `trig_recip_portable.cpp`
     - `exp_log_highway.cpp`, `exp_log_portable.cpp`
     - `abs_highway.cpp`, `abs_portable.cpp`
     - `rounding_highway.cpp`, `rounding_portable.cpp`
     - `mod_highway.cpp`, `mod_portable.cpp`
     - `sinpi_kernel.hpp`

3. **Engine Registration (`src/bundle/src/register/builtin/`)**:
   - `elfun_reg.cpp` registers all elementary math functions into `Engine` under `namespace numkit::bundle::builtin`.
   - Old `src/builtin/src/elfun.cpp` with mixed registration code was deleted.

4. **Forwarding Headers (`src/math/include/numkit/math/`)**:
   - `trig/trigonometry.hpp`, `exp_log/exponents.hpp`, `complex/complex.hpp`, `arithmetic/rounding.hpp`, `arithmetic/misc.hpp` forward to `numkit::builtin::*`.

## Verification & Results
- Built with `desktop-fast` preset (MSVC with Highway SIMD enabled).
- Layer validation: `python tools/check_layering.py` passed with 0 violations.
- Test suite: `numkit_gtest.exe` passed 914/914 tests with 100% green.
