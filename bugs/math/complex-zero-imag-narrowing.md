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
c = complex([0 0 0]); c(2)=7; isreal(c)% MATLAB 1 ; numkit NOW 1 (indexed-assign)
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
- **indexed assignment** — `c(i)=…`, `M(i,j)=…`, N-D, and `c(:)=…`: a scope
  guard narrows the mutated target on every write path (VM `INDEX_SET` /
  `INDEX_SET_2D` / `INDEX_SET_ND`, TW `execIndexedAssign`). The `isComplex()`
  gate keeps the hot real-assignment paths free; once an array narrows to real,
  later writes skip the scan.

Guards: `ComplexMathTest.NarrowsArithmeticAllReal`, `.NarrowsIndexingAllRealSlice`,
`.NarrowsResidualOps` (all on both backends), incl. over-narrow checks that a
genuinely complex result of the same ops stays complex.

## PRESERVE — already correct (match MATLAB, unchanged)
`reshape`, `transpose`/`ctranspose`, concatenation `[z z]` / `cat`, `sort`,
`unique`. A bare `0i` literal also stays complex. Verified `isreal == 0` against
MATLAB R2025b.

## Related, NOT part of this bug
`trace`, `cummax`, `cummin` currently **throw** "Not a double array" on complex
input (MATLAB accepts them and narrows an all-real result). That is a separate
*complex-unsupported* defect, not a narrowing divergence — tracked separately.

## References
- `src/value/include/numkit/value/value.hpp` + `value.cpp` (`narrowComplex`)
- `src/ops/include/numkit/ops/helpers.hpp` (`elementwiseComplex` / `unaryComplex`)
- `src/core/src/vm.cpp` (`NarrowSlotGuard`), `src/core/src/tree_walker.cpp`
- `src/bundle/src/register/{math/reductions_reg,lang/{matrix_reg,manip_reg},
  linalg/vector_ops_reg,stats/descriptive/descriptive_reg,fusion/fused_rules}.cpp`
- `src/core/tests/complex_math_test.cpp`
- Related: bugs/math/maxmin-complex.md (headline `max([1 -3 2],2+0i)` matches).
