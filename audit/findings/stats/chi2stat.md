# stats.dist/chi2stat — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 8e48677
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/chi2.cpp` (`chi2stat`)
- Spec: `tools/parity/specs/chi2stat.json`
- `[m, v] = chi2stat(k)` — `m = k`, `v = 2k`. Matches MATLAB.

## MATLAB R2025b — actual behavior

`[m, v] = chi2stat(nu)`. Vector inputs supported.

## Gaps (numkit vs MATLAB)

**No major gap detected.**

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `chi2stat(5)` | `m=5, v=10` | identical ✅ |

## Recommended fixes

1. **Spec extension** — add fingerprint with vector `nu`. `tol = 0`.

## Out of scope for this ТЗ

- N/A.
