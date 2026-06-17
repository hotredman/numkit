# numkit keeps complex-with-zero-imaginary; MATLAB narrows it to real

- **Status:** 🔴 OPEN — **arithmetic narrowing FIXED** (2026-06-17); residual:
  forced-`complex()` values through pure structural / index / indexed-assign ops
  still don't narrow (niche)
- **Severity:** P2 (divergent result on a fundamental type predicate; niche but real)
- **Kind:** bug
- **Found:** 2026-06-17 via the max/min complex audit (edge probe of `max(real, 2+0i)`)

## Symptom
MATLAB R2025b **narrows** a complex value whose imaginary part is exactly zero
*and that arose from an operation* back to a real double — so `isreal(2+0i)` is
`1`. The `complex(x,0)` constructor is the exception: it FORCES complex storage
(`isreal(complex(2,0))` is `0`).

```matlab
isreal(2+0i)            % MATLAB: 1 ; numkit was 0 -> NOW 1 (arithmetic narrows) [FIXED]
isreal((1+1i)+(1-1i))   % MATLAB: 1 ; numkit NOW 1 [FIXED]
isreal(complex(2,0))    % MATLAB: 0 ; numkit 0 (complex() forced -- agrees)
isreal(ifftshift(complex(ones(2)))) % MATLAB: 1 ; numkit 0 (structural -- RESIDUAL)
```

## Fixed (2026-06-17): arithmetic narrowing
`narrowComplex()` in `elementwiseComplex` (src/ops/include/numkit/ops/helpers.hpp)
narrows an all-real complex result back to a real double (a NaN imaginary part
keeps it complex). It covers binary arithmetic (`+ - .* ./ .^` and the scalar
matrix ops) and the max/min comparator — the dominant SOURCE of complex values.
So `2+0i`, `(1+1i)+(1-1i)`, `complex(1,1).*complex(1,-1)`, and the headline
`max([1 -3 2], 2+0i)` == `[2 2 2]` all now match MATLAB. Anything built via
arithmetic narrows at the source and propagates as real, so downstream structural
ops then see real input. Guard: `ComplexMathTest.NarrowsArithmeticAllReal`.

## Remaining (residual, niche, deferred)
A value forced complex via `complex()` and then passed through a PURE structural
or in-place op whose result is all-real still stays complex in numkit (MATLAB
narrows). These are separate chokepoints and rare in practice (they need a
forced-`complex()` source, since any arithmetic source already narrowed):

- `ifftshift`/`fftshift`/`circshift`/transpose and other element-reordering ops
- indexing (`w(2)`), indexed assignment (`c(2)=7`), concatenation `[z, z]`
- matrix multiply (`*`) building the result manually (not via elementwiseComplex)
- unary `-`/`conj` on a forced-complex value
- the bare `0i` literal (`isreal(0i)`: MATLAB 1, numkit 0)

To finish: apply `narrowComplex` at each of those op outputs. Each is additive
and low-risk (the helper already exists); deferred as low value.

## References
- `src/ops/include/numkit/ops/helpers.hpp` (`narrowComplex`, `elementwiseComplex`)
- `src/core/tests/complex_math_test.cpp` (`NarrowsArithmeticAllReal`)
- Related: bugs/math/maxmin-complex.md (headline `max([1 -3 2],2+0i)` now matches).
