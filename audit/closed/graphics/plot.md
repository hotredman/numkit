# graphics/plot — ТЗ for completion

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
- Notes: Initial closure was DEFERRED. plot(x, y) emits __FIGURE_DATA__ JSON to stdout via numkit figure manager (consumed by IDE/REPL). Bit-identical SHAPE with MATLAB; numkit does not implement MATLAB graphics-handle objects, so h = plot(...) does not bind h. Spec verifies side-effect runs.
