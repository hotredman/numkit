# stats/raylfit — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** bfda361
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/fit/fit.cpp:255` (`raylfit`)
- Adapter: `libs/stats/src/fit/fit.cpp:552` (`raylfit_reg`)
- Spec: `tools/parity/specs/raylfit.json`
- What works today:
  - `[shat, sci] = raylfit(x[, alpha])`
  - `σ̂ = √(Σx²/(2N))`; CI from chi² inversion on `2N·σ̂²`
  - Edge: `N==0` ⇒ `NaN`

## MATLAB R2025b — actual behavior

Documented signatures (`help raylfit`):

- `raylfit(data, alpha)`
- `[phat, pci] = raylfit(data, alpha)`

`nargin('raylfit') == 2`. Probe attempts to add `cens` or `freq` returned
`Too many input arguments.` — confirming the surface is exactly two args.

## Gaps (numkit vs MATLAB)

**No behavioural gap detected.**

Test-coverage gap only:

| # | Coverage gap | Recommendation |
|---|---|---|
| 1 | Non-default `alpha` | add `raylfit(x, 0.01)` |
| 2 | Empty input | add `raylfit([])` |
| 3 | Single-element input | add `raylfit([2.5]')` |

## Reference table (from probe)

Inputs:
```
x = [1.5 2.3 0.8 3.1 1.7 2.0 1.4 2.8 1.9 2.2]'
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `[s, sc] = raylfit(x)` | `1.4650938536; [1.1208834401; 2.1156973340]` | identical ✅ |
| `raylfit(x, 0.05, [], freq7)` | ERR: too many args | (would silently accept since adapter doesn't check) |

Note: numkit's `raylfit_reg` reads `args[1]` as alpha but doesn't error
on extra args, only ignores them. This is a soft-divergence — a script
that wrongly passes `cens`/`freq` will not get the MATLAB error. Could
be flagged as a separate small fix (strict nargin check), but not
mandatory for parity.

## Recommended fixes

1. **Spec extension** — add the three coverage gaps. Fingerprint:
   `[s_b, sc_b(1), sc_b(2),`
   ` s_a, sc_a(1), sc_a(2),`
   ` s_one]`. `tol = 1e-9`.
2. **(Optional) Strict-nargin check:** make the adapter throw on
   `args.size() > 2` to match MATLAB's `Too many input arguments`. Low
   priority; numkit's lax behaviour matches Octave.
3. **PROGRESS.md row update:** unchanged.

## Out of scope for this ТЗ

- A `cens`/`freq` form for Rayleigh — MATLAB does not have one.
