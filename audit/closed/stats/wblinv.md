# stats.dist/wblinv — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 105c2b4
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Numbers match MATLAB exactly across
all probed inputs.

## Recommended fixes

1. **Spec extension** — fingerprint over parameter sweeps + edge
   cases. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Pure spec coverage, no impl change. Numkit wblinv already
  matched MATLAB exactly. 19 fingerprints (defaults via 1-arg form
  + p=0/p=1 boundaries + p outside [0,1] + scaled + shape variants
  + vector + 4 bad-params + NaN p/a). Parity OK numkit ↔ MATLAB ↔
  Octave at tol=1e-12. 6 TEST_F gtest + smoke.
