# signal/interpft — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 9bce106
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/transforms/...` (`interpft`)
- Spec: `tools/parity/specs/interpft.json`
- `Y = interpft(X, n)` — matches MATLAB exactly on probed input.

## MATLAB R2025b — actual behavior

- `Y = interpft(X, n)` — band-limited interpolation (FFT-based)
- `Y = interpft(X, n, dim)` — along dim

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | `dim` arg | likely not supported | low |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `interpft([1:8]', 16)` head | `[1 0.473 2 2.969 3 3.301 4 4.5]` | identical ✅ |

## Recommended fixes

1. **Add `dim` arg** support.
2. **Spec extension** — fingerprint for matrix input + dim.

## Out of scope for this ТЗ

- N/A.
