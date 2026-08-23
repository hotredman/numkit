# Memory: Public C++ API Architectural Standards & Documentation Guidelines

## Context & Objectives
As part of the decomposition of the monolithic standard library into modular categories under `src/builtin/` (`elmat`, `elfun`, `matfun`, `datafun`, `specfun`, `polyfun`, `strfun`, `timefun`, `datatypes`, `iofun`, `general`, `ops`, `lang`), we provide clean, engine-free public C++ APIs in `numkit::builtin::...`.

To guarantee maximum developer productivity, clear IDE tooltips (via IntelliSense/Clangd), and consistent library ergonomics across all modules, we establish this formal standard for all public C++ headers.

## Decision: Full Doxygen Documentation Standard
Every function exposed in `include/numkit/builtin/*.hpp` must provide full Doxygen comments:
1. **Mathematical & Algebraic Formula**: Mention explicit notation in `@brief` (e.g. `Elementwise right division (y = a ./ b)`).
2. **Behavioral Semantics**:
   - Singleton expansion / automatic broadcasting across N-D shapes.
   - Type promotions, complex conversion when applicable (e.g., negative base raised to fractional powers in `power`).
   - Dimension agreement requirements (e.g., inner dimensions in `mtimes`, batch broadcasting in `pagemtimes`).
3. **Parameter Specifications**:
   - Clear descriptions for every operand (`@param a`, `@param b`, etc.).
   - Standard `@param mr Memory resource for allocations (nullptr for default).` for all allocating functions.
4. **Return Specification**:
   - Detailed `@return` specifying shape and type.
5. **Cross-References (`@see`)**:
   - Pairwise links between complementary and dual functions (e.g. `plus` ↔ `minus`, `rdivide` ↔ `ldivide` ↔ `mrdivide`, `power` ↔ `mpower`).
6. **Exception Specification (`@throws`)**:
   - Explicitly list exceptions on mismatched dimensions or invalid arguments.

## C++ Keyword Collision Handling
Where MATLAB builtin names collide with reserved C++ keywords (`and`, `or`, `not`, `xor`), declare both descriptive functions and short alias overloads:
- `logical_and` and `and_op`
- `logical_or` and `or_op`
- `logical_not` and `not_op`
- `logical_xor` and `xor_op`

## Engine Decoupling
Public builtin headers (`include/numkit/builtin/*.hpp`) must NOT include `<numkit/core/engine.hpp>`. Instead:
- Forward-declare `namespace numkit { class Engine; }`.
- Only registration functions (`void register_<category>(Engine &engine);`) accept `Engine &`.
- All operational builtins operate purely on `Value`, `Span<const Value>`, and PMR memory resources.

## Completed Modules & Verification Status
All 13 modular categories under `src/builtin/` have now been completed with engine-decoupled C++ APIs, comprehensive Doxygen documentation, and automated unit test suites:
1. **`ops`**: Arithmetic, relational, logical (`logical_and`/`and_op`, etc.), and transposition (`ctranspose`, `transpose`).
2. **`lang`**: `iskeyword`, `keywords`, `isvarname`, `setenv`, `getenv`.
3. **`general`**: `help`, `what`, `builtins`, `categories`.
4. **`matfun`**: `idivide`, matrix decompositions, and matrix operations.
5. **`specfun`**: Special mathematical functions (gamma, beta, Bessel, Airy, erf, combinatorics/discrete number theory).
6. **`polyfun`**: Polynomial operations, root finding, interpolation (`interp1`..`interpn`, `spline`, `pchip`), and integration (`trapz`, `cumtrapz`).
7. **`elmat`**, **`elfun`**, **`datafun`**, **`strfun`**, **`timefun`**, **`datatypes`**, **`iofun`**: Full coverage.

Verification: All 36 tests across 13 builtin test suites run cleanly (36/36 passed, 0 failures), and `tools/check_layering.py` verifies 0 architecture violations.
