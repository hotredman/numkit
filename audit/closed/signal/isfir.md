# signal/isfir — ТЗ for completion

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
- Closed in commit: pending (A1 N/A cleanup)
- Closed date: 2026-05-09
- Notes: DEFINITIVE N/A (re-classified). MATLAB R2025b ships isfir() ONLY as a method on digitalFilter system objects, not as a standalone top-level function. Numkit exposes it as a top-level convenience predicate (verified working via direct probe: isfir([1 2 3])=1, isfir([1 2 3], [1 -0.5])=0). Definite N/A -- no MATLAB top-level reference for parity.
