# stats/poissfit — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** bfda361
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/fit/fit.cpp:86` (`poissfit`)
- Adapter: `libs/stats/src/fit/fit.cpp:490` (`poissfit_reg`)
- Spec: `tools/parity/specs/poissfit.json`
- What works today:
  - `[lhat, lci] = poissfit(x[, alpha])`
  - `lambda = mean(x)`; exact CI via Garwood (chi² inversion on `2S`)
  - Edge: `N==0` ⇒ `NaN`; `S==0` ⇒ `lo=0`

## MATLAB R2025b — actual behavior

Documented signatures (`help poissfit`):

- `lambdahat = poissfit(data)`
- `[lambdahat, lambdaci] = poissfit(data)`
- `[lambdahat, lambdaci] = poissfit(data, alpha)`

`nargin('poissfit') == 2`. **No `cens`/`freq`/`options`** in MATLAB's
documented surface.

## Gaps (numkit vs MATLAB)

**No behavioural gap detected.**

The gap this ТЗ flags is in **test coverage** — the spec uses one input
and never exercises:

| # | Coverage gap | Recommendation |
|---|---|---|
| 1 | All-zero data | add `poissfit([0 0 0 0]')` — exercises `S==0` branch |
| 2 | Non-default `alpha` | add `poissfit(x, 0.01)` |
| 3 | Empty input | add `poissfit([])` (should return `NaN`) |

## Reference table (from probe)

Inputs:
```
x = [3 4 5 4 5 6 7 5 4 3 5 6 4 3 5]'
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `[lh, lci] = poissfit(x)` | `4.6; [3.5790745705; 5.8215944063]` | identical ✅ |
| `[lh, lci] = poissfit([0 0 0 0]')` | `0; [0; 0.9222198635]` | identical ✅ |

## Recommended fixes

1. **Spec extension** — add the three coverage gaps. Fingerprint:
   `[lh_b, lci_b(1), lci_b(2),`
   ` lh_z, lci_z(1), lci_z(2),`
   ` lh_a, lci_a(1), lci_a(2)]`. `tol = 1e-9`.
2. **PROGRESS.md row update:** unchanged.

## Out of scope for this ТЗ

- A `freq`-weighted form (would be useful for histogrammed Poisson
  counts) — MATLAB does not document one; treating it as missing here
  would be padding the API beyond MATLAB's contract.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Pure spec coverage, no impl change. Spec extended from
  3 to 12 fingerprints across 4 cases (basic + all-zero + α=0.01
  + empty). Parity OK numkit ↔ MATLAB at tol=1e-9. Octave's
  poissfit has its own bug on empty input (`'lb' undefined`); we
  follow MATLAB. 4 TEST_F gtest + smoke.
