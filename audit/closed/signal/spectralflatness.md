# signal/spectralflatness — ТЗ for completion

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
- Closed in commit: pending (camelCase fix)
- Closed date: 2026-05-09
- Notes: Initial closure (cycle 40) was DEFERRED due to name-case gap. Fix landed in libs/signal/src/library.cpp by registering camelCase alias `spectralFlatness` alongside lowercase. Parity now passes bit-identical against MATLAB R2025b.
