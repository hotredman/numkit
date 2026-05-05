# signal/idct — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small (joint with `dct`)
**Audited at commit:** 9bce106
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/transforms/...` (`idct`)
- Spec: `tools/parity/specs/idct.json`
- Roundtrip OK for 1-D Type-II/III pair.

## MATLAB R2025b — actual behavior

- `Y = idct(X)` — inverse of 1-D Type-II
- `Y = idct(X, n[, dim])` — pad/truncate, along dim
- `Y = idct(___, 'Type', t)` — match Type used in `dct`

## Gaps (numkit vs MATLAB)

Same shape as `dct` (see `audit/findings/signal/dct.md`):
- Matrix input column-wise — needs verification
- Length override / dim arg — not supported
- `'Type'` flag — not supported

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `idct(dct([1:8]'))` roundtrip | `[1..8]` (within 1e-15) | `[1..8]` ✅ |

## Recommended fixes

Joint with `audit/findings/signal/dct.md`. Same parser shape.

Spec: extend with matrix, length-override, Type. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A — joint fix.
