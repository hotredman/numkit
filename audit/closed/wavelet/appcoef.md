# wavelet/appcoef — ТЗ for completion

**Status:** closed (boundary modes other than 'sym' deferred)
**Priority:** medium
**Effort:** small
**Audited at commit:** 0e895fe
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/dwt/multilevel.cpp:136` (`appcoef`)
- Adapter: `libs/wavelet/src/dwt/multilevel.cpp:242` (`appcoef_reg`)
- Spec: `tools/parity/specs/appcoef.json`
- What works today:
  - `A = appcoef(c, l, wname[, level])` — extract approximation
  - `level=0` returns the full reconstruction

## MATLAB R2025b — actual behavior

Documented signatures (`help appcoef`):

- `A = appcoef(c, l, wname)`
- `A = appcoef(c, l, LoR, HiR)`
- `A = appcoef(___, N)` — extract at level N
- `A = appcoef(___, Mode=extmode)` — boundary mode

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | numeric values | follow MATLAB dwt convention | inherits dwt mismatch | medium (cascade from dwt) |
| 2 | `(c, l, LoR, HiR)` custom synthesis filters | supplied pair | not supported | high |
| 3 | `Mode=extmode` N-V | match analysis mode | silently ignored | high |

## Reference table (from probe)

Inputs: `[c, l] = wavedec(x, 3, 'db2')` for x = `[1..16]'`

| Inputs | MATLAB-derived | numkit |
|---|---|---|
| default level (= max = n=3) | (probed on randn) | head matches numkit's wavedec output `[8.09, 32.79, 44.53, 42.45]` |
| level=1 | (probed on randn) | head numkit `[1.77, 4.76, 7.59, 10.42, 13.25, 16.07]` |

## Recommended fixes

1. **Cascade fix from `dwt`/`idwt`:** numeric values become
   MATLAB-correct once dwt/idwt land.
2. **Accept `(c, l, LoR, HiR)`** custom-filter form.
3. **Add `Mode=` N-V parsing.**
4. **Spec extension:** add fingerprint for default-level and level=1
   (after the cascade fix). `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commits: 32ab3ce0 (wfilters cascade), 6a3b1fe8 (idwt
  helper exposed), this commit (custom-filter form + Mode N-V).
- Closed date: 2026-05-08
- Notes: Gap #1 (numeric values) closed via the wfilters
  Lo_D/Lo_R label-swap cascade (32ab3ce0). appcoef now bit-
  identical to MATLAB R2025b at default and per-level extraction.

  Gap #2 (custom `(LoR, HiR)` filter form) closed via new
  `appcoef_with_filters()` helper that uses `idwt_with_filters_pub`
  (exported from dwt.cpp) instead of going through wname lookup
  on every cascade step.

  Gap #3 (`Mode=`/`'mode'` N-V) parsed; only 'sym' supported,
  others throw clear "not yet implemented" error.

  4 artefacts shipped (impl + 13-fp parity spec + 7 gtests + smoke
  via wfilters_smoke.m which already exercises wavedec). Bit-
  identical numkit ↔ MATLAB on all 13 fingerprints. Octave doesn't
  ship `wavedec` so parity is numkit ↔ MATLAB only.

  Boundary modes 'per' / 'zpd' / 'ppd' / etc. remain DEFERRED
  (same scope decision as the dwt/idwt cascade closures).

