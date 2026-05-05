# stats/movmad — ТЗ for completion

**Status:** open
**Priority:** **critical**
**Effort:** small (joint with the rest of mov* family)
**Audited at commit:** 4f021db
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/moving/moving.cpp:334` (`movmad`)
- Adapter: `libs/stats/src/moving/moving.cpp:518` (`movmad_reg`)
- Spec: `tools/parity/specs/movmad.json`
- Default NaN behaviour: include (poison) — produces extra `0` and
  garbage at NaN-touching windows
- Per-window via `winMadInPlace` = median of |x − median(x)|

## MATLAB R2025b — actual behavior

Same surface as `movmean` (see `audit/findings/stats/movmean.md`).

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | default NaN: numkit returns garbage (mixes NaN into median) | **critical** |
| 2 | `'omitnan'`/`'includenan'`/`Endpoints`/`SamplePoints` all throw | high |

## Reference table (from probe)

Inputs: `A = [1 3 2 5 4 6 NaN 8 7 10]'`, `A2 = (1:9)'`

| Inputs | MATLAB | numkit |
|---|---|---|
| `movmad(A2, 3)` | `[0.5 1 1 1 1 1 1 1 0.5]` | identical ✅ |
| `movmad(A, 3)` default | `[1 1 1 1 1 1 1 0.5 1 1.5]` (omit) | `[1 1 1 1 1 2 NaN 0 1 1.5]` ❌ — wrong at positions 6, 8 |
| `movmad(A, 3, 'omitnan')` | `[1 1 1 1 1 1 1 0.5 1 1.5]` | THROWS |

## Recommended fixes

Apply the joint `parse_mov_extras` helper from
`audit/findings/stats/movmean.md`. The MAD reducer (`winMadInPlace`)
needs to drop NaN entries from `copy` and `devs` before each
`winMedianInPlace` call.

Spec: extend `movmad.json` similarly. `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A — joint fix with the family.

## Closed
- Closed in commit: PENDING (joint mov* family fix)
- Closed date: 2026-05-06
- Notes: nanflag {includemissing|includenan|omitmissing|omitnan} + Endpoints {shrink|discard|fill|scalar} + k=0 error all implemented in libs/stats/src/moving/moving.cpp via shared parseMovExtras helper. SamplePoints/DataVariables/ReplaceValues throw with documented messages.
