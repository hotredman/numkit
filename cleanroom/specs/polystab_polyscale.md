# Clean-room specification — `polystab` & `polyscale`

Two polynomial root-manipulation utilities from the Signal Processing
toolbox. Written per `cleanroom/PROTOCOL.md` (Spec Author role). Both
operate on a polynomial given by its coefficient vector in **descending
power order**: `a = [a0 a1 ... aN-1]` represents
`a0·z^(N-1) + a1·z^(N-2) + ... + aN-1`.

> **Scope.** Functional equivalence to MATLAB R2025b `polystab` /
> `polyscale` over their documented argument sets. Both are textbook
> z-plane operations; the goal is a from-scratch implementation of the
> standard algorithm, not a translation of MATLAB's `.m` files.

## Public references

- A. V. Oppenheim & R. W. Schafer, *Discrete-Time Signal Processing*,
  3rd ed., Pearson, 2010 — z-transform scaling property
  `α^n·x[n] ↔ X(z/α)` (§3.2), and minimum-phase systems: a zero/pole
  at `z = r` can be reflected to its conjugate-reciprocal location
  `z = 1/conj(r)` without changing the magnitude response, only the
  phase (§5.6, "Minimum-Phase Systems").
- J. D. Markel & A. H. Gray, *Linear Prediction of Speech*, Springer,
  1976 — radial root scaling as LPC "bandwidth expansion": replacing
  `A(z)` by `A(z/α)` moves every root by the factor `α`.
- M. H. Hayes, *Statistical Digital Signal Processing and Modeling*,
  Wiley, 1996 — spectral factorisation / stabilisation by reflecting
  roots across the unit circle.

---

## 1. `polyscale` — scale the roots of a polynomial

### 1.1 Algorithm

Scaling every root of a polynomial by a factor `α` is the z-transform
scaling property: if `A(z)` has roots `r_k`, then `A(z/α)` has roots
`α·r_k`. In coefficient form, substituting `z → z/α` multiplies the
coefficient of `z^(N-1-k)` by `α^k`. Hence:

```
b[k] = a[k] · α^k          for k = 0 .. N-1
```

where `N` is the polynomial length. With `α` real in `(0,1)` the roots
contract radially toward the origin (LPC bandwidth expansion); `α > 1`
expands them; complex `α` rotates as well as scales.

### 1.2 Argument set & shapes

`b = polyscale(a, alpha)`

- **`a`** — polynomial coefficients. Either a vector (row or column,
  one polynomial of length `n = numel(a)`) or an `m × n` matrix whose
  **each row** is a separate length-`n` polynomial.
- **`alpha`** — the scaling factor. Either a scalar, or a row vector of
  length `n` (a per-coefficient factor — element `k` is raised to the
  power `k`). Any other `alpha` length (≠ 1 and ≠ n) → error
  `m:polyscale:BadScale`.

Output `b`:
- Vector `a` → **row vector** `1 × n`.
- Matrix `a` (`m × n`) → `m × n`, each row scaled independently.
- Per element: `b(i,k) = a(i,k) · p(k)` where the power factor is
  `p(k) = alpha^k` (scalar `alpha`) or `p(k) = alpha(k)^k` (vector
  `alpha`), `k = 0 .. n-1`.

Edge cases:
- Empty `a` → empty (`0 × 0`).
- Scalar `a` (`n = 1`) → returns `a` unchanged (`α^0 = 1`).

Type: the result is **complex** if either `a` or `alpha` is complex,
otherwise **real** (`double`).

> Documented divergence from MATLAB: MATLAB computes
> `a .* alpha.^(0:length(a)-1)` and, for a *column*-vector `a`, its
> implicit expansion yields an `n × n` matrix. numkit instead treats
> any vector `a` as a single polynomial and returns a `1 × n` row.
> This is the conventional polynomial representation; record it in
> PROGRESS.md and keep the column-vector case out of the parity
> fingerprint.

---

## 2. `polystab` — stabilise a polynomial

