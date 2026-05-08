# stats.dist/nbinrnd — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 41862bc
**Audit date:** 2026-05-06

## Gaps

**No major gap detected** on numerics; *rnd shows minor RNG mismatch (cascade from base RNG — not bit-identical to MATLAB on the same seed)

## Recommended fixes

1. **Spec extension** — fingerprint over parameter sweeps. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.
