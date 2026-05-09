# graphics/rlim — ТЗ for completion

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
- Closed in commit: pending (plotting batch)
- Closed date: 2026-05-09
- Notes: Setter form rlim([lo hi]) works (updates polar-axes JSON). Getter form rlim (no args) requires graphics-handle return semantics that numkit does not implement (architectural limit). Octave also lacks rlim, so parity reports N/A.
