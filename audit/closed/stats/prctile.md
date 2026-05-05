# stats/prctile — ТЗ for completion

**Status:** open
**Priority:** **high** (PROGRESS already flags `correctness=MISMATCH`)
**Effort:** small (joint with `quantile`)
**Audited at commit:** ba142e6
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/descriptive/descriptive.cpp:409` (`prctile`)
- Adapter: `libs/stats/src/descriptive/descriptive.cpp:1042` (`prctile_reg`)
- Spec: `tools/parity/specs/prctile.json` (PROGRESS:
  `correctness=MISMATCH`)
- Implemented as `prctile(A, p) = quantile(A, p/100)` — inherits
  every gap and bug from `quantile`.

## MATLAB R2025b — actual behavior

Documented signatures (`help prctile`):

- `P = prctile(A, p)` — `p` in `[0, 100]`
- `P = prctile(A, p, "all")` / `(A, p, dim)` / `(A, p, vecdim)`
- `P = prctile(___, Method=method)` — same as `quantile`

## Gaps (numkit vs MATLAB)

Identical to `quantile` (see `audit/findings/stats/quantile.md`):
- interpolation scheme mismatch (Type-7 vs MATLAB R2007a) — same
  MISMATCH that's flagged in PROGRESS
- `"all"` / vecdim throw
- `Method` N-V throws

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `prctile([1 2 3 4 5]', 50)` | `3` | `3` ✅ |
| `prctile([1:10]', [25 50 75])` | `[3 5.5 8]` | `[3.25 5.5 7.75]` ❌ |
| `prctile(A, 50, "all")` | `6` | THROWS |

## Recommended fixes

Apply the joint fix from `audit/findings/stats/quantile.md`. Once
`quantile` uses MATLAB's R2007a interpolation, `prctile` (which
delegates) gets parity for free.

Spec extension: regenerate `prctile.json` after the algorithm
swap. `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A — joint fix with `quantile`.

## Closed
- Closed in commit: PENDING (joint quantile/prctile/iqr fix)
- Closed date: 2026-05-06
- Notes: switched default interpolation to MATLAB R2025b "midpoint" (R2007a / Type-5, positions (k-0.5)/N). Method N-V pair accepts {midpoint|inclusive|exclusive|approximate}. Added 'all' string and full-flatten vecdim dispatch. Approximate method falls back to midpoint (no t-digest yet). Integer-n quantile form (quantile(A, n)) NOT supported — pass an explicit p vector. Partial vecdim NOT supported — only full-flatten coverage.
