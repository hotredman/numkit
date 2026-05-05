# wavelet/coifwavf — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** 1c2df89
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/filter/families.cpp` (`coifwavf`)
- Spec: `tools/parity/specs/coifwavf.json`
- Only `coif1` supported (probe shows `coif2` throws)

## MATLAB R2025b — actual behavior

- `h = coifwavf(wname)` — supports `'coif1'..'coif5'`

Length = 6K for `'coifK'`.

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | only coif1 supported (vs MATLAB coif1..coif5) | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `coifwavf('coif1')` | `[-0.0514 0.2389 0.6029 0.2721 -0.0514 -0.0111]` | identical ✅ |
| `coifwavf('coif2')` head | `[0.0116 -0.0293 -0.0476 0.2730]` | THROWS |

## Recommended fixes

1. **Extend Coiflet coefficient table to coif5.** Public-domain
   tables exist; lengths are 6, 12, 18, 24, 30 for K = 1..5.
2. **Spec extension** — add fingerprint for coif2..coif5 once
   supported. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.
