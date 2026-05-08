# signal/gausswin — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 69ef496
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/windows/windows.cpp` (`gausswin`)
- Adapter: `libs/signal/src/windows/windows.cpp:563` (`gausswin_reg`)
- Spec: `tools/parity/specs/gausswin.json`
- What works today:
  - `w = gausswin(N[, alpha])` — default `alpha=2.5`
  - All probed cases match MATLAB exactly

## MATLAB R2025b — actual behavior

Documented signatures (`help gausswin`):

- `w = gausswin(L)` — `alpha = 2.5` default
- `w = gausswin(L, alpha)` — `alpha` is the reciprocal of the
  standard deviation

## Gaps (numkit vs MATLAB)

**No major gap detected.** All 2 probed values match exactly.

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `gausswin(8)` (alpha=2.5) | `[0.0439 0.2030 0.5633 0.9382 0.9382 0.5633 0.2030 0.0439]` | identical ✅ |
| `gausswin(8, 4)` | `[3.35e-4 0.0169 0.2301 0.8494 0.8494 0.2301 0.0169 3.35e-4]` | identical ✅ |

## Recommended fixes

1. **Spec extension** — add fingerprint for several alpha values
   (1.5, 2.5, 4, 8) and N ∈ {8, 16, 64}. `tol = 1e-12`.
2. **(Optional) Accept `typeName`** for `'single'` precision.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Pure spec coverage, no impl change. Numkit gausswin
  already matched MATLAB exactly across all (alpha, N) pairs.

  Spec extended from 1 to 9 fingerprints (alpha ∈ {1.5, 2.5, 4,
  8} × N ∈ {8, 16, 64} sample points + N=1 single-point). Parity
  OK numkit ↔ MATLAB ↔ Octave at tol=1e-12. 4 TEST_F gtest +
  smoke. Single-precision typeName (recommendation #2) deferred —
  not yet implemented across the window family; would need a
  cross-cutting refactor.
