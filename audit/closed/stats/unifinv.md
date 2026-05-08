# stats.dist/unifinv — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** e580a5c
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Numbers match MATLAB exactly.

## Recommended fixes

1. **Spec extension** — fingerprint over parameter sweeps + edge
   cases. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Pure spec coverage, no impl change. 16 fingerprints
  (defaults via 1-arg form + wider interval + p=0/p=1 boundaries +
  vector + p outside [0,1] + 3 bad-params + NaN p/a). Parity OK
  numkit ↔ MATLAB at tol=1e-12. Octave's unifinv doesn't ship the
  1-arg default form; we follow MATLAB. 5 TEST_F gtest + smoke.
