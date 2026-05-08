# wavelet/dbwavf — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** medium
**Audited at commit:** 1c2df89
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/filter/families.cpp` (`dbwavf`)
- Spec: `tools/parity/specs/dbwavf.json`
- `h = dbwavf(wname)` — supports db1..db4
- Throws clean error message on unknown wname

## MATLAB R2025b — actual behavior

- `h = dbwavf(wname)` — supports `'db1'..'db45'`

Returns the Daubechies scaling filter. Length = 2N for `'dbN'`.
Sum = 1.

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | only db1..db4 supported (vs MATLAB db1..db45) | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `dbwavf('db1')` | `[0.5 0.5]` | identical ✅ |
| `dbwavf('db2')` | `[0.3415 0.5915 0.1585 -0.0915]` | identical ✅ |
| `dbwavf('db4')` | `[0.1629 0.5055 0.4461 -0.0198 -0.1323 0.0218 0.0233 -0.0075]` | identical ✅ |
| `dbwavf('db5')` | `[10 values]` | THROWS — unsupported |
| `dbwavf('db8')` | `[16 values]` | THROWS |

## Recommended fixes

1. **Extend Daubechies coefficient table to N=20 or N=45.** The
   coefficients are determined by the roots of the Daubechies
   polynomial; a precomputed table (e.g. from PyWavelets or
   any DSP textbook) covers the full range. For higher orders the
   coefficients become ill-conditioned (db20+) — MATLAB's table
   is the de-facto reference.
2. **Spec extension** — add fingerprint for db5..db10 once
   supported. `tol = 1e-12`.

## Out of scope for this ТЗ

- The biorthogonal/symlet/coiflet families — separate functions.

## Closed (partial)
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Extended Daubechies coefficient table from db1..db4 to
  db1..db10. Coefficients extracted from MATLAB R2025b
  `flip(dbwavf('dbN')*sqrt(2))` (matches numkit's reversed Lo_R
  storage convention) at 17-decimal precision. All sums verified
  to equal 1 within 1e-12. Error message updated to reflect new
  range.

  **Deferred — db11..db45:** beyond the typical practical range
  (db11+ becomes numerically ill-conditioned for filter design;
  most real-world wavelet code uses db4..db10). Coefficients are
  available from MATLAB if needed; can be added similarly when a
  consumer requires them.

  Spec extended from 8 to 18 fingerprints (lengths + sums + key
  values across db1..db10). Parity OK numkit ↔ MATLAB at
  tol=1e-12. Octave doesn't ship `dbwavf` (Wavelet Toolbox
  function); we follow MATLAB. 9 TEST_F gtest (existing 5 + 4
  new lengths / sums / db5 / db8). 65 wavelet-suite tests pass —
  no regression.
