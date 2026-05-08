# stats.cluster/cophenet — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** b2f133b
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Numbers match MATLAB exactly across
all probed inputs.

## Recommended fixes

1. **Spec extension** — fingerprint covering metric variants /
   parameter sweeps. `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Auditor said "no major gap detected"; spec extension
  caught a real gap. The 2-output form `[c, d] = cophenet(Z, Y)`
  was throwing "Undefined function or variable 'd'" because the
  adapter only emitted outs[0].

  **Fix:** refactored `cophenet` into `cophenet_full` which
  returns both the scalar correlation and the (1×Yn) cophenetic
  distance vector dcoph. Adapter now reads nargout and emits
  outs[1] when requested. The 1-arg `cophenet()` API kept as a
  wrapper that returns the scalar only.

  Spec created (didn't exist) with 9 fingerprints (scalar c +
  2-output [c, d] + dcoph distances). Parity OK numkit ↔ MATLAB
  ↔ Octave at tol=1e-9. 3 TEST_F gtest (new file).
