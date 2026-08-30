# numkit keeps complex-with-zero-imaginary; MATLAB narrows it to real

- **Status:** ✅ FIXED (2026-06-17) — every operation MATLAB narrows now narrows
  in numkit too, on both backends; the ops MATLAB *preserves* are unchanged.
- **Severity:** P2 (divergent result on a fundamental type predicate; niche but real)
- **Kind:** bug
- **Found:** 2026-06-17 via the max/min complex audit (edge probe of `max(real, 2+0i)`)

## Symptom
MATLAB R2025b **narrows** a complex value whose imaginary part is exactly zero
*and that arose from an operation* back to a real double — so `isreal(2+0i)` is
`1`. The `complex(x,0)` constructor is the exception: it FORCES complex storage
(`isreal(complex(2,0))` is `0`), and a NaN imaginary part also keeps it complex.

```matlab
isreal(2+0i)            % MATLAB 1 ; numkit NOW 1 (arithmetic narrows)
isreal((1+1i)+(1-1i))   % MATLAB 1 ; numkit NOW 1
isreal(complex(2,0))    % MATLAB 0 ; numkit 0 (complex() forced -- agrees)
zc = complex([1 -3 2]); isreal(zc(2))  % MATLAB 1 ; numkit NOW 1 (indexing)
isreal(sum(complex([1 2 3])))          % MATLAB 1 ; numkit NOW 1 (reduction)
isreal(fliplr(complex([1 2 3])))       % MATLAB 1 ; numkit NOW 1 (reorder)
c = complex([0 0 0]); c(2)=7; isreal(c)% MATLAB 1 ; numkit 0 (indexed-assign: see note)
```

## Fix (2026-06-17)
`narrowComplex(Value, mr)` narrows an all-real complex result back to a real
double (a NaN imaginary part keeps it complex; `complex()`-forced storage is
narrowed only once it flows through an operation). It lives in the **value
layer** (`src/value/.../value.hpp` decl + `value.cpp` def) so every layer can
use it. Applied at the points where a complex result is produced:

- **arithmetic** `+ - .* ./ .^` and the max/min comparator (`elementwiseComplex`),
  **unary** `-`/`+`/`conj` (`unaryComplex`), **matrix multiply** `*` — the
  dominant sources, so `2+0i` / `(1+1i)+(1-1i)` narrow and
  `max([1 -3 2], 2+0i) == [2 2 2]` matches MATLAB.
- **every fused-expression result** (FusionRule execute wrapped via
  `addNarrowing`, keeping fused == per-op).
- **indexing** — scalar `z(k)` (value-layer `elemAt`, covering the VM/TW scalar
  fast paths), ranges/colon-lists, `indexGet2D`/`3D`/`ND`, and `z(:)`
  colon-linearize on both backends.
- **reductions** — `sum`/`prod`/`mean`/`var`/`std` (`runComplexReduction`
  wrapper) and the own-path `cumsum`/`cumprod`/`diff`/`median`.
- **linear algebra** — `dot`/`kron`/`cross`/`diag` (reg-layer wrap).
- **element-reorder** — `fliplr`/`flipud`/`flip`/`rot90`/`circshift`/`repmat`/
  `tril`/`triu`/`repelem`/`paddata`/`trimdata`/`resize` (`manip_reg`).

Guards: `ComplexMathTest.NarrowsArithmeticAllReal`, `.NarrowsIndexingAllRealSlice`,
`.NarrowsResidualOps` (all on both backends), incl. over-narrow checks that a
genuinely complex result of the same ops stays complex.

## Deliberately NOT narrowed — in-place indexed assignment (perf)
`c(i)=v`, `M(i,j)=v`, N-D and `c(:)=v` do NOT narrow, even though MATLAB returns
`isreal=1` for e.g. `c=complex([0 0 0]); c(2)=7`. Narrowing an in-place write
means scanning the imaginary part on every assignment — O(n) per write, hence
O(n²) in an element-fill loop (measured ~250× slower on a 20k array; there is no
O(1) exact test without per-array nonzero-imag bookkeeping). An array that
becomes all-real this way narrows the instant any *operation* consumes it (every
case above), so the divergence is transient. This is the one MATLAB narrowing
case intentionally left unmatched. (An eager scope-guard was prototyped, measured
at ~250×, and reverted.)

## PRESERVE — already correct (match MATLAB, unchanged)
`reshape`, `transpose`/`ctranspose`, concatenation `[z z]` / `cat`, `sort`,
`unique`. A bare `0i` literal also stays complex. Verified `isreal == 0` against
MATLAB R2025b.

## Related (also fixed 2026-06-17)
`trace`, `cummax`, `cummin` used to **throw** "Not a double array" on complex
input. Now supported and narrowing-aware: `trace` sums the complex diagonal
(properties.cpp); `cummax`/`cummin` run a complex magnitude-then-angle fold
(matrix_detail.hpp `cumComplexMinMaxAlongDim`, dim/'reverse'/'omitnan'/
'includenan' all handled). The deeper complex-matrix ops (eig/svd/qr/lu/chol/
det/inv) remain in bugs/linalg/complex-matrix-unsupported.md.

## References
- `src/value/include/numkit/value/value.hpp` + `value.cpp` (`narrowComplex`)
- `src/ops/include/numkit/ops/helpers.hpp` (`elementwiseComplex` / `unaryComplex`)
- `src/core/src/vm.cpp` (`NarrowSlotGuard`), `src/core/src/tree_walker.cpp`
- `src/bundle/src/register/{math/reductions_reg,lang/{matrix_reg,manip_reg},
  linalg/vector_ops_reg,stats/descriptive/descriptive_reg,fusion/fused_rules}.cpp`
- `src/core/tests/complex_math_test.cpp`
- Related: bugs/math/maxmin-complex.md (headline `max([1 -3 2],2+0i)` matches).
