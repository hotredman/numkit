# signal/tukeywin — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 69ef496
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/windows/windows.cpp` (`tukeywin`)
- Adapter: `libs/signal/src/windows/windows.cpp:545` (`tukeywin_reg`)
- Spec: `tools/parity/specs/tukeywin.json`
- What works today:
  - `w = tukeywin(N[, r])` — default `r=0.5`
  - r-edge cases (r=0 → rectwin, r=1 → hann) handled correctly

## MATLAB R2025b — actual behavior

Documented signatures (`help tukeywin`):

- `w = tukeywin(L, r)` — `r` is the cosine fraction (0 ≤ r ≤ 1)

## Gaps (numkit vs MATLAB)

**No major gap detected.** All probed cases match.

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `tukeywin(8)` (r=0.5) | `[0 0.6113 1 1 1 1 0.6113 0]` | identical ✅ |
| `tukeywin(8, 0)` | `[1 1 1 1 1 1 1 1]` | identical ✅ |
| `tukeywin(8, 1)` | `[0 0.1883 0.6113 0.9505 0.9505 0.6113 0.1883 0]` | identical ✅ |
| `tukeywin(8, 0.25)` | `[0 1 1 1 1 1 1 0]` | identical ✅ |

## Recommended fixes

1. **Spec extension** — add fingerprint for r ∈ {0, 0.25, 0.5,
   0.75, 1} × N ∈ {8, 16, 64}. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.
