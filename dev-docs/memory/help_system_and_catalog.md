# MATLAB-Style Category Organization and `help` / `what` / `builtins` System

## Context & Problem
Previously, Numkit registered hundreds of built-in and toolbox functions across multiple layers without a user-facing introspection and documentation query system (like MATLAB's `help`, `what`, and `inmem`). Users in the REPL and IDE had no way to discover registered functions or query signatures and category listings interactively.

## Architectural Decision & Solution
Introduced a centralized, structured Help and Category Catalog (`help_catalog.hpp`, `help_catalog.cpp`) and registered user-facing introspection functions (`help_reg.cpp`):

1. **Standard MATLAB Category Mapping**:
   * **`elmat`**: Elementary matrices and matrix manipulation (`zeros`, `ones`, `eye`, `rand`, `randn`, `linspace`, `diag`, `reshape`, `cat`, `find`, `flip`, `rot90`, `repmat`, `size`, `length`, `isnan`, `eps`, `pi`, `inf`, `nan`...).
   * **`elfun`**: Elementary math functions (Trigonometric `sin`..`atan2`, Exponential `exp`..`log`, Complex `abs`..`imag`, Rounding `round`..`sign`...).
   * **`matfun`**: Matrix functions and linear algebra (`inv`, `det`, `eig`, `svd`, `lu`, `qr`, `chol`, `rank`, `null`, `pinv`, `rref`, `norm`, `cond`, `expm`, `logm`, `sqrtm`, `funm`...).
   * **`datafun`**: Data analysis, statistics, FFT and convolution (`sum`, `mean`, `median`, `std`, `var`, `min`, `max`, `sort`, `fft`, `ifft`, `fft2`, `conv`, `filter`, `cov`, `corrcoef`, `movmean`...).
   * **`specfun`**: Specialized math functions (`gamma`, `beta`, `erf`, `besselj`, `bessely`, `legendre`...).
   * **`polyfun`**: Polynomials, interpolation, and integration (`roots`, `poly`, `polyval`, `polyfit`, `interp1`, `interp2`, `spline`, `integral`, `trapz`...).
   * **`strfun`**: String manipulation and regex (`strcmp`, `strfind`, `strrep`, `sprintf`, `sscanf`, `strsplit`, `lower`, `upper`, `regexprep`...).
   * **`timefun`**: Time and dates (`tic`, `toc`, `clock`, `date`, `now`, `pause`, `timeit`...).
   * **`datatypes`**: Structures, cells, maps, and OOP inspection (`struct`, `cell`, `fieldnames`, `cellfun`, `arrayfun`, `class`, `isa`, `methods`, `properties`, `containers.Map`...).
   * **`iofun`**: File input/output (`fopen`, `fclose`, `fread`, `fwrite`, `fprintf`, `fscanf`, `fgetl`, `fseek`, `ftell`, `feof`...).
   * **`general`**: Workspace and session (`clear`, `clc`, `who`, `whos`, `which`, `exist`, `what`, `inmem`, `help`, `cd`, `dir`...).
   * **`graphics`**: Plotting and 2D/3D visualization (`plot`, `surf`, `mesh`, `contour`, `bar`, `histogram`, `figure`, `imshow`, `title`, `xlabel`, `legend`...).
   * **Toolboxes**: `image` / `images`, `signal`, `optim`, `ode`, `stats`, `control`.

2. **User-Facing Built-ins (`help_reg.cpp`)**:
   * **`help`**:
     * `help` — displays the catalog of all categories.
     * `help <category>` — displays formatted category summary with subheadings and one-line function descriptions.
     * `help <function>` — displays function header, signature, and documentation docstring.
     * `txt = help(...)` — returns help text as string when output argument is requested.
   * **`what`**:
     * `what <category>` — returns struct with `.m` cellstr array of functions (or prints column-aligned list).
   * **`builtins`**:
     * `builtins()` — returns cellstr of all 400+ registered functions.
     * `builtins(category)` — returns cellstr of functions in specified category.
   * **`inmem`**:
     * `[M, MEX, C] = inmem()` — returns loaded user functions and classes.

3. **IDE Integration & Demos**:
   * Added interactive example [`examples/Basics/help_system_demo.m`](file:///c:/Users/User/Projects/megahard/numkit/examples/Basics/help_system_demo.m) demonstrating all features.
   * Added automated test suite [`tests/gtest/integration/help_system_test.cpp`](file:///c:/Users/User/Projects/megahard/numkit/tests/gtest/integration/help_system_test.cpp).

## Quantitative Verification & Results
* `HelpSystemTest`: 7/7 tests passed in 45 ms.
* `numkit_repl.exe --compat examples/Basics/help_system_demo.m` executed with zero errors and produced full formatted output for all 18 categories and 419 registered functions.
