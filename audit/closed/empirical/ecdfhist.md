# stats.empirical/ecdfhist — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** f92087f
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Numbers match MATLAB exactly on probed
inputs.

## Recommended fixes

1. **Spec extension** — fingerprint over more inputs and N-V
   options. `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Auditor said "no major gap detected"; spec extension
  caught a real off-by-one in the bin assignment for boundary
  values.

  **Bug:** numkit used `floor((v - xmin) / width)` for the bin
  index, which sends boundary values (v == edge[k]) to the UPPER
  bin. MATLAB sends them to the LOWER bin (i.e., bin k contains
  (edge[k-1], edge[k]]).

  **Fix:** changed to `ceil((v - xmin) / width) - 1` clamped to
  [0, m-1], which matches MATLAB's edge-inclusion convention.

  Visible only when m doesn't evenly tile the data range — e.g.
  ecdfhist(ecdf(1:10), 3) had numkit n = [0.1, 0.1, 0.1333] vs
  MATLAB [0.1333, 0.1, 0.1]. Pre-existing m=10 / m=5 cases
  happened to align so passed by accident.

  Spec extended from 1 to 17 fingerprints (m ∈ {3, 5, 10} ×
  uniform/non-uniform). Parity OK numkit ↔ MATLAB at tol=1e-9.
  Octave doesn't ship `ecdfhist`; we follow MATLAB. 5 TEST_F gtest
  (new file).
