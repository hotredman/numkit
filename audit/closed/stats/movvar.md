# stats/movvar — ТЗ for completion

**Status:** open
**Priority:** **critical**
**Effort:** small (joint with the rest of mov* family)
**Audited at commit:** 4f021db
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/moving/moving.cpp:311` (`movvar`)
- Adapter: `libs/stats/src/moving/moving.cpp:498` (`movvar_reg`)
- Spec: `tools/parity/specs/movvar.json`
- What works today:
  - `M = movvar(X, k[, w[, dim]])` — w is normFlag (0 = N-1, 1 = N)
  - Default w=0 (sample variance, divide by N-1)
  - Throws on `w != 0 && w != 1`
- Default NaN behaviour: include (poison) ✗

## MATLAB R2025b — actual behavior

Documented signatures (`help movvar`):

- `M = movvar(A, k)`
- `M = movvar(A, [kb kf])`
- `M = movvar(___, w)` — `w` is normFlag (0 default = N-1, 1 = N) OR
  a weight vector (length N) for weighted variance
- `M = movvar(___, w, dim)`
- `M = movvar(___, nanflag)` — `'omitnan'` (default) / `'includenan'`
- `M = movvar(___, Name, Value)` — `Endpoints`, `SamplePoints`

The third positional may be either a scalar (normFlag) or a vector
(weights). numkit only accepts scalar.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | default NaN | `omitnan` | `includenan` (poisoned) | **critical** |
| 2 | `'omitnan'`/`'includenan'` explicit | — | adapter calls `args[2].toScalar()` ⇒ throws when nanflag follows k | high |
| 3 | `w` as weight vector | weighted moving variance | only scalar normFlag accepted; `args[2].toScalar()` throws on vector | medium |
| 4 | `'Endpoints', 'discard'` etc. | — | throws | high |
| 5 | `'SamplePoints', t` | non-uniform | throws | medium |

## Reference table (from probe)

Inputs: `A = [1 3 2 5 4 6 NaN 8 7 10]'`, `A2 = (1:9)'`

| Inputs | MATLAB | numkit |
|---|---|---|
| `movvar(A2, 3)` (w=0) | `[0.5 1 1 1 1 1 1 1 0.5]` | identical ✅ |
| `movvar(A2, 3, 1)` (w=1) | `[0.25 0.667 0.667 0.667 0.667 0.667 0.667 0.667 0.25]` | identical ✅ |
| `movvar(A2, 3, 0, 1)` (dim=1) | `[0.5 1 1 1 1 1 1 1 0.5]` | identical ✅ |
| `movvar(A, 3, 0, 'omitnan')` | (skip-NaN per window) | THROWS `Cannot convert char to scalar` |
| `movvar(A2, 3, 0, 'Endpoints', 'discard')` | length 7 | THROWS |

## Recommended fixes

Apply the joint `parse_mov_extras` helper from
`audit/findings/stats/movmean.md`, but with one extra wrinkle: the
3rd positional may be `w` (scalar OR vector). The adapter must
distinguish via `isChar()` — strings start the N-V/nanflag tail —
from scalar/vector data.

Variance reducer (`winVar`) needs an omitnan path: drop NaN entries
before computing `mean` and `sum_of_squares`, adjust denominator.

Weighted-variance branch: when `w` is a vector of length N, compute
`mean = Σ w_i x_i / Σ w_i` and `var = Σ w_i (x_i - mean)² / Σ w_i`
(or `Σ w_i (x_i - mean)² / (Σ w_i - 1)` for normFlag=0).

Spec: extend `movvar.json` with w=0/w=1, dim, omitnan, Endpoints.
`tol = 1e-9`.

## Out of scope for this ТЗ

- The weighted-variance branch is a sizeable extension; if it
  doesn't ship in the same fix, leave a follow-up ticket.

## Closed
- Closed in commit: PENDING (joint mov* family fix)
- Closed date: 2026-05-06
- Notes: nanflag {includemissing|includenan|omitmissing|omitnan} + Endpoints {shrink|discard|fill|scalar} + k=0 error all implemented in libs/stats/src/moving/moving.cpp via shared parseMovExtras helper. SamplePoints/DataVariables/ReplaceValues throw with documented messages.
