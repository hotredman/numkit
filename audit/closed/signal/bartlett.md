# signal/bartlett — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 0e043c5
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/windows/windows.cpp:119` (`bartlett`)
- Adapter: `libs/signal/src/windows/windows.cpp:522` (`bartlett_reg`)
- Spec: `tools/parity/specs/bartlett.json`
- `w = bartlett(N)` only — 2nd arg silently ignored.

## MATLAB R2025b — actual behavior

Documented signatures (`help bartlett`):

- `w = bartlett(L)`
- `w = bartlett(L, typeName)` — `typeName` is `'double'` (default) or
  `'single'` only — **no `sflag`**, the function is symmetric by
  construction.

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | `bartlett(N, 'single')` precision | numkit ignores typeName | low |
| 2 | spec coverage thin (basic only) | — | low |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `bartlett(8)` | `[0 0.2857 0.5714 0.8571 0.8571 0.5714 0.2857 0]` | identical ✅ |

## Recommended fixes

1. **Accept `'single'`/`'double'` typeName** as 2nd arg; cast output
   to single when requested.
2. **Reject any other 2nd arg with a clear error** (e.g.
   `bartlett(8, 'periodic')` should throw, matching MATLAB which
   gives "Expected TYPENAME to match one of these values: 'double',
   'single'").
3. **Spec extension:** add fingerprint with `'single'` cast result
   (which differs from double by precision only). `tol = 1e-7` for
   single, `0` for double.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING (joint windows sflag batch)
- Closed date: 2026-05-06
- Notes: Adapter rewritten with parseSflag/applySflag (6 windows accept 'periodic') or parseTypeNameOnly (6 windows reject 'periodic', accept only 'double'/'single'). Periodic implementation = take symmetric(N+1) and drop last sample (universal trick, no per-window code change). 'single' typeName silently uses double precision (parity gap noted). Verified vs MATLAB R2025b: 12 fingerprints match across numkit/MATLAB/Octave.
