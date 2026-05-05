# stats/movsum — ТЗ for completion

**Status:** open
**Priority:** **critical**
**Effort:** small (joint with the rest of mov* family)
**Audited at commit:** 4f021db
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/moving/moving.cpp:260` (`movsum`)
- Adapter: `libs/stats/src/moving/moving.cpp:458` (`movsum_reg`)
- Spec: `tools/parity/specs/movsum.json`
- What works today:
  - `Y = movsum(X, k[, dim])` with scalar / `[kb kf]` window
  - Default endpoint = shrink ✓
  - Default NaN = include (poison) ✗ — MATLAB default is `omitnan`

## MATLAB R2025b — actual behavior

Same surface as `movmean` (see `audit/findings/stats/movmean.md` for
full N-V description):

- `M = movsum(A, k)` / `(A, [kb kf])` / `(___, dim)`
- `(___, nanflag)` — `'omitnan'` (default) / `'includenan'`
- `(___, 'Endpoints', val)`, `(___, 'SamplePoints', t)`

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | default NaN behaviour | `omitnan` | `includenan` (poisoned) | **critical** |
| 2 | `'omitnan'`/`'includenan'` explicit | — | throws `Cannot convert char to scalar` | high |
| 3 | `'Endpoints', ...` | discard/fill/scalar | throws | high |
| 4 | `'SamplePoints', t` | non-uniform window | throws | medium |

## Reference table (from probe)

Inputs: `A = [1 3 2 5 4 6 NaN 8 7 10]'`, `A2 = (1:9)'`

| Inputs | MATLAB | numkit |
|---|---|---|
| `movsum(A2, 3)` | `[3 6 9 12 15 18 21 24 17]` | identical ✅ |
| `movsum(A, 3)` (default) | `[4 6 10 11 15 10 14 15 25 17]` (omit) | `[4 6 10 11 15 NaN NaN NaN 25 17]` (include) ❌ |
| `movsum(A, 3, 'omitnan')` | `[4 6 10 11 15 10 14 15 25 17]` | THROWS |
| `movsum(A2, 3, 'Endpoints', 'fill')` | `[NaN 6 9 12 15 18 21 24 NaN]` | THROWS |

## Recommended fixes

Apply the joint `parse_mov_extras` helper described in
`audit/findings/stats/movmean.md` "Recommended fixes" §2-§5. The
sum reducer is the simplest case — when `omitnan`, just skip NaN
entries.

Spec: same shape as `movmean.json` extension — basic, asymmetric,
default-NaN, omitnan, includenan, Endpoints variants, SamplePoints.
`tol = 0` (integer-stable).

## Out of scope for this ТЗ

- N/A — this function shares its scaffold with the other mov*.

## Closed
- Closed in commit: PENDING (joint mov* family fix)
- Closed date: 2026-05-06
- Notes: nanflag {includemissing|includenan|omitmissing|omitnan} + Endpoints {shrink|discard|fill|scalar} + k=0 error all implemented in libs/stats/src/moving/moving.cpp via shared parseMovExtras helper. SamplePoints/DataVariables/ReplaceValues throw with documented messages.
