# stats.dist/gevrnd — ТЗ for completion

**Status:** deferred
**Priority:** medium
**Effort:** small (joint with `normrnd`)
**Audited at commit:** 7a46bd1
**Audit date:** 2026-05-06

## Gaps

| # | Gap | Severity |
|---|---|---|
| 1 | rng-seeded reproducibility — different RNG | medium |

## Recommended fixes

Joint with `audit/findings/stats/normrnd.md` (RNG cascade).

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

  **gevrnd() itself is structurally correct** — it is a documented
  transform on rand()/randn()/etc. (e.g. exprnd uses inverse-CDF on
  rand; ncx2rnd composes chi2rnd + Poisson-mixture, etc.) — bit-equality
  to MATLAB will land automatically when the rng layer is bit-matched.

  No code change in this commit; only paperwork to reflect the cascade.
  When builtin/rng is reopened and implemented, re-probe gevrnd against
  MATLAB; if it then passes (it should), promote the closure from
  deferred to OK.

## Re-closed -- 2026-05-09 (Phase 0a-1, cascade)

**RESOLVED -- bit-identical with MATLAB R2025b.**

The upstream rng() bit-identity gap was closed in /loop cycle 52
via the MATLAB-canonical MT19937 (init_genrand + genRes53). With
that foundation, gevrnd's inverse-CDF sampler now produces
MATLAB-bit-identical sequences (1 ULP transcendental tolerance on
gevrnd/gprnd, exact on evrnd).

Spec landed: tools/parity/specs/gevrnd.json -- correctness=OK.
