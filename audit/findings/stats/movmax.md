# stats/movmax — ТЗ for completion

**Status:** open
**Priority:** **critical**
**Effort:** small (joint with the rest of mov* family)
**Audited at commit:** 4f021db
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/moving/moving.cpp:276` (`movmax`)
- Adapter: `libs/stats/src/moving/moving.cpp:474` (`movmax_reg`)
- Spec: `tools/parity/specs/movmax.json`
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
| `movmax(A2, 3)` | `[2 3 4 5 6 7 8 9 9]` | identical ✅ |
| `movmax(A, 3)` default | `[3 3 5 5 6 6 8 8 10 10]` (omit, inferred) | `[3 3 5 5 6 NaN NaN NaN 10 10]` ❌ |
| `movmax(A, 3, 'includenan')` | `[3 3 5 5 6 NaN NaN NaN 10 10]` | THROWS |

## Recommended fixes

Apply the joint `parse_mov_extras` helper from
`audit/findings/stats/movmean.md`. Mirror change to `winMax`.

Spec: extend `movmax.json` with default-NaN, omitnan, includenan,
Endpoints variants. `tol = 0`.

## Out of scope for this ТЗ

- N/A — joint fix with the family.
