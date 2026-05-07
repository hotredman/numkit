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

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-07
- Notes: Same vectorisation gap as betastat. ТЗ said "no major gap"
  but two real fixes needed:
    1. chi2stat_reg was scalar-only (`args[0].toScalar()`) — added
       elementwise vectorisation matching MATLAB's vector contract.
    2. chi2stat(0) returned `(0, 0)` — MATLAB returns NaN/NaN
       (moments undefined for degenerate). Fixed: k <= 0 ⇒ NaN.
  12-fingerprint spec covers scalar / vector / k=0 / k<0. 3 TEST_F
  gtest + smoke .m. Parity OK numkit ↔ MATLAB ↔ Octave.
