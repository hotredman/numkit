# builtin/rat — ТЗ for completion

**Status:** open
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
