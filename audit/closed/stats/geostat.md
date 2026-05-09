# stats.dist/geostat — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** 41862bc
**Audit date:** 2026-05-06

## Gaps

**No major gap detected** on numerics.

## Recommended fixes

1. **Spec extension** — fingerprint over parameter sweeps. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Stats namespace batch (17 funcs: pdf/inv/stat for ev/geo/gev/gp + corrcoef/cov/datasample/datastats/dummyvar/combnk/geornd). Bit-identical MATLAB R2025b on probed inputs (15 verified, 2 deferred — datasample/datastats).
