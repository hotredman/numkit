# signal/nuttallwin — ТЗ for completion

**Status:** open
**Priority:** **high**
**Effort:** small (joint with the rest of signal.windows)
**Audited at commit:** 0e043c5
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/windows/windows.cpp:337` (`nuttallwin`)
- Adapter: `libs/signal/src/windows/windows.cpp:591` (`nuttallwin_reg`)
- Spec: `tools/parity/specs/nuttallwin.json`
- 2nd arg silently ignored.

## MATLAB R2025b — actual behavior

Same surface as `hann`:

- `w = nuttallwin(L)` / `(L, sflag)` / `(___, typeName)`

4-term Nuttall coefficients [0.3635819, 0.4891775, 0.1365995,
0.0106411]. Periodic mode uses `L` denominator.

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | `nuttallwin(N, 'periodic')` silently returns symmetric | **high** |
| 2 | `'single'` typeName ignored | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `nuttallwin(8)` | `[3.63e-4 0.0378 0.3427 0.8919 0.8919 0.3427 0.0378 3.63e-4]` | identical ✅ |
| `nuttallwin(8, 'periodic')` | `[3.63e-4 0.0252 0.227 0.702 1 0.702 0.227 0.0252]` | (returns symmetric) ❌ |

## Recommended fixes

Joint with `audit/findings/stats/hann.md`. `tol = 1e-10`.

## Out of scope for this ТЗ

- N/A — joint fix.

## Closed
- Closed in commit: PENDING (joint windows sflag batch)
- Closed date: 2026-05-06
- Notes: Adapter rewritten with parseSflag/applySflag (6 windows accept 'periodic') or parseTypeNameOnly (6 windows reject 'periodic', accept only 'double'/'single'). Periodic implementation = take symmetric(N+1) and drop last sample (universal trick, no per-window code change). 'single' typeName silently uses double precision (parity gap noted). Verified vs MATLAB R2025b: 12 fingerprints match across numkit/MATLAB/Octave.
