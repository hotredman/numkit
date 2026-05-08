# wavelet/morlet — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** ddf4218
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/shape/shape.cpp` (`morlet`)
- Adapter: same file
- Spec: `tools/parity/specs/morlet.json`
- `[psi, x] = morlet(LB, UB, N)` — real Morlet, matches MATLAB

## MATLAB R2025b — actual behavior

Documented signatures (`help morlet`):

- `[psi, x] = morlet(lb, ub, n)` — only signature

Real Morlet: `ψ(t) = exp(-t²/2)·cos(5t)`.

## Gaps (numkit vs MATLAB)

**No major gap detected.** Numbers match.

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `morlet(-5, 5, 8)` ψ | `[3.69e-6 9.29e-4 -0.0279 -0.704 -0.704 -0.0279 9.29e-4 3.69e-6]` | identical ✅ |

## Recommended fixes

1. **Spec extension** — N values {16, 64, 256}; asymmetric ranges.
   `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Pure spec coverage, no impl change. Numkit morlet
  already matched MATLAB exactly. Spec extended from 5 to 8
  fingerprints (N ∈ {8, 16, 64} on [-5, 5] + asymmetric [0, 5]).
  Parity OK numkit ↔ MATLAB ↔ Octave at tol=1e-12. 4 TEST_F
  gtest + smoke.
