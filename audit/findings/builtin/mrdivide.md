# builtin/mrdivide — ТЗ for completion

**Status:** open
**Priority:** **critical**
**Effort:** medium
**Audited at commit:** 42e1ec3
**Audit date:** 2026-05-06

## Gaps

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `mrdivide(A, B) = A/B` (matrix right division) | solves `X*B = A` for X | THROWS "Matrix right division not implemented" | **critical** — core MATLAB matrix op |

## Reference table

| Inputs | MATLAB | numkit |
|---|---|---|
| `mrdivide([1 2; 3 4], [5 6; 7 8])` | `[3 -2; 2 -1]` | THROWS |

## Recommended fixes

1. **Implement matrix right division.** Standard implementation:
   `X = A/B` is equivalent to `(B'\A')'`. So:
   - Compute `B'\A'` via `mldivide(B', A')` (LU/QR-based).
   - Transpose the result.
   numkit appears to have `mldivide` partially (probe interrupted)
   — verify and use as the primitive.
2. **Spec extension** — add fingerprint over square (LU path),
   tall (QR path), wide (least-squares), and singular cases.
   `tol = 1e-10`.

## Out of scope for this ТЗ

- The page-wise version `pagemrdivide` (already ❌ in PROGRESS).
