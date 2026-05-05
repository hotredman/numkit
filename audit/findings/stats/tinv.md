# stats.dist/tinv — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 8e48677
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/students_t.cpp` (`tinv`)
- Spec: `tools/parity/specs/tinv.json`
- `x = tinv(p, nu)` — matches MATLAB exactly.

## MATLAB R2025b — actual behavior

`x = tinv(p, nu)`.

## Gaps (numkit vs MATLAB)

**No major gap detected.**

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `tinv(0.975, 5)` | `2.5705818356` | identical ✅ |
| `tinv(0.5, 10)` | `0` | `0` ✅ |

## Recommended fixes

1. **Spec extension** — add fingerprint with `nu = Inf` (Gaussian
   limit), edge p (0, 1), small-nu (1, 2). `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A.
