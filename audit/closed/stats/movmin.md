# stats/movmin — ТЗ for completion

**Status:** open
**Priority:** **critical**
**Effort:** small (joint with the rest of mov* family)
**Audited at commit:** 4f021db
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/moving/moving.cpp:268` (`movmin`)
- Adapter: `libs/stats/src/moving/moving.cpp:466` (`movmin_reg`)
- Spec: `tools/parity/specs/movmin.json`
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
| `movmin(A2, 3)` | `[1 1 2 3 4 5 6 7 8]` | identical ✅ |
| `movmin(A, 3)` default | `[1 1 2 2 4 4 6 7 7 7]` (omit) | `[1 1 2 2 4 4 6 NaN 7 7]` (poisoned at NaN window) ❌ |

## Recommended fixes

Apply the joint `parse_mov_extras` helper from
`audit/findings/stats/movmean.md`. Min/max reducer skips NaN by
default in MATLAB; current numkit `winMin` doesn't filter NaN.

Spec: extend `movmin.json` with default-NaN, omitnan, includenan,
Endpoints variants, asymmetric window. `tol = 0`.

## Out of scope for this ТЗ

- N/A — joint fix with the family.

## Closed
- Closed in commit: PENDING (joint mov* family fix)
- Closed date: 2026-05-06
- Notes: nanflag {includemissing|includenan|omitmissing|omitnan} + Endpoints {shrink|discard|fill|scalar} + k=0 error all implemented in libs/stats/src/moving/moving.cpp via shared parseMovExtras helper. SamplePoints/DataVariables/ReplaceValues throw with documented messages.
