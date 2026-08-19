# Architecture Plan: Decomposing `builtin_library.cpp` into `src/builtin/`

## Context & Objectives
The standard library built-ins in Numkit are currently registered primarily through a 4390-line monolithic file [`src/bundle/src/builtin_library.cpp`](file:///c:/Users/User/Projects/megahard/numkit/src/bundle/src/builtin_library.cpp) and scattered adapters in `src/bundle/src/register/`.

To achieve full modularity and exact parity with MATLAB's standard library organization (`elmat`, `elfun`, `matfun`, `datafun`, `specfun`, `polyfun`, `strfun`, `timefun`, `datatypes`, `iofun`, `general`, `ops`, `lang`), we are decomposing this monolith into a dedicated, flat `src/builtin/` directory.

---

## Target Directory Layout

```text
src/builtin/
├── CMakeLists.txt                    # Registers all builtin sources, headers, and tests
│
├── include/numkit/builtin/
│   ├── builtin.hpp                   # Master installer: BuiltinLibrary::install(Engine &)
│   ├── elmat.hpp                     # void register_elmat(Engine &engine);
│   ├── elfun.hpp                     # void register_elfun(Engine &engine);
│   ├── matfun.hpp                    # void register_matfun(Engine &engine);
│   ├── datafun.hpp                   # void register_datafun(Engine &engine);
│   ├── specfun.hpp                   # void register_specfun(Engine &engine);
│   ├── polyfun.hpp                   # void register_polyfun(Engine &engine);
│   ├── strfun.hpp                    # void register_strfun(Engine &engine);
│   ├── timefun.hpp                   # void register_timefun(Engine &engine);
│   ├── datatypes.hpp                 # void register_datatypes(Engine &engine);
│   ├── iofun.hpp                     # void register_iofun(Engine &engine);
│   ├── general.hpp                   # void register_general(Engine &engine);
│   ├── ops.hpp                       # void register_ops(Engine &engine);
│   └── lang.hpp                      # void register_lang(Engine &engine);
│
├── src/
│   ├── builtin.cpp                   # Calls register_elmat, register_elfun, etc.
│   ├── elmat.cpp                     # zeros, ones, eye, rand, linspace, reshape, diag, cat, repmat, find, flip, size, isnan, eps, pi...
│   ├── elfun.cpp                     # sin, cos, tan, exp, log, sqrt, abs, round, floor, ceil, mod, rem, real, imag, conj...
│   ├── matfun.cpp                    # inv, det, eig, svd, lu, qr, chol, rank, null, pinv, rref, norm, cond, expm, funm...
│   ├── datafun.cpp                   # sum, prod, mean, median, std, var, min, max, sort, fft, ifft, fft2, conv, filter, cov...
│   ├── specfun.cpp                   # gamma, gammainc, gammaln, beta, erf, erfc, besselj, bessely, legendre...
│   ├── polyfun.cpp                   # roots, poly, polyval, polyfit, interp1, interp2, spline, integral, trapz...
│   ├── strfun.cpp                    # strcmp, strfind, strrep, sprintf, sscanf, strsplit, lower, upper, regexprep...
│   ├── timefun.cpp                   # tic, toc, clock, date, now, pause, timeit, cputime...
│   ├── datatypes.cpp                 # struct, cell, fieldnames, cellfun, arrayfun, class, isa, methods, properties...
│   ├── iofun.cpp                     # fopen, fclose, fread, fwrite, fprintf, fscanf, fgetl, fseek, ftell, feof...
│   ├── general.cpp                   # clear, clc, who, whos, which, exist, what, inmem, help, cd, dir...
│   ├── ops.cpp                       # plus, minus, times, mtimes, rdivide, ldivide, and, or, not, xor...
│   └── lang.cpp                      # nargin, nargout, inputname, eval, evalin, feval, builtin, error, warning...
│
└── tests/
    ├── CMakeLists.txt                # Adds test TUs via target_sources(numkit_gtest PRIVATE ...)
    ├── elmat_test.cpp
    ├── elfun_test.cpp
    ├── matfun_test.cpp
    ├── datafun_test.cpp
    ├── specfun_test.cpp
    ├── polyfun_test.cpp
    ├── strfun_test.cpp
    ├── timefun_test.cpp
    ├── datatypes_test.cpp
    ├── iofun_test.cpp
    ├── general_test.cpp
    ├── ops_test.cpp
    ├── lang_test.cpp
    └── builtin_test.cpp
```

---

## Category-to-Function Mapping

| Module | Functions Included | Header | Source | Test |
|:---|:---|:---|:---|:---|
| **`elmat`** | `zeros`, `ones`, `eye`, `rand`, `randn`, `randi`, `linspace`, `logspace`, `freqspace`, `meshgrid`, `ndgrid`, `cat`, `horzcat`, `vertcat`, `reshape`, `diag`, `blkdiag`, `tril`, `triu`, `fliplr`, `flipud`, `flip`, `rot90`, `repmat`, `repelem`, `permute`, `ipermute`, `shiftdim`, `circshift`, `squeeze`, `find`, `sub2ind`, `ind2sub`, `bsxfun`, `size`, `length`, `ndims`, `numel`, `isempty`, `isequal`, `isequaln`, `isnan`, `isinf`, `isfinite`, `isscalar`, `isvector`, `isrow`, `iscolumn`, `ismatrix`, `true`, `false`, `eps`, `pi`, `inf`, `nan` | `elmat.hpp` | `elmat.cpp` | `elmat_test.cpp` |
| **`elfun`** | `sin`, `sind`, `sinh`, `asin`, `asind`, `asinh`, `cos`, `cosd`, `cosh`, `acos`, `acosd`, `acosh`, `tan`, `tand`, `tanh`, `atan`, `atand`, `atan2`, `atan2d`, `atanh`, `sec`, `secd`, `sech`, `asec`, `csc`, `cscd`, `csch`, `acsc`, `cot`, `cotd`, `coth`, `acot`, `hypot`, `deg2rad`, `rad2deg`, `exp`, `log`, `log10`, `log2`, `pow2`, `sqrt`, `cbrt`, `nextpow2`, `abs`, `angle`, `complex`, `conj`, `real`, `imag`, `round`, `floor`, `ceil`, `fix`, `mod`, `rem`, `sign` | `elfun.hpp` | `elfun.cpp` | `elfun_test.cpp` |
| **`matfun`** | `inv`, `pinv`, `det`, `trace`, `rank`, `null`, `orth`, `rref`, `norm`, `cond`, `rcond`, `linsolve`, `decomposition`, `lu`, `qr`, `chol`, `svd`, `eig`, `schur`, `balance`, `expm`, `logm`, `sqrtm`, `funm` | `matfun.hpp` | `matfun.cpp` | `matfun_test.cpp` |
| **`datafun`**| `sum`, `prod`, `cumsum`, `cumprod`, `diff`, `gradient`, `mean`, `median`, `mode`, `std`, `var`, `min`, `max`, `bounds`, `sort`, `sortrows`, `cov`, `corrcoef`, `conv`, `conv2`, `filter`, `filter2`, `fft`, `ifft`, `fft2`, `ifft2`, `fftn`, `ifftn`, `fftshift`, `ifftshift`, `movmean`, `movmedian`, `movstd`, `movvar`, `movmin`, `movmax`, `movsum` | `datafun.hpp` | `datafun.cpp` | `datafun_test.cpp` |
| **`specfun`**| `gamma`, `gammainc`, `gammaln`, `psi`, `beta`, `betainc`, `betaln`, `factorial`, `erf`, `erfc`, `erfinv`, `erfcinv`, `besselj`, `bessely`, `besseli`, `besselk`, `legendre`, `ellipke` | `specfun.hpp` | `specfun.cpp` | `specfun_test.cpp` |
| **`polyfun`**| `roots`, `poly`, `polyval`, `polyfit`, `polyder`, `polyint`, `deconv`, `interp1`, `interp2`, `interp3`, `interpn`, `spline`, `pchip`, `integral`, `integral2`, `integral3`, `trapz`, `cumtrapz` | `polyfun.hpp` | `polyfun.cpp` | `polyfun_test.cpp` |
| **`strfun`** | `strcmp`, `strncmp`, `strcmpi`, `strncmpi`, `strfind`, `strrep`, `contains`, `startsWith`, `endsWith`, `sprintf`, `sscanf`, `strsplit`, `strjoin`, `strtrim`, `lower`, `upper`, `num2str`, `str2num`, `str2double`, `regexprep`, `regexp`, `regexpi` | `strfun.hpp` | `strfun.cpp` | `strfun_test.cpp` |
| **`timefun`**| `tic`, `toc`, `clock`, `date`, `now`, `datestr`, `datenum`, `pause`, `cputime`, `timeit` | `timefun.hpp` | `timefun.cpp` | `timefun_test.cpp` |
| **`datatypes`**| `struct`, `cell`, `isstruct`, `iscell`, `iscellstr`, `fieldnames`, `isfield`, `getfield`, `setfield`, `rmfield`, `cellfun`, `arrayfun`, `structfun`, `class`, `isa`, `isobject`, `isprop`, `ismethod`, `methods`, `properties`, `containers.Map` | `datatypes.hpp` | `datatypes.cpp` | `datatypes_test.cpp` |
| **`iofun`**  | `fopen`, `fclose`, `fread`, `fwrite`, `fprintf`, `fscanf`, `fgetl`, `fgets`, `fseek`, `ftell`, `feof`, `frewind`, `fileread`, `tempname`, `tempdir` | `iofun.hpp` | `iofun.cpp` | `iofun_test.cpp` |
| **`general`**| `clear`, `clc`, `who`, `whos`, `which`, `exist`, `what`, `inmem`, `help`, `doc`, `path`, `addpath`, `rmpath`, `cd`, `pwd`, `dir` | `general.hpp` | `general.cpp` | `general_test.cpp` |
| **`ops`**    | `plus`, `minus`, `times`, `mtimes`, `rdivide`, `ldivide`, `power`, `mpower`, `lt`, `gt`, `le`, `ge`, `eq`, `ne`, `and`, `or`, `not`, `xor` | `ops.hpp` | `ops.cpp` | `ops_test.cpp` |
| **`lang`**   | `nargin`, `nargout`, `inputname`, `eval`, `evalin`, `feval`, `builtin`, `error`, `warning`, `lasterr`, `lastwarn` | `lang.hpp` | `lang.cpp` | `lang_test.cpp` |
| **`builtin`**| Facade registering all the above modules via `BuiltinLibrary::install(Engine &engine)` | `builtin.hpp` | `builtin.cpp` | `builtin_test.cpp` |

---

## Step-by-Step Execution Roadmap for the Next Agent

1. **Create `src/builtin/CMakeLists.txt`**:
   * Collect `NUMKIT_BUILTIN_SOURCES`, `NUMKIT_BUILTIN_HEADERS`, and `NUMKIT_BUILTIN_INCLUDE_DIRS`.
   * Export to `PARENT_SCOPE`.
   * Include `tests/CMakeLists.txt`.

2. **Wire `src/builtin/` into Root `CMakeLists.txt`**:
   * Add `add_subdirectory(src/builtin)`.
   * Add `NUMKIT_BUILTIN_INCLUDE_DIRS` to layer validation check.
   * Add `numkit_builtin_obj` to `NUMKIT_LAYER_OBJECTS`.

3. **Migrate Each Module One by One**:
   * For each category:
     * Create `src/builtin/include/numkit/builtin/<category>.hpp` declaring `void register_<category>(Engine &engine);`.
     * Create `src/builtin/src/<category>.cpp` moving the implementations from `builtin_library.cpp`.
     * Create `src/builtin/tests/<category>_test.cpp` with unit tests checking functions and MATLAB parity.
     * Run `cmake --build --preset=desktop-fast --target numkit_gtest && build\desktop-fast\tests\gtest\Release\numkit_gtest.exe --gtest_filter=<Category>Test.*`.

4. **Replace Monolithic `builtin_library.cpp` with `src/builtin/src/builtin.cpp`**:
   * Implement `BuiltinLibrary::install(Engine &engine)` to simply invoke `register_<category>(engine)` for all 13 categories.

5. **Verify Full Build and Run All Tests**:
   * Full gtest suite: `numkit_gtest.exe`.
   * Interpreter smoke test: `numkit_repl.exe --compat examples/Basics/help_system_demo.m`.
   * Build web and desktop: `scripts\desktop-build.bat --no-package && scripts\web-build.bat && scripts\publish-pages.bat --skip-build --push`.

---

## Important Rules & Guidelines
* **No `_reg` suffixes**: All files must use clean names (`elmat.cpp`, `elfun.cpp`, `matfun.cpp`, etc.).
* **1-to-1 Test Symmetry**: Every `.cpp` in `src/builtin/src/` must have its corresponding `_test.cpp` in `src/builtin/tests/`.
* **Preserve Toolboxes**: Standalone domain toolboxes (`src/toolboxes/image`, `signal`, `optim`, `ode`, `stats`, `control`, `audio`, `comm`, `wavelet`, `fusion`) stay in `src/toolboxes/` and are NOT part of `src/builtin/`.
