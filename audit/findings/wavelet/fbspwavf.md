# wavelet/fbspwavf — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** ddf4218
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/shape/shape.cpp` (`fbspwavf`)
- Adapter: same file
- Spec: `tools/parity/specs/fbspwavf.json`
- `[psi, x] = fbspwavf(LB, UB, N, m, fb, fc)` — matches MATLAB

## MATLAB R2025b — actual behavior

Documented signatures (`help fbspwavf`):

- `[psi, x] = fbspwavf(lb, ub, n, m, fb, fc)` — required all

`ψ(t) = √fb·(sinc(fb·t/m))^m · exp(2πi·fc·t)`.

## Gaps (numkit vs MATLAB)

**No major gap detected.** Numbers match.

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `fbspwavf(-5, 5, 8, 2, 1, 1)` real | `[0.0162 -0.0111 0.00272 -0.143 -0.143 0.00272 -0.0111 0.0162]` | identical ✅ |

## Recommended fixes

1. **Spec extension** — fingerprint over (m, fb, fc) variations.
   `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.
