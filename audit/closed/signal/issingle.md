# signal/issingle — ТЗ for completion

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
- Notes: DEFINITIVE N/A (re-classified). MATLAB R2025b has no top-level issingle() function -- canonical spelling is isa(x, 'single'). Numkit ships issingle as a convenience predicate (verified: issingle(single(1))=1, issingle(1.0)=0). Definite N/A.
