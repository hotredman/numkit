# builtin/rat — ТЗ for completion

**Status:** closed
**Priority:** **high** (PROGRESS notes `correctness=MISMATCH`)
**Effort:** small
**Audited at commit:** f82f380
**Audit date:** 2026-05-06

## Gaps

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `[N, D] = rat(X, tol)` 2-output form | returns numerator + denominator integers | numkit returns string output only — `[N, D] = rat(...)` throws "Undefined function or variable 'D'" | **high** |
| 2 | numerical output | continued-fraction expansion ints | string-only output diverges from the bench expectation | high |

## Reference table

| Inputs | MATLAB | numkit |
|---|---|---|
| `[N, D] = rat(pi, 1e-3)` | `N=355, D=113` | THROWS |
| `rat(pi, 1e-3)` 1-output | (returns string `'3 + 1/(7 + 1/16)'`) | (probe needed) |

## Recommended fixes

1. **Implement 2-output `[N, D]`** form: in addition to the string
   form, when called with 2 outputs return the integer numerator
   and denominator as scalars (or vectors if X is a vector).
2. **Spec extension** — add fingerprint over both 1-out (string
   form) and 2-out (numeric) cases.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Joint closure with builtin/rats.md (sibling).

  **Algorithm change**: replaced the simple-CF expansion (using
  `floor()`) with the **regularized** continued-fraction expansion
  (using `round()`). Result: signed CF coefficients matching MATLAB
  R2025b exactly.

  Examples:
  - `rat(0.5)` MATLAB=`'1 + 1/(-2)'`  (NOT `'0 + 1/(2)'` from simple CF)
  - `rat(pi, 1e-3)` MATLAB=`'3 + 1/(7 + 1/(16))'`
  - `rat(2.7183)` MATLAB=`'3 + 1/(-4 + 1/(2 + 1/(5 + 1/(-2 + 1/(-20)))))'`

  **String format**: nested `'a0 + 1/(a1 + 1/(... + 1/(an)))'` instead
  of the prior simple `'p / q'` form.

  **2-output form `[N, D] = rat(x[, tol])`**: now implemented and
  fully vectorised over `x`. Denominator always normalised positive.
  ТЗ reference verified: `[N,D] = rat(pi, 1e-3) → N=355, D=113`.

  **Default tolerance**: `1e-6 * max(1, |x|)` per MATLAB docs
  (`1e-6 * norm(X(:),1)` for vectors, but element-wise behavior
  collapses to per-element scaled tol for the scalar/iterator path).

  **rats sibling fix**: rendered as fixed-width `numerator/denominator`
  field (default len=13, field width = len+1 = 14 with leading sign
  column). Numerator right-justified in cols 1..7, slash at col 8,
  denominator left-justified in cols 9..14 — matches MATLAB R2025b
  layout bit-identically (`strlength(rats(0.5)) = 14`,
  `strfind(rats(pi),'/') = 8`).

  4 artefacts:
  - impl: `libs/builtin/src/language/strings/strings.cpp` —
    new `ratExpansion()` + `buildCFString()` + `defaultRatTol()`
    helpers; rewritten `rat()`, `rats()`, `rat_reg` (now nargout-aware)
  - parity: `tools/parity/specs/{rat,rats}.json` — 17 + 4 fingerprints,
    both correctness=OK against MATLAB R2025b. Octave skipped (no
    `strlength` builtin in current Octave).
  - gtest: `libs/builtin/tests/rat_test.cpp` — 15 tests covering
    2-output (scalar, vector, sign normalisation), 1-output CF string
    (canonical probes incl. Inf/NaN), rats default+custom width.
    Existing `BuiltinTest.Rat` regression test updated to assert the
    new MATLAB-matching strings (was asserting the old wrong format).
  - smoke: `libs/builtin/tests/smoke/rat_smoke.m`

  Octave's `strlength` is not yet implemented (per Octave 11.1.0
  error message), so parity uses `--prefer-matlab` semantics that
  the harness already applies.


