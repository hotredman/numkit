# stats.dist/normstat — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 8e48677
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/normal.cpp:129` (`normstat`)
- Adapter: `libs/stats/src/distributions/normal.cpp:200` (`normstat_reg`)
- Spec: `tools/parity/specs/normstat.json`
- `[m, v] = normstat(mu, sigma)` — matches MATLAB exactly

## MATLAB R2025b — actual behavior

`[m, v] = normstat(mu, sigma)` — `m = mu`, `v = sigma²`. Vector
inputs supported (returns same-shape arrays).

## Gaps (numkit vs MATLAB)

**No major gap detected.**

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `[m,v] = normstat(3, 2)` | `m=3, v=4` | identical ✅ |

## Recommended fixes

1. **Spec extension** — add fingerprint with vector inputs (mu and
   sigma both vectors). `tol = 0`.

## Out of scope for this ТЗ

- N/A.
