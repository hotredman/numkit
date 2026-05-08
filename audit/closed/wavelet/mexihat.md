# wavelet/mexihat — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** ddf4218
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/shape/shape.cpp` (`mexihat`)
- Adapter: same file
- Spec: `tools/parity/specs/mexihat.json`
- `[psi, x] = mexihat(LB, UB, N)` — matches MATLAB exactly

## MATLAB R2025b — actual behavior

Documented signatures (`help mexihat`):

- `[psi, x] = mexihat(lb, ub, n)` — only signature

Mexican hat: `ψ(t) = (2/√3)·π^(-1/4)·(1-t²)·exp(-t²/2)`.

## Gaps (numkit vs MATLAB)

**No major gap detected.** Numbers match exactly.

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `mexihat(-5, 5, 8)` ψ | `[-7.76e-5 -0.0173 -0.314 0.329 0.329 -0.314 -0.0173 -7.76e-5]` | identical ✅ |
| x grid | `[-5 -3.571 -2.143 -0.714 0.714 2.143 3.571 5]` | identical ✅ |

## Recommended fixes

1. **Spec extension** — add fingerprint with N values from {16, 64,
   256} and asymmetric ranges (e.g., `mexihat(0, 5, 16)`).
   `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Pure spec coverage, no impl change. Numkit mexihat
  already matched MATLAB exactly across all probed (LB, UB, N).
  Spec extended from 4 to 9 fingerprints (N ∈ {8, 16, 64} on
  [-5, 5] + asymmetric range [0, 5]). Parity OK numkit ↔ MATLAB
  ↔ Octave at tol=1e-12. 4 TEST_F gtest + smoke.
