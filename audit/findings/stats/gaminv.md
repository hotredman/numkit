# stats.dist/gaminv — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 301e5a5
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.**

## Reference table

| Inputs | MATLAB | numkit |
|---|---|---|
| `gaminv(0.5, 2, 1)` | `1.6783469900` | identical ✅ |

## Recommended fixes

1. **Spec extension** — fingerprint over (a, b) variations. `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A.
