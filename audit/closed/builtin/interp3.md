# builtin/interp3 — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** 3cb06a1
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** PROGRESS shows `correctness=OK`.

## Recommended fixes

1. **Spec extension** — fingerprint covering edge cases.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: pending (cycle 6)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED -- interp3(X, Y, Z, V, ...) with explicit grids failed because readGridAxis required X/Y/Z to be 1-D vectors, but meshgrid returns 3-D arrays. Fix: rewrote readGridAxis in libs/builtin/src/math/interp/interp.cpp to auto-detect the varying dimension of the input grid (works for both meshgrid and ndgrid). Bit-identical with MATLAB R2025b on probed input ([X,Y,Z]=meshgrid(1:5,1:5,1:5); V=X+Y+Z; interp3(X,Y,Z,V,2.5,2.5,2.5) = 7.5).
