# signal/upfirdn — ТЗ for completion

**Status:** closed
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
- Closed in commit: pending (trivial-fix batch)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED -- output length differed by 1 (numkit Lx*p, MATLAB ceil(((Lx-1)*p + Lh) / q)). Root cause: numkit reused upsample (which adds Lx*p trailing zeros) + filter (which truncates to input length); MATLAB does upsample-no-trailing + full convolve. Fix: rewrite upfirdn to do explicit convolution at the upsampled positions then downsample by q. Bit-identical with MATLAB R2025b and scipy.signal.upfirdn on probed input (returns 11 elements vs old 10).
