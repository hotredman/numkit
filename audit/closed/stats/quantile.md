# stats/quantile — ТЗ for completion

**Status:** open
**Priority:** **high** (PROGRESS already flags `correctness=MISMATCH`)
**Effort:** medium
**Audited at commit:** ba142e6
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/descriptive/descriptive.cpp:404` (`quantile`)
- Adapter: `libs/stats/src/descriptive/descriptive.cpp:1030` (`quantile_reg`)
- Spec: `tools/parity/specs/quantile.json` (note: PROGRESS flags
  `correctness=MISMATCH` for the bench input)
- What works today:
  - `Q = quantile(A, p[, dim])` — scalar dim
  - Vector or matrix `p`

## MATLAB R2025b — actual behavior

Documented signatures (`help quantile`):

- `Q = quantile(A, p)` — `p` in `[0, 1]`
- `Q = quantile(A, n)` — integer `n` ⇒ `n` evenly-spaced quantiles
  `((1:n)-0.5)/n`
- `Q = quantile(___, "all")`
- `Q = quantile(___, dim)`
- `Q = quantile(___, vecdim)`
- `Q = quantile(___, Method=method)` — `'exact'` (default) or
  `'approximate'`

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `quantile(A, n)` integer-n form | n evenly-spaced quantiles `((1:n)-0.5)/n` | numkit treats `n` as a single fractional `p` (passes through `quantile(p)` unchanged). Probe: `quantile([1:10], [0.25 0.5 0.75])` returns `[3.25 5.5 7.75]` vs MATLAB `[3 5.5 8]` — **DIFFERENT INTERPOLATION SCHEME** | high |
| 2 | `quantile(A, p, "all")` | full-flatten | throws | high |
| 3 | `quantile(A, p, [1 2])` (vecdim) | reduce | throws | high |
| 4 | `Method='approximate'` N-V | t-digest fast approx | throws | medium |
| 5 | per-slice interpolation rule | linear with `(k-0.5)/N` quantile positions (R2007a algorithm) | numkit: probe shows `quantile([1:10], 0.25) = 3.25`; MATLAB: `quantile([1:10], 0.25) = 3` | high — same root cause as MISMATCH in PROGRESS |

## Reference table (from probe)

Inputs: `A = [1 4 7; 2 5 8; 3 6 9; 4 7 10; 5 8 11]`

| Inputs | MATLAB | numkit |
|---|---|---|
| `quantile([1 2 3 4 5]', 0.5)` | `3` | `3` ✅ |
| `quantile([1:10]', [0.25 0.5 0.75])` | `[3 5.5 8]` | `[3.25 5.5 7.75]` ❌ |
| `quantile(A, 0.5)` | `[3 6 9]` | `[3 6 9]` ✅ (median agrees) |
| `quantile(A, 0.5, "all")` | `6` | THROWS |
| `quantile(A, 0.5, [1 2])` | `6` | THROWS |
| `quantile([1:100]', 0.5, 'Method', 'approximate')` | `50.5` | THROWS |

## Recommended fixes

1. **Switch interpolation to MATLAB's R2007a algorithm.** MATLAB
   places sample data points at quantile positions `((k-0.5)/N)` for
   `k = 1..N`. To find `quantile(A, p)`:
   - Sort `A`.
   - Map `p` to position `q = p · N + 0.5`.
   - Linearly interpolate between `A(floor(q))` and `A(ceil(q))`,
     clamping to the endpoints for `q ≤ 1` or `q ≥ N`.
   This produces `quantile([1:10], 0.25) = 3` (since at p=0.25,
   q = 3.0, exact at index 3 → A(3) = 3).
   The current numkit code appears to use the simpler
   `q = p · (N-1) + 1` linear-interp scheme (Type-7 in Hyndman &
   Fan), which gives `3.25` instead.
2. **Accept integer `n` as "n evenly-spaced quantiles":** when the
   user passes `quantile(A, 4)`, return the 4 quantiles at
   `(0.5, 1.5, 2.5, 3.5) / 4 = [0.125 0.375 0.625 0.875]`. Detect
   integer-valued scalar `p ≥ 1` (and `p == round(p)`).
3. **Adapter: type-dispatch dim** for `"all"` and vecdim
   (same fix as `var`/`std`/`median`).
4. **`Method='approximate'`:** implement t-digest or just point-fall-
   back to exact (no functional gap, just signature compatibility).
5. **Spec extension:** PROGRESS notes `correctness=MISMATCH`. Once
   the algorithm switches, regenerate `quantile.json` fingerprint
   with both single-`p` and multi-`p` cases. `tol = 1e-9`.

## Out of scope for this ТЗ

- The newer R2024a `'Method', 'tdigest'` mode — separate proposal.

## Closed
- Closed in commit: PENDING (joint quantile/prctile/iqr fix)
- Closed date: 2026-05-06
- Notes: switched default interpolation to MATLAB R2025b "midpoint" (R2007a / Type-5, positions (k-0.5)/N). Method N-V pair accepts {midpoint|inclusive|exclusive|approximate}. Added 'all' string and full-flatten vecdim dispatch. Approximate method falls back to midpoint (no t-digest yet). Integer-n quantile form (quantile(A, n)) NOT supported — pass an explicit p vector. Partial vecdim NOT supported — only full-flatten coverage.
