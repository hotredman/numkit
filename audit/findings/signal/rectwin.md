# signal/rectwin — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 0e043c5
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/windows/windows.cpp:110` (`rectwin`)
- Adapter: `libs/signal/src/windows/windows.cpp:514` (`rectwin_reg`)
- Spec: `tools/parity/specs/rectwin.json`
- `w = rectwin(N)` only — returns N ones.

## MATLAB R2025b — actual behavior

`w = rectwin(L)` — single-argument, no second arg accepted. MATLAB
throws "Too many input arguments" if you try anything else.

## Gaps (numkit vs MATLAB)

**No major gap.** `rectwin` is the simplest window — single argument
in MATLAB; same in numkit.

| # | Gap | Severity |
|---|---|---|
| 1 | `rectwin(N, 'periodic')` accepted silently in numkit (extra arg dropped); MATLAB throws | very low |
| 2 | Spec coverage thin (only N=1024) | low |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `rectwin(8)` | `[1 1 1 1 1 1 1 1]` | identical ✅ |
| `rectwin(8, 'periodic')` | THROWS "Too many input arguments" | accepts silently (extra arg dropped) |

## Recommended fixes

1. **(Optional) Strict-nargin check:** throw on `args.size() > 1`
   to match MATLAB. Low priority; numkit's lax behaviour matches
   Octave.
2. **Spec extension:** add fingerprint with N=1, N=2 edges.
   `tol = 0`.

## Out of scope for this ТЗ

- N/A.
