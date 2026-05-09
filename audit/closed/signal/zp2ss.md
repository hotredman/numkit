# signal/zp2ss — ТЗ for completion

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
- Closed in commit: pending (tf2ss canonical-form fix)
- Closed date: 2026-05-09
- Notes: Initial closure (cycle 43) was DEFERRED -- different state-space canonical form. Root cause: numkit tf2ss built BOTTOM-row companion (A=[0 I; -ah_rev]) with B=[0;...;1] and reversed C; MATLAB tf2ss returns the controller canonical form: TOP-row companion (A=[-a2 ... -a(N+1); I 0]), B=[1;0;...;0], C=[b2-a2*b1, ...]. All four high-level converters (sos2ss, ss2sos, ss2zp, zp2ss) flow through tf2ss, so fixing tf2ss closes all four at once. Verified bit-identical with MATLAB R2025b on probed inputs.
