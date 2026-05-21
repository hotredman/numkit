# Clean-room specification — `scaleFilterSections`

Distribute scale values across the sections of a cascaded-transfer-
function (CTF) numerator. Written per `cleanroom/PROTOCOL.md` (Spec
Author role).

> **Scope.** Functional equivalence to MATLAB R2025b
> `scaleFilterSections`. The operation is elementary algebra (per-row
> scalar multiplication); the goal is an independent implementation of
> the documented behaviour. The rewrite also lifts a gap in the current
> code — complex-valued numerator coefficients are supported.

## Public references

- L. B. Jackson, *Digital Filters and Signal Processing*, 3rd ed.,
  Kluwer, 1996 — cascade (series) realisation of IIR filters and the
  distribution of an overall gain across the cascaded sections.
- A. V. Oppenheim & R. W. Schafer, *Discrete-Time Signal Processing*,
  3rd ed., 2010 — §6.3, cascade-form filter structures: a cascade of
  `K` sections has an overall transfer function equal to the product
  of the section transfer functions.
- MATLAB R2025b `help scaleFilterSections` — the documented interface:
  `Bg = scaleFilterSections(B, g)` scales the sections of the CTF
  numerator `B` by the scale values `g` (scalar or vector).

## 1. Background

A cascaded-transfer-function (CTF) filter is a series of `K` sections;
its overall numerator is the **product** of the per-section numerator
polynomials. `scaleFilterSections` redistributes a desired gain across
those sections without changing the overall product more than intended
— useful for fixed-point range management.

`B` is a `K × Q` matrix: row `k` (`k = 0 … K-1`) is the length-`Q`
numerator polynomial of cascade section `k`. A plain row vector is the
single-section case `K = 1`.

## 2. Algorithm

`Bg = scaleFilterSections(B, g)`

Let `K` be the number of cascade sections (rows of `B`; `K = 1` for a
row-vector `B`).

### 2.1 Scalar `g`

Distribute the gain evenly across all `K` sections by giving each the
`K`-th root of its magnitude, and concentrate the sign on the **last**
section:

```
root = |g|^(1/K)
for k = 0 … K-1:
    Bg[k, :] = root * B[k, :]
Bg[K-1, :] *= sign(g)
```

Because `(|g|^(1/K))^K · sign(g) = g`, the overall cascade product is
multiplied by exactly `g`.

### 2.2 Vector `g` (length `K + 1`)

The first `K` entries are per-section scale factors; the `(K+1)`-th
entry `g[K]` is an additional overall gain, distributed as a `K`-th
root like the scalar case:

```
root = |g[K]|^(1/K)
for k = 0 … K-1:
    Bg[k, :] = root * g[k] * B[k, :]
Bg[K-1, :] *= sign(g[K])
```

### 2.3 `sign`

`sign(x)` is the MATLAB sign: for real `x` it is `+1 / 0 / -1`; for a
complex `x` it is `x / |x|` (and `0` when `x == 0`). For real scale
values — the common case — this reduces to `±1` / `0`.

### 2.4 Shapes, types, errors

- Output `Bg` has the **same shape** as `B` (`K × Q`, or `1 × Q`).
- `B` may be real or complex; `Bg` is complex iff `B` or `g` is
  complex, otherwise real (`double`).
- `g` must be a **scalar** or a **vector of length `K + 1`**; any other
  length raises `m:scaleFilterSections:invalidNumberOfScaleValues`
  ("Invalid number of scale values. Specify either a scalar or a
  vector of length equal to the number of sections + 1.").
- An all-ones `g` leaves `B` unchanged (it is the identity of the
  formula — an explicit fast path is optional, not required).

## 3. numkit interface

Unchanged from the existing header
`numkit/signal/filter_implementation/conversions_extras.hpp`:

```cpp
Value scaleFilterSections(const Value &CTFNum, const Value &SV,
                          std::pmr::memory_resource *mr = nullptr);
```

- Keep the error id `m:scaleFilterSections:invalidNumberOfScaleValues`
  (and `m:scaleFilterSections:nargin` in the registration layer).
- PMR HARD RULE: any scratch via `ScratchArena` / `ScratchVec<T>`; the
  returned `Value` is allocated on `mr`. (This function needs almost no
  scratch — it can read inputs element-wise and write the result
  directly.)

## 4. Verification

- gtest `libs/signal/tests/ctfutils_test.cpp` — keep the existing
  scaleFilterSections cases (scalar `g`, vector `g`, all-ones), and add
  the single-section `K = 1` case and a complex-`B` case. Hardcoded
  expected values are MATLAB R2025b reference output.
- Parity `tools/parity/specs/signal_scalefiltersections.json` — new
  spec; require `correctness = OK` vs MATLAB R2025b. Octave 11.1.0 does
  not ship `scaleFilterSections` — expect `N/A` there.
- **MATLAB-independent correctness test** (mandatory): the defining
  property is that the cascade product is scaled by exactly `g`. Build
  `B`, scale it, and verify `prod over k of polynomial(Bg[k]) ==
  g · prod over k of polynomial(B[k])` — concretely, convolve the rows
  of `B` together and the rows of `Bg` together and assert
  `conv(Bg rows) == g · conv(B rows)` to floating-point tolerance.

## Constraints for the Implementer

- Do **not** open `libs/signal/src/filter_implementation/conversions_extras.cpp`.
- Do **not** consult MATLAB `.m` source (`scaleFilterSections.m`,
  `scalectfnum.m`) or any third-party reference implementation.
- Implement solely from this specification and the cited public
  references.
