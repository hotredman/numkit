# wavelet/iswt — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** d3d8da7
**Audit date:** 2026-05-06

## Gaps

**No major gap detected on basic call.** Cascade from `wfilters`
mismatch (audit/findings/wavelet/wfilters.md) may affect non-haar
wavelets — needs re-probe after that fix.

## Recommended fixes

1. **Verify after wfilters fix** — re-probe with db2/sym4/coif1
   wavelets to confirm coefficients match MATLAB.
2. **Spec extension** — fingerprint over wavelet families + levels.

## Out of scope for this ТЗ

- 2-D forms (swt2/iswt2 not implemented).
- modwtmra, modwtcorr, modwtvar (separate functions).
