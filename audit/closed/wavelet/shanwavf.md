# wavelet/shanwavf — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** ddf4218
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/shape/shape.cpp` (`shanwavf`)
- Adapter: same file
- Spec: `tools/parity/specs/shanwavf.json`
- `[psi, x] = shanwavf(LB, UB, N, fb, fc)` — matches MATLAB

## MATLAB R2025b — actual behavior

Documented signatures (`help shanwavf`):

- `[psi, x] = shanwavf(lb, ub, n, fb, fc)` — required all

`ψ(t) = √fb·sinc(fb·t)·exp(2πi·fc·t)`.

## Gaps (numkit vs MATLAB)

**No major gap detected.** Numbers match.

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `shanwavf(-5, 5, 8, 1, 1)` real | `[3.9e-17 0.0783 0.0402 -0.0775 -0.0775 0.0402 0.0783 3.9e-17]` | identical ✅ |

## Recommended fixes

1. **Spec extension** — fingerprint over (fb, fc) variations.
   `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Pure spec coverage, no impl change. Numkit shanwavf
  already matched MATLAB exactly. Spec extended from 5 to 7
  fingerprints (default fb=1/fc=1 + non-default fb=0.5/fc=2 +
  N=33 peak). Parity OK numkit ↔ MATLAB ↔ Octave at tol=1e-10.
  3 TEST_F gtest + smoke.
