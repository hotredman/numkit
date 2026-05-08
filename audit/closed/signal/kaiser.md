# signal/kaiser — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 69ef496
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/windows/windows.cpp` (`kaiser`)
- Adapter: `libs/signal/src/windows/windows.cpp:504` (`kaiser_reg`)
- Spec: `tools/parity/specs/kaiser.json`
- What works today:
  - `w = kaiser(N[, beta])` — default `beta=0.5`
  - All probed cases match MATLAB exactly

## MATLAB R2025b — actual behavior

Documented signatures (`help kaiser`):

- `w = kaiser(L, beta)` — `beta` is the shape parameter; common
  values `0` (≈ rectangular), `5` (Hamming-like), `8.6` (Blackman-
  like)

The MATLAB doc lists only the `(L, beta)` 2-arg form (no
default-beta single-arg in the doc), though the function actually
accepts `kaiser(L)` with `beta=0.5`.

## Gaps (numkit vs MATLAB)

**No major gap detected.** Probed values match.

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `kaiser(8)` (beta=0.5) | `[0.9403 0.9693 0.9889 0.9988 0.9988 0.9889 0.9693 0.9403]` | identical ✅ |
| `kaiser(8, 5)` | `[0.0367 0.2707 0.6517 0.9552 0.9552 0.6517 0.2707 0.0367]` | identical ✅ |
| `kaiser(8, 10)` | `[0.000355 0.0598 0.4014 0.9073 0.9073 0.4014 0.0598 0.000355]` | identical ✅ |

## Recommended fixes

1. **Spec extension** — add fingerprint for beta ∈ {0, 1, 5, 8.6,
   12} × N ∈ {8, 16, 64}. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Pure spec coverage, no impl change. Numkit kaiser already
  matched MATLAB exactly across all probed (beta, N) pairs.

  Spec extended from 1 fingerprint (single beta=5 / N=1024 use)
  to 10: beta ∈ {0, 1, 5, 8.6, 12} × N ∈ {8, 16, 64} sample
  points + default-beta + N=1 (single-point window). Parity OK
  numkit ↔ MATLAB ↔ Octave at tol=1e-12. 8 TEST_F gtest (existing
  3 + 5 new for default / β=5 / β=8.6 / β=12 / single-point) +
  smoke.
