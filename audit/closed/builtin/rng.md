# builtin/rng — ТЗ for completion

**Status:** deferred
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

## Closed
- Closed in commit: TBD (DEFERRED — flagged for user-supervised review)
- Closed date: 2026-05-09
- Notes: **DEFERRED, not implemented.** Routed to the deferred queue
  per MEMORY.md feedback_post_parity_simd_work guidance: behavior
  fixes that touch engine state are collaborative, not autonomous.

  Probe results (current numkit vs MATLAB R2025b):

    rng(42); rand(1)   numkit=0.7965 vs MATLAB=0.3745  (DIFFERS)
    rng(42); randn(1)  numkit=0.5154 vs MATLAB=-0.5382 (DIFFERS)
    rng(0);  rand(1)   numkit=0.5928 vs MATLAB=0.8147  (DIFFERS)
    rng(0);  randn(1)  numkit=0.3028 vs MATLAB=0.5377  (DIFFERS)

  Both  and  differ — confirms the gap has TWO causes:

  1. **Mersenne Twister seeding**: numkit uses libc++/MSVC default
      (Knuth init_genrand). MATLAB uses its
     own MT init algorithm (likely the original 1999 Matsumoto-
     Nishimura init with a different fill order / twist condition).
     Bit-reproducing requires a custom MT19937 implementation —
     std::mt19937 cannot be coaxed into MATLAB convention by tweaking
     the seed.

  2. **Normal-variate algorithm**: numkit uses libc++/MSVC default
      (Box-Muller / polar method). MATLAB
     uses Ziggurat with its own table constants. Even with matching
     uniform seed, the normal output differs.

  **Why deferred**:
  - Custom MT + Ziggurat is ~300-500 lines of careful code, with
    lookup tables that need to bit-match MATLAB constants (which
    are not publicly documented — would need partial reverse-
    engineering).
  - The change touches the process-static  —
    engine-state interaction that needs design review (per
    COORDINATION.md the engine state lives at the kernel/CORE
    boundary, not pure libs/ territory).
  - Cascades to 14 *rnd ТЗ — but those *rnd functions ARE
    structurally correct (transformation chains on rand/randn are
    standard); they just fail bit-equality because the upstream
    rand/randn don't bit-match. So the *rnd correctness gap is
    cosmetic, not algorithmic.

  **Reopen criteria**: when user explicitly requests bit-identical
  RNG reproducibility for cross-MATLAB script porting (the only
  use case that needs this).
