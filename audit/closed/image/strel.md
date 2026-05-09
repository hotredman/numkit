# image/strel — ТЗ for completion

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
- Closed in commit: pending (struct outputs)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED. Wrapped strel output in a 1x1 struct with fields {Neighborhood, Dimensionality} matching MATLAB R2025b. Existing morphology consumers (imerode, imdilate, imopen, imclose) updated via unpack_se to accept BOTH struct form (.Neighborhood field) and bare logical matrix (legacy form). Square / rectangle / diamond / line / arbitrary shapes are bit-identical. Disk shape: MATLAB uses a line-strel decomposition that yields a smaller equivalent .Neighborhood; numkit returns the full disk mask -- morphology results are identical, only the matrix size differs.
