# stats.dist/evrnd — ТЗ for completion

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

  **evrnd() itself is structurally correct** — it is a documented
  transform on rand()/randn()/etc. (e.g. exprnd uses inverse-CDF on
  rand; ncx2rnd composes chi2rnd + Poisson-mixture, etc.) — bit-equality
  to MATLAB will land automatically when the rng layer is bit-matched.

  No code change in this commit; only paperwork to reflect the cascade.
  When builtin/rng is reopened and implemented, re-probe evrnd against
  MATLAB; if it then passes (it should), promote the closure from
  deferred to OK.
