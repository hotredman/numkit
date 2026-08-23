# Full Migration to Pure C++ Builtin: Data Types & Introspection (`datatypes`)

## Context
As part of the layer separation refactoring (Layer L2 Compute vs Layer L3 Registration), data types, type predicates, structure manipulation, cell array queries, and numeric range limits (`datatypes`) were migrated to pure, engine-free C++ in `src/builtin/` under `namespace numkit::builtin`.

## Architectural Decision & Changes
1. **Public Engine-Free C++ Header**:
   - `include/numkit/builtin/datatypes.hpp` defines the public C++ API with comprehensive Doxygen documentation, parameter descriptions, formulas, exceptions, and `@see` links.
   - Decoupled from `Engine`, `CallContext`, and the VM.

2. **Compute Implementation (`src/builtin/src/datatypes/`)**:
   - `predicates.cpp`: `isnumeric`, `isfloat`, `isinteger`, `islogical`, `ischar`, `isstring`, `isnan`, `isinf`, `isfinite`, `isempty`, `isscalar`, `isvector`, `isrow`, `iscolumn`, `ismatrix`, `isequal`, `isequaln`, `iscell`, `isstruct`.
   - `structs_cells.cpp`: `isfield`, `getfield`, `setfield`, `rmfield`.
   - `limits.cpp`: `cast`, `realmin`, `realmax`, `intmin`, `intmax`, `flintmax`.

3. **Engine Registration (`src/bundle/src/register/builtin/`)**:
   - `datatypes_reg.cpp` registers all datatype builtins into `Engine` under `namespace numkit::bundle::builtin`.
   - Old monolithic `src/builtin/src/datatypes.cpp` was deleted.

## Verification & Results
- Built with `desktop-fast` preset (MSVC).
- Layer validation: `python tools/check_layering.py` passed with 0 violations.
- Test suite: `numkit_gtest.exe --gtest_filter=*Datatypes*:*Types*:*Builtin*` passed 812/812 tests (100% green).
