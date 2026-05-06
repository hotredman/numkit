# stats/binofit — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** bfda361
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/fit/fit.cpp:209` (`binofit`)
- Adapter: `libs/stats/src/fit/fit.cpp:540` (`binofit_reg`)
- Spec: `tools/parity/specs/binofit.json`
- What works today:
  - `[phat, pci] = binofit(x, n[, alpha])`
  - Clopper–Pearson exact CI via beta inversion
  - Vector inputs return `Nx1` `phat` + `Nx2` `pci`
  - Edge: `x=0` ⇒ lower CI = 0; `x=n` ⇒ upper CI = 1; `n<=0` ⇒ NaN

## MATLAB R2025b — actual behavior

Documented signatures (`help binofit`):

- `phat = binofit(x, n)`
- `[phat, pci] = binofit(x, n)`
- `[phat, pci] = binofit(x, n, alpha)`

That is the **complete** MATLAB API. No `cens`, no `freq`, no
`options`. `nargin('binofit') == 3`.

## Gaps (numkit vs MATLAB)

**No behavioural gap detected.** All probed inputs produce
bit-identical (or 1e-9-equivalent) outputs.

The gap this ТЗ flags is in **test coverage** — the spec exercises only
one scalar input and never covers the vector path or the boundary
edges. Since these branches DO exist in the implementation, they should
be guarded.

| # | Coverage gap | Today | Recommendation |
|---|---|---|---|
| 1 | Vector inputs not in spec | only `binofit(7, 10)` | add `[phat, pci] = binofit([3 5 7]', [10 10 10]')` |
| 2 | Edge `x=0` not in spec | not exercised | add `binofit(0, 10)` |
| 3 | Edge `x=n` not in spec | not exercised | add `binofit(10, 10)` |
| 4 | Non-default `alpha` not in spec | only default 0.05 | add `binofit(7, 10, 0.01)` |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `binofit(7, 10)` → `phat` | `0.7000000000` | `0.7000000000` ✅ |
| `binofit(7, 10)` → `pci` | `[0.3475471499; 0.9332604888]` | identical ✅ |
| `binofit([3 5 7]', [10 10 10]')` → `phat` | `[0.30; 0.50; 0.70]` | identical ✅ |
| `binofit([3 5 7]', [10 10 10]')` → `pci(1,:)` | `[0.0667395112 0.6524528501]` | identical ✅ |
| `binofit([3 5 7]', [10 10 10]')` → `pci(3,:)` | `[0.3475471499 0.9332604888]` | identical ✅ |
| `binofit(0, 10)` → `phat`, `pci` | `0; [0; 0.3084971078]` | identical ✅ |
| `binofit(10, 10)` → `phat`, `pci` | `1; [0.6915028922; 1]` | identical ✅ |

## Recommended fixes

1. **Spec extension** — extend `binofit.json` to cover the four
   coverage gaps above. Suggested expression block:
   ```
   k = 7; n = 10; xV = [3 5 7]'; nV = [10 10 10]';
   [ph_s, pci_s] = binofit(k, n);
   [ph_v, pci_v] = binofit(xV, nV);
   [ph_z, pci_z] = binofit(0, 10);
   [ph_f, pci_f] = binofit(10, 10);
   [ph_a, pci_a] = binofit(k, n, 0.01);
   ```
   Fingerprint:
   `[ph_s, pci_s(1), pci_s(2),`
   ` ph_v(1), ph_v(2), ph_v(3), pci_v(1,1), pci_v(1,2), pci_v(3,1), pci_v(3,2),`
   ` ph_z, pci_z(1), pci_z(2),`
   ` ph_f, pci_f(1), pci_f(2),`
   ` ph_a, pci_a(1), pci_a(2)]`. `tol = 1e-9`.
2. **Smoke test (optional):** existing smoke is sufficient if it
   already covers the scalar form. Adding the four edges to the smoke
   would reduce regression risk.
3. **PROGRESS.md row update:** unchanged — comment is already
   accurate.

## Out of scope for this ТЗ

- `Method` keyword (`'Wilson'`, etc.) — MATLAB's `binofit` does not
  support method selection; it's hard-coded to Clopper–Pearson.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-06
- Notes: No code change needed (impl already matched). Parity spec
  extended to cover scalar / vector / x=0 / x=n / alpha=0.01 paths.
  7 TEST_F gtest + smoke .m added. Parity OK numkit ↔ MATLAB ↔ Octave
  across all 19 fingerprint values.
