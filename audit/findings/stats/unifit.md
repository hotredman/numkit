# stats/unifit — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** bfda361
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/fit/fit.cpp:119` (`unifit`)
- Adapter: `libs/stats/src/fit/fit.cpp:514` (`unifit_reg`)
- Spec: `tools/parity/specs/unifit.json`
- What works today:
  - `[ahat, bhat, ACI, BCI] = unifit(x[, alpha])`
  - MLE = (min, max); CI extension `delta = (b-a)·(α^(-1/N) − 1)`
  - Edge: `N==0` ⇒ all `NaN`

## MATLAB R2025b — actual behavior

Documented signatures (`help unifit`):

- `[ahat, bhat] = unifit(data)`
- `[ahat, bhat, ACI, BCI] = unifit(data)`
- `[ahat, bhat, ACI, BCI] = unifit(data, alpha)`

`nargin('unifit') == 2`. Probe with extra args returned
`Too many input arguments.`

## Gaps (numkit vs MATLAB)

**No behavioural gap detected.**

Test-coverage gap only:

| # | Coverage gap | Recommendation |
|---|---|---|
| 1 | Non-default `alpha` | add `unifit(x, 0.01)` |
| 2 | Empty input | add `unifit([])` |
| 3 | Single-element input | add `unifit([5])` |

## Reference table (from probe)

Inputs:
```
x = [2 5 3 7 4 6 8 1 9 5]'
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `[a, b, aci, bci] = unifit(x)` | `a=1; b=9; aci=[-1.7942627814; 1]; bci=[9; 11.7942627814]` | identical ✅ |

## Recommended fixes

1. **Spec extension** — add the three coverage gaps. Fingerprint:
   `[a_b, b_b, aci_b(1), bci_b(2),`
   ` a_a, b_a, aci_a(1), bci_a(2),`
   ` a_one, b_one]`. `tol = 1e-9`.
2. **(Optional) Strict-nargin check:** see `raylfit` ТЗ for the same
   suggestion. Low priority.
3. **PROGRESS.md row update:** unchanged.

## Out of scope for this ТЗ

- A `cens`/`freq` form for the uniform — MATLAB does not have one.
