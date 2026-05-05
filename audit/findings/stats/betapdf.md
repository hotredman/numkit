# stats.dist/betapdf — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 301e5a5
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/beta.cpp` (`betapdf`)
- Spec: `tools/parity/specs/betapdf.json`
- `Y = betapdf(X, a, b)` — matches MATLAB exactly.

## MATLAB R2025b — actual behavior

`y = betapdf(x, a, b)`. `x` outside `(0, 1)` ⇒ 0. `a<=0` or `b<=0`
⇒ NaN.

## Gaps (numkit vs MATLAB)

**No major gap detected.**

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `betapdf(0.5, 2, 3)` | `1.5` | identical ✅ |
| `betapdf([0.1 0.5 0.9]', 2, 3)` | `[0.972 1.5 0.108]` | identical ✅ |
| `betapdf(1.5, 2, 3)` (out-of-(0,1)) | `0` | `0` ✅ |
| `betapdf(0.5, 0, 3)` (invalid a) | `NaN` | `nan` ✅ |

## Recommended fixes

1. **Spec extension** — add fingerprint covering edge X (0, 1),
   invalid params, vector inputs. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.
