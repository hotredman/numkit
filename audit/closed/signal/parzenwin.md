# signal/parzenwin — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 0e043c5
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/windows/windows.cpp:312` (`parzenwin`)
- Adapter: `libs/signal/src/windows/windows.cpp:583` (`parzenwin_reg`)
- Spec: `tools/parity/specs/parzenwin.json`
- 2nd arg silently ignored.

## MATLAB R2025b — actual behavior

Same surface as `bartlett`:

- `w = parzenwin(L)` / `(L, typeName)` — only `'double'`/`'single'`.

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | `'single'` typeName ignored | low |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `parzenwin(8)` | `[0.0039 0.1055 0.4727 0.918 0.918 0.4727 0.1055 0.0039]` | identical ✅ |

## Recommended fixes

Apply the joint typeName-acceptance fix (see
`audit/findings/stats/bartlett.md`). `tol = 1e-7`.

## Out of scope for this ТЗ

- N/A — joint fix.

## Closed
- Closed in commit: PENDING (joint windows sflag batch)
- Closed date: 2026-05-06
- Notes: Adapter rewritten with parseSflag/applySflag (6 windows accept 'periodic') or parseTypeNameOnly (6 windows reject 'periodic', accept only 'double'/'single'). Periodic implementation = take symmetric(N+1) and drop last sample (universal trick, no per-window code change). 'single' typeName silently uses double precision (parity gap noted). Verified vs MATLAB R2025b: 12 fingerprints match across numkit/MATLAB/Octave.