### 2.1 Algorithm

`polystab` returns a polynomial with the **same magnitude frequency
response** as the input but with every root moved to (or kept) inside
the unit circle — the minimum-phase version of the polynomial.

A root `r` with `|r| > 1` is reflected to its **conjugate reciprocal**
`1 / conj(r)`, which has magnitude `1/|r| < 1` and the same angle as
`r`. Reflecting a root across the unit circle this way changes only
the phase response, not the magnitude (Oppenheim & Schafer §5.6).

Procedure:

1. Compute the roots `r_k` of `a` (numkit's own `roots`).
2. For each root: if `|r_k| > 1`, replace it with `1 / conj(r_k)`;
   otherwise keep it. (Roots on or inside the unit circle are kept;
   a root exactly on the circle satisfies `1/conj(r) = r`, so the
   boundary is a no-op either way. A zero root stays zero.)
3. Rebuild the polynomial from the stabilised roots (numkit's own
   `poly`), giving a monic coefficient vector `p`.
4. Restore the original gain: multiply `p` by the **first non-zero
   coefficient** of `a`. (Leading zeros in `a` are not significant —
   `roots` ignores them — so the gain is taken from the first
   coefficient that is actually non-zero.)
5. If `a` is real, the stabilised polynomial is real (reflection maps
   conjugate root pairs to conjugate root pairs); take the real part
   to drop any imaginary round-off.

### 2.2 Argument set & shapes

`b = polystab(a)`

- **`a`** — polynomial coefficients, a **vector** (row or column).
  A matrix input → error `m:polystab:notVector` ("Input must be a
  vector.").
- Output `b` — always a **row vector**.

Edge cases:
- Empty `a` → empty (`0 × 0`).
- Scalar `a` → returned unchanged (no roots to move).
- `a` real → `b` real; `a` complex → `b` complex.
- All roots already inside the unit circle → `b` equals `a` (up to
  floating-point round-off from the roots→poly round trip).

---

## 3. Interface (numkit)

Unchanged from the existing header
`numkit/signal/digital_filtering/poly_utils.hpp`:

```cpp
Value polyscale(const Value &p, const Value &scale,
                std::pmr::memory_resource *mr = nullptr);
Value polystab (const Value &a,
                std::pmr::memory_resource *mr = nullptr);
```

- Error identifiers: keep `m:polyscale:BadScale`, `m:polyscale:nargin`,
  `m:polystab:nargin`; add `m:polystab:notVector` for matrix input.
- PMR HARD RULE: scratch via `ScratchArena` / `ScratchVec<T>`; the
  returned `Value` is allocated on `mr`.
- `polystab` may call numkit's own `builtin::roots` and `builtin::poly`
  — these are numkit's first-party code and the root→poly round trip
  is the textbook construction, not a MATLAB translation.

## 4. Verification

- gtest `libs/signal/tests/polyutils_test.cpp` — extend with the
  new branches (row-vector `alpha`, matrix `a`, complex paths,
  matrix-input error). Re-baseline hardcoded values only where the
  clean-room result genuinely differs.
- Parity `tools/parity/specs/signal_polyscale.json` +
  `signal_polystab.json` (or one combined spec) — require
  `correctness = OK` against MATLAB R2025b. Octave 11.1.0 does not
  ship either function in core — expect `N/A` there.
- **MATLAB-independent correctness test** (mandatory): after
  `polystab`, every root of `b` must satisfy `|root| ≤ 1`, and
  `abs(freqz(b))` must equal `abs(freqz(a))` (magnitude response
  preserved). For `polyscale`, `roots(polyscale(a,α))` must equal
  `α · roots(a)`.

## Constraints for the Implementer

- Do **not** open `libs/signal/src/digital_filtering/poly_utils.cpp`.
- Do **not** consult MATLAB `.m` source (`polystab.m`, `polyscale.m`)
  or any third-party reference implementation.
- Implement solely from this specification and the cited public
  references.
