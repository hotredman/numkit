# Memory: Consolidation of `numkit::math` and `numkit::lang` into `numkit::builtin` (Variant 1)

## Context & Problem
Previously, the standard library functions in numkit were partitioned across several distinct layers and namespaces:
- `numkit::math` (in `src/math/` and subdirectories `complex`, `discrete`, `exponents`, `geom`, `integration`, `interp`, `misc`, `permutations`, `polynomials`, `reductions`, `rng`, `rounding`, `special`, `trigonometry`).
- `numkit::lang` (in `src/lang/` and subdirectories `binary_ops`, `coder`, `format`, `int_math`, `manip`, `matrix`, `nd_manip`, `print`, `regex`, `scan`, `strings`, `types`, `unary_ops`).
- `numkit::builtin` (in `src/builtin/`).

This separation caused inconsistent namespace usage across toolboxes and tests, duplicated functions across layers, and complicated header includes with transitional mappings.

## Decision & Solution
Following MATLAB standard library organization, all standard library compute functionality has been consolidated into:
1. **Directory Structure**: All standard functions reside under `src/builtin/` organized into 13 standard MATLAB categories:
   - `elfun/`: Elementary math (trig, exp, complex, rounding)
   - `elmat/`: Elementary matrices and array manipulation (eye, diag, reshape, cat, etc.)
   - `matfun/`: Matrix functions and linear algebra routines
   - `datafun/`: Data analysis, summaries, cumulative, FFT, convolution
   - `specfun/`: Specialized math functions (gamma, bessel, beta, combinatorics, etc.)
   - `polyfun/`: Polynomials, interpolation, calculus, geometry, integration
   - `strfun/`: String manipulation and regular expressions
   - `timefun/`: Date and time functions
   - `datatypes/`: Type inspection, type conversions, cell/struct helpers
   - `iofun/`: Formatted I/O, file reading/writing helpers
   - `general/`: Commands, workspace management, helper utilities
   - `ops/`: Operators, arithmetic, relational, bitwise operations
   - `lang/`: Language control and environment functions

2. **Unified Flat Namespace**:
   - All 13 categories are defined under `namespace numkit::builtin`.
   - No sub-namespaces like `numkit::math` or `numkit::lang` remain in the codebase.
   - Public C++ API headers live under `<numkit/builtin/<category>.hpp>`.

3. **Strict Layer Separation**:
   - `src/builtin/` (L2) contains 100% pure C++ engine-free compute kernels.
   - `src/bundle/src/register/builtin/` (L3) contains all `Engine` adapters, registration functions, and `CallContext` wrappers (`*_reg.cpp`).
   - `src/runtime/` (L2) handles engine-coupled runtime builtins (`eval`, `rehash`, `print_io`, `scan_io`).
   - Toolboxes (`signal`, `stats`, `image`, `control`, etc.) call `numkit::builtin::*` directly.

4. **Tests & Benchmarks**:
   - All tests migrated from `src/math/tests/` and `src/lang/tests/` to `src/builtin/tests/`.
   - All benchmarks migrated to `src/builtin/benchmarks/`.
   - Removed obsolete `src/math/` and `src/lang/` directories.

## Validation & Verification
- Pure compute libraries (`numkit_builtin_obj.lib`), runtime (`numkit_runtime_obj.lib`), bundle (`numkit_bundle_obj.lib`), toolboxes (`numkit_toolboxes_obj.lib`), and full static library (`numkit.lib`) compile cleanly with zero errors.
- `tools/check_layering.py` passes with zero violations.
- Test suite passes cleanly across all builtins, standard library categories, toolboxes, and file I/O.
