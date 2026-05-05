# stats/movprod — ТЗ for completion

**Status:** open
**Priority:** **critical**
**Effort:** small (joint with the rest of mov* family)
**Audited at commit:** 4f021db
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/moving/moving.cpp:284` (`movprod`)
- Adapter: `libs/stats/src/moving/moving.cpp:482` (`movprod_reg`)
- Spec: `tools/parity/specs/movprod.json`
- Default NaN behaviour: include (poison) ✗

## MATLAB R2025b — actual behavior

Same surface as `movmean` (see `audit/findings/stats/movmean.md`).

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | default NaN: numkit poisons, MATLAB omits | **critical** |
| 2 | `'omitnan'`/`'includenan'`/`Endpoints`/`SamplePoints` all throw | high |

## Reference table (from probe)

Inputs: `A = [1 3 2 5 4 6 NaN 8 7 10]'`, `A2 = (1:9)'`

| Inputs | MATLAB | numkit |
|---|---|---|
| `movprod(A2, 3)` | `[2 6 24 60 120 210 336 504 72]` | identical ✅ |
| `movprod(A, 3)` default | `[3 6 30 40 120 24 48 56 560 70]` (omit) | `[3 6 30 40 120 NaN NaN NaN 560 70]` ❌ |

## Recommended fixes

Apply the joint `parse_mov_extras` helper from
`audit/findings/stats/movmean.md`. Mirror change to `winProd`
(skip NaN factors when omitnan).

Spec: extend `movprod.json` similarly. `tol = 1e-9` (catastrophic
cancellation possible for very large products).

## Out of scope for this ТЗ

- N/A — joint fix with the family.
