# builtin/rng — ТЗ for completion

**Status:** open
**Priority:** **high** (PROGRESS notes `correctness=MISMATCH` — root cause of all *rnd RNG mismatches)
**Effort:** medium
**Audited at commit:** 3cb06a1
**Audit date:** 2026-05-06

## Gaps

| # | Gap | Severity |
|---|---|---|
| 1 | After `rng(seed)`, subsequent `rand()`/`randn()`/etc. produce DIFFERENT values from MATLAB. The base RNG is not bit-compatible with MATLAB's Mersenne Twister + Ziggurat normal. This cascades into ALL distribution `*rnd` functions (norm/chi2/t/F/beta/gam/exp/rayl/wbl/logn/ev/gev/gp/ncx2 = 14 audit `rnd` ТЗ). | **high — root cause of cross-MATLAB script reproducibility failure** |

## Recommended fixes

1. **Adopt MATLAB's Mersenne Twister seeding convention.** MATLAB's
   `rng(seed)` initializes a specific MT state from an integer seed
   that's slightly different from the standard `mt19937` seeding.
   Match the `mxArray` initialization to bit-reproduce MATLAB's
   first 100 outputs of `rand()`.
2. **Adopt MATLAB's Ziggurat normal-variate algorithm** for `randn()`.
3. **Spec extension:** add deterministic-seed fingerprint —
   `rng(42); randn(1)` should equal MATLAB's `-0.5382438937`.
   `tol = 0`.

## Out of scope for this ТЗ

- N/A — single fix cascades to all 14 `*rnd` functions.
