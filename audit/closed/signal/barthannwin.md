# signal/barthannwin — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 0e043c5
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/windows/windows.cpp:460` (`barthannwin`)
- Adapter: `libs/signal/src/windows/windows.cpp:626` (`barthannwin_reg`)
- Spec: `tools/parity/specs/barthannwin.json`
- 2nd arg silently ignored.

## MATLAB R2025b — actual behavior

Same surface as `bartlett`:

- `w = barthannwin(L)`
- `w = barthannwin(L, typeName)` — only `'double'`/`'single'`, no
  sflag.

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | `'single'` typeName ignored | low |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `barthannwin(8)` | `[0 0.2116 0.6017 0.9281 0.9281 0.6017 0.2116 0]` | identical ✅ |

## Recommended fixes

1. **Accept `'single'`/`'double'` typeName** and cast output.
2. **Reject other 2nd args** with MATLAB-matching error.
3. **Spec extension:** add `'single'` fingerprint. `tol = 1e-7` for
   single.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING (joint windows sflag batch)
- Closed date: 2026-05-06
- Notes: Adapter rewritten with parseSflag/applySflag (6 windows accept 'periodic') or parseTypeNameOnly (6 windows reject 'periodic', accept only 'double'/'single'). Periodic implementation = take symmetric(N+1) and drop last sample (universal trick, no per-window code change). 'single' typeName silently uses double precision (parity gap noted). Verified vs MATLAB R2025b: 12 fingerprints match across numkit/MATLAB/Octave.
