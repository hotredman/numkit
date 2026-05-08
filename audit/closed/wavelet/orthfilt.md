# wavelet/orthfilt — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 1c2df89
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/filter/wfilters.cpp` (`orthfilt`)
- Spec: `tools/parity/specs/orthfilt.json`
- `[Lo_D, Hi_D, Lo_R, Hi_R] = orthfilt(W)` — quadruple from a
  unit-norm scaling filter

## MATLAB R2025b — actual behavior

`[Lo_D, Hi_D, Lo_R, Hi_R] = orthfilt(W)`. `W` is the scaling filter
(sum=1, length even). Returns the standard orthogonal quadruple
following Mallat's convention.

## Gaps (numkit vs MATLAB)

**No major gap detected on the basic call.** Probed values match.

| # | Gap | Severity |
|---|---|---|
| 1 | Spec coverage thin (single input) | low |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `orthfilt(dbwavf('db2'))` Lo_D | `[-0.1294 0.2241 0.8365 0.4830]` | identical ✅ |
| same Hi_R | `[-0.1294 -0.2241 0.8365 -0.4830]` | identical ✅ |

Note: this matches MATLAB's `wfilters('db2')` Lo_D — confirming
the numkit-internal orthfilt convention IS correct. The bug
described in `audit/findings/wavelet/wfilters.md` is in `wfilters`,
not in `orthfilt`. `wfilters` should call `orthfilt` correctly to
produce MATLAB-compatible labels.

## Recommended fixes

1. **Spec extension** — add fingerprint with several scaling
   filters (`db1..db4`, `sym4`, `coif1`). `tol = 1e-12`.
2. **(Documentation)** Note in PROGRESS that `orthfilt` is
   correct; the cascade error in `wfilters` should not be confused
   with this function.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Pure spec coverage, no impl change. Numkit orthfilt
  already matched MATLAB exactly across db2 (4-tap), db4 (8-tap),
  and a 2-tap custom scaling filter.

  Spec extended from 8 to 18 fingerprints (db2 + db4 longer +
  custom 2-tap). Parity OK numkit ↔ MATLAB at tol=1e-12. Octave
  doesn't ship `dbwavf` (Wavelet Toolbox); we follow MATLAB. 6
  TEST_F gtest (existing 3 + 3 new high-precision db2 / db4 /
  custom).
