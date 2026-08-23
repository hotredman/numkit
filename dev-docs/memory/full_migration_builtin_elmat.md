# Full Migration to Pure C++ Builtin: Elementary Matrices (`elmat`)

## Context
As part of the layer separation refactoring (Layer L2 Compute vs Layer L3 Registration), elementary matrix and array manipulation functions (`elmat`) were migrated to pure, engine-free C++ in `src/builtin/` under `namespace numkit::builtin`.

## Architectural Decision & Changes
1. **Public Engine-Free C++ Header**:
   - `include/numkit/builtin/elmat.hpp` defines the public C++ API with comprehensive Doxygen documentation, parameter descriptions, exceptions, and `@see` links.
   - Structured returns `Meshgrid2D` and `Meshgrid3D` for coordinate grids.
   - Decoupled from `Engine`, `CallContext`, and `detail` registrations.

2. **Compute Implementation (`src/builtin/src/elmat/`)**:
   - `creation.cpp`: `zeros`, `ones`, `eye`, `linspace`, `logspace`, `magic`, `hilb`, `invhilb`, `pascal`, `toeplitz`, `vander`, `wilkinson`, `rosser`, `hadamard`, `hankel`, `compan`, `meshgrid`, `ndgrid`.
   - `manipulation.cpp`: `repmat`, `repelem`, `reshape`, `diag`, `blkdiag`, `cat`, `horzcat`, `vertcat`, `rot90`, `fliplr`, `flipud`, `flip`, `circshift`, `permute`, `ipermute`, `shiftdim`, `squeeze`, `head`, `tail`, `paddata`, `trimdata`, `bsxfun`, `tril`, `triu`.

3. **Engine Registration (`src/bundle/src/register/builtin/`)**:
   - `elmat_reg.cpp` registers all elementary matrix functions into `Engine` under `namespace numkit::bundle::builtin`.
   - Old monolithic `src/builtin/src/elmat.cpp` was deleted.

## Verification & Results
- Built with `desktop-fast` preset (MSVC).
- Layer validation: `python tools/check_layering.py` passed with 0 violations.
- Test suite: `numkit_gtest.exe --gtest_filter=*Elmat*:*Builtin*` passed 696/696 tests (100% green).
