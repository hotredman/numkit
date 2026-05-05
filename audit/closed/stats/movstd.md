# stats/movstd — ТЗ for completion

**Status:** open
**Priority:** **critical**
**Effort:** small (joint with movvar + the rest of mov* family)
**Audited at commit:** 4f021db
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/moving/moving.cpp:323` (`movstd`) — wraps
  `movvar` and applies `sqrt` element-wise
- Adapter: `libs/stats/src/moving/moving.cpp:508` (`movstd_reg`)
- Spec: `tools/parity/specs/movstd.json`
- Default NaN behaviour: include (poison) ✗

## MATLAB R2025b — actual behavior

Same surface as `movvar` (see `audit/findings/stats/movvar.md`):

- `M = movstd(A, k)` / `(A, [kb kf])` / `(___, w)` / `(___, w, dim)`
- `(___, nanflag)` — `'omitnan'` (default) / `'includenan'`
- `(___, 'Endpoints', val)`, `(___, 'SamplePoints', t)`

## Gaps (numkit vs MATLAB)

Identical to `movvar` (see `audit/findings/stats/movvar.md`). Since
`movstd` delegates to `movvar`, fixing the underlying impl + adapter
fixes both.

## Reference table (from probe)

Inputs: `A2 = (1:9)'`

| Inputs | MATLAB | numkit |
|---|---|---|
| `movstd(A2, 3)` (w=0) | `[0.7071 1 1 1 1 1 1 1 0.7071]` | identical ✅ |
| `movstd(A2, 3, 1)` (w=1) | `[0.5 0.8165 0.8165 0.8165 0.8165 0.8165 0.8165 0.8165 0.5]` | identical ✅ |
| (NaN + Endpoints + SamplePoints) | (per `movvar`) | THROWS (per `movvar`) |

## Recommended fixes

Fixing `movvar` (see ТЗ) automatically fixes `movstd`. Spec extension
should mirror `movvar`'s but on std-scale outputs. `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A — joint fix with `movvar` and the family.

## Closed
- Closed in commit: PENDING (joint mov* family fix)
- Closed date: 2026-05-06
- Notes: nanflag {includemissing|includenan|omitmissing|omitnan} + Endpoints {shrink|discard|fill|scalar} + k=0 error all implemented in libs/stats/src/moving/moving.cpp via shared parseMovExtras helper. SamplePoints/DataVariables/ReplaceValues throw with documented messages.
