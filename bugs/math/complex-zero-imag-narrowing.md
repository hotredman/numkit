# numkit keeps complex-with-zero-imaginary; MATLAB narrows it to real

- **Status:** 🔴 OPEN — arithmetic + unary + matmul + fused + **indexing** all
  narrow (2026-06-17); thin residual: a few forced-`complex()`-only structural
  ops (z(:)/reorder/reductions/indexed-assign) still don't narrow (niche)
- **Severity:** P2 (divergent result on a fundamental type predicate; niche but real)
- **Kind:** bug
- **Found:** 2026-06-17 via the max/min complex audit (edge probe of `max(real, 2+0i)`)

## Symptom
MATLAB R2025b **narrows** a complex value whose imaginary part is exactly zero
*and that arose from an operation* back to a real double — so `isreal(2+0i)` is
`1`. The `complex(x,0)` constructor is the exception: it FORCES complex storage
(`isreal(complex(2,0))` is `0`).

```matlab
isreal(2+0i)            % MATLAB 1 ; numkit was 0 -> NOW 1 (arithmetic narrows) [FIXED]
isreal((1+1i)+(1-1i))   % MATLAB 1 ; numkit NOW 1 [FIXED]
isreal(complex(2,0))    % MATLAB 0 ; numkit 0 (complex() forced -- agrees)
zc = complex([1 -3 2]); isreal(zc(2))  % MATLAB 1 ; numkit NOW 1 (indexing) [FIXED]
```

## Fixed (2026-06-17)
`narrowComplex()` narrows an all-real complex result back to a real double (a NaN
imaginary part keeps it complex). It was **moved to the value layer**
(`src/value/.../value.hpp` decl + `value.cpp` def) so every layer can use it.
Applied at:

- **arithmetic** `+ - .* ./ .^` and the max/min comparator (`elementwiseComplex`)
  — the dominant SOURCE of complex values, so `2+0i`/`(1+1i)+(1-1i)` narrow and
  `max([1 -3 2], 2+0i)` == `[2 2 2]` matches MATLAB.
- **unary** `-`/`+`/`conj` (`unaryComplex`) and **matrix multiply** `*`.
- **every fused-expression result** (FusionRule execute wrapped via `addNarrowing`,
  keeping fused==per-op).
- **indexing** — scalar `z(k)` (value-layer `elemAt`, covering the VM/TW scalar
  fast paths), ranges/colon-lists, and `indexGet2D`/`3D`/`ND` slices.

Guards: `ComplexMathTest.NarrowsArithmeticAllReal` + `.NarrowsIndexingAllRealSlice`.
Anything built via arithmetic narrows at the source and propagates as real.

## Remaining (residual, niche, deferred)
Only a value FORCED complex via `complex()` and then passed through one of these
all-real-yielding ops still stays complex (MATLAB narrows; numkit doesn't):

- `z(:)` colon-linearize — numkit implements it as a **reshape**, which (like
  `reshape`) preserves; MATLAB's `z(:)` narrows. Same-mechanism-as-reshape niche.
- element-reordering `fliplr`/`flip`/`circshift`/`repmat`/`rot90` (lang/manip.cpp,
  many per-rank return sites; no shared chokepoint).
- reductions `sum`/`prod` (math; the `reduce` template is double-accumulator — the
  complex path isn't a single clean chokepoint).
- indexed assignment `c(2)=7` (value-layer `indexSet` is in-place `void` with no
  `mr` — narrowing the variable post-assign needs a different hook).

All require a forced-`complex()` source (arithmetic already narrows at the source),
so they're edge-of-edge. **PRESERVE — already correct (match MATLAB):** `reshape`,
`transpose`/`ctranspose`, concatenation `[z z]`/`cat`, `sort`. Bare `0i` literal
also stays complex.

## References
- `src/value/include/numkit/value/value.hpp` + `value.cpp` (`narrowComplex` — value layer)
- `src/ops/include/numkit/ops/helpers.hpp` (`elementwiseComplex`/`unaryComplex` apply it)
- `src/value/src/value.cpp` (`elemAt` + `indexGet`/`2D`/`3D`/`ND` apply it)
- `src/core/tests/complex_math_test.cpp` (`NarrowsArithmeticAllReal`, `NarrowsIndexingAllRealSlice`)
- Related: bugs/math/maxmin-complex.md (headline `max([1 -3 2],2+0i)` now matches).
