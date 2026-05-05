# stats/movmedian — ТЗ for completion

**Status:** open
**Priority:** **critical**
**Effort:** small (joint with the rest of mov* family)
**Audited at commit:** 4f021db
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/moving/moving.cpp:292` (`movmedian`)
- Adapter: `libs/stats/src/moving/moving.cpp:490` (`movmedian_reg`)
- Spec: `tools/parity/specs/movmedian.json`
- Default NaN behaviour: include (poison) ✗
- Per-window median via `nth_element` on stack/heap copy

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
| `movmedian(A2, 3)` | `[1.5 2 3 4 5 6 7 8 8.5]` | identical ✅ |
| `movmedian(A, 3)` default | `[2 2 3 4 5 5 7 7.5 8 8.5]` (omit) | `[2 2 3 4 5 6 NaN 7 8 8.5]` ❌ |
| `movmedian(A, 3, 'omitnan')` | `[2 2 3 4 5 5 7 7.5 8 8.5]` | THROWS |

## Recommended fixes

Apply the joint `parse_mov_extras` helper from
`audit/findings/stats/movmean.md`. The median reducer needs to drop
NaN entries from `tmp/heap` before `nth_element`.

Spec: extend `movmedian.json` similarly. `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A — joint fix with the family.

## Closed
- Closed in commit: PENDING (joint mov* family fix)
- Closed date: 2026-05-06
- Notes: nanflag {includemissing|includenan|omitmissing|omitnan} + Endpoints {shrink|discard|fill|scalar} + k=0 error all implemented in libs/stats/src/moving/moving.cpp via shared parseMovExtras helper. SamplePoints/DataVariables/ReplaceValues throw with documented messages.
