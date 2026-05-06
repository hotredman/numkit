# signal/flattopwin — ТЗ for completion

**Status:** open
**Priority:** **high**
**Effort:** small (joint with the rest of signal.windows)
**Audited at commit:** 0e043c5
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/windows/windows.cpp:190` (`flattopwin`)
- Adapter: `libs/signal/src/windows/windows.cpp:555` (`flattopwin_reg`)
- Spec: `tools/parity/specs/flattopwin.json`
- 2nd arg silently ignored.

## MATLAB R2025b — actual behavior

Same surface as `hann`:

- `w = flattopwin(L)` / `(L, sflag)` / `(___, typeName)`

5-term flat-top window coefficients (Heinzel/Schroeder convention).
Periodic mode uses `L` instead of `L-1` in the denominator.

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | `flattopwin(N, 'periodic')` silently returns symmetric | **high** |
| 2 | `'single'` typeName ignored | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `flattopwin(8)` | `[-4.21e-4 -0.0368 0.0107 0.7809 0.7809 0.0107 -0.0368 -4.21e-4]` | identical ✅ |
| `flattopwin(8, 'periodic')` | `[-4.21e-4 -0.0269 -0.0547 0.4441 1 0.4441 -0.0547 -0.0269]` | (returns symmetric) ❌ |

## Recommended fixes

Joint with `audit/findings/stats/hann.md`. `tol = 1e-10`.

## Out of scope for this ТЗ

- N/A — joint fix.

## Closed
- Closed in commit: PENDING (joint windows sflag batch)
- Closed date: 2026-05-06
- Notes: Adapter rewritten with parseSflag/applySflag (6 windows accept 'periodic') or parseTypeNameOnly (6 windows reject 'periodic', accept only 'double'/'single'). Periodic implementation = take symmetric(N+1) and drop last sample (universal trick, no per-window code change). 'single' typeName silently uses double precision (parity gap noted). Verified vs MATLAB R2025b: 12 fingerprints match across numkit/MATLAB/Octave.
