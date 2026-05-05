# stats.dist/betainv — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 301e5a5
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/beta.cpp` (`betainv`)
- Spec: `tools/parity/specs/betainv.json`
- Matches MATLAB.

## Gaps (numkit vs MATLAB)

**No major gap detected.**

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `betainv(0.5, 2, 3)` | `0.3857275681` | identical ✅ |
| `betainv(0.95, 2, 3)` | `0.7513953743` | identical ✅ |

## Recommended fixes

1. **Spec extension** — fingerprint over (a, b) ∈ {(0.5, 0.5),
   (1, 1), (2, 5), (10, 10)} × p ∈ {0.05, 0.5, 0.95}. `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A.
