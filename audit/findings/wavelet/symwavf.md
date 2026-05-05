# wavelet/symwavf — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** medium
**Audited at commit:** 1c2df89
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/filter/families.cpp` (`symwavf`)
- Spec: `tools/parity/specs/symwavf.json`
- Supports sym2 and sym4 only (per the same family table as
  `wfilters`)

## MATLAB R2025b — actual behavior

- `h = symwavf(wname)` — supports `'sym2'..'sym45'`

Length = 2N for `'symN'`.

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | only sym2/sym4 supported (vs MATLAB sym2..sym45) | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `symwavf('sym2')` | `[0.3415 0.5915 0.1585 -0.0915]` | identical ✅ |
| `symwavf('sym4')` head | `[0.0228 -0.0089 -0.0702 0.2106]` | identical ✅ |
| `symwavf('sym3')` | length 6 | likely THROWS — unsupported |
| `symwavf('sym5')` etc. | unsupported | THROWS |

## Recommended fixes

1. **Extend Symlet coefficient table to sym20+.** Public-domain
   tables exist (Symlets are the "least asymmetric" Daubechies
   variants).
2. **Spec extension** — add fingerprint for sym3, sym5..sym10
   once supported. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.
