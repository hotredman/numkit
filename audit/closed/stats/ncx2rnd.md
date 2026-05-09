# stats.dist/ncx2rnd — ТЗ for completion

**Status:** deferred
**Priority:** medium
**Effort:** small
**Audited at commit:** d68c22b
**Audit date:** 2026-05-06

## Gaps

| # | Gap | Severity |
|---|---|---|
| 1 | rng-seeded reproducibility — different RNG | medium |

## Recommended fixes

1. Joint with `audit/findings/stats/normrnd.md` (RNG cascade).

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD (DEFERRED — cascades on builtin/rng)
- Closed date: 2026-05-09
- Notes: **DEFERRED via cascade.** This ТЗ is purely about rng-seeded
  reproducibility against MATLAB R2025b. The underlying gap lives in
  builtin/rng (Mersenne Twister seeding + Ziggurat normal-variate
  algorithm) — see audit/closed/builtin/rng.md for the full reasoning
  and reopen criteria.

  **ncx2rnd() itself is structurally correct** — it is a documented
  transform on rand()/randn()/etc. (e.g. exprnd uses inverse-CDF on
  rand; ncx2rnd composes chi2rnd + Poisson-mixture, etc.) — bit-equality
  to MATLAB will land automatically when the rng layer is bit-matched.

  No code change in this commit; only paperwork to reflect the cascade.
  When builtin/rng is reopened and implemented, re-probe ncx2rnd against
  MATLAB; if it then passes (it should), promote the closure from
  deferred to OK.

## Status update -- 2026-05-09 (Phase 0a-1 + 0a-2)

The upstream rng() bit-identity was closed in Phase 0a-1 (commit
5488a97e), unblocking the rand-driven cascade (evrnd, gevrnd,
gprnd). However, ncx2rnd's specific composition path is:
  J ~ Poisson(λ/2),  X ~ Gamma(k/2 + J, scale=2)
which goes through std::poisson_distribution and
std::gamma_distribution -- neither MATLAB-bit-identical even with
a bit-identical underlying MT.

Closing ncx2rnd to bit-identity therefore requires a separate
(deferred) Phase 0a-1b sweep landing **all three**:
  - Marsaglia-Tsang Ziggurat for randn() with MATLAB-canonical
    128-row table (reverse-engineering required since constants
    aren't fully documented)
  - MATLAB-canonical Gamma sampler (likely Marsaglia-Tsang Gamma 2000
    on Ziggurat-driven normals)
  - MATLAB-canonical Poisson sampler

Estimated effort: ~330 LOC + multi-cycle table verification.
Realistic chance of full bit-identity: ~10-15% (each stage compounds
table-lookup divergence; even small ULP differences propagate).

For now ncx2rnd remains DEFERRED. Statistical correctness is
preserved (random samples are valid draws from chi²(k, λ); just
not bit-equal to MATLAB's specific draws under same rng(seed)).
