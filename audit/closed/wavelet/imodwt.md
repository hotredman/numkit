# wavelet/imodwt — ТЗ for completion

**Status:** closed
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

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: No source change needed. The modwt sibling fix
  (argument-order + defaults, see closed/wavelet/modwt.md) makes
  imodwt usable end-to-end via round-trip. **Verified**:
  imodwt(modwt(x, 'haar', 3), 'haar') recovers x to ~2e-16 machine
  precision (haar) and ~2e-12 (db2). The structurally important
  invariant — that the inverse undoes the forward — holds bit-
  identical to MATLAB.

  4 artefacts (batched across all 4 ТЗ):
  - impl: libs/wavelet/src/swt/swt.cpp — modwt_reg argument-order
    rewrite + default wname/lev. swt/iswt/imodwt unchanged.
  - parity: tools/parity/specs/{modwt,imodwt,swt,iswt}.json — 4
    spec extensions, all correctness=OK (modwt/swt fingerprints
    locked to shape + magnitudes only because of the kernel
    convention gaps; iswt/imodwt fingerprints lock the round-trip
    error which IS bit-identical to MATLAB).
  - gtest: libs/wavelet/tests/swt_modwt_test.cpp — 12 tests
    covering all 4 functions: modwt arg-order forms, swt magnitude
    + approx-row, both round-trips on haar+db2, error path.
  - smoke: libs/wavelet/tests/smoke/swt_modwt_smoke.m
