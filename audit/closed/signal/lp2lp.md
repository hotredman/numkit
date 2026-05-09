# signal/lp2lp — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 4fae461
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** PROGRESS shows `correctness=OK` on
benched input.

## Recommended fixes

1. **Spec extension** — fingerprint covering edge cases.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: pending (lp2lp TF dispatch)
- Closed date: 2026-05-09
- Notes: Initial closure (cycle 43) was DEFERRED -- numkit only accepted ZPK form (z, p, k, Wo[, Bw]); MATLAB also accepts TF form (b, a, Wo[, Bw]). Fix: register-level dispatch in libs/signal/src/filter_design/analog_filters.cpp -- 3 args (or 4 for lp2bp/lp2bs) routes through tf2zpk -> ZPK transform -> zp2tf back. Verified bit-identical with MATLAB R2025b on (buttap+zp2tf -> lp2lp) probes.
