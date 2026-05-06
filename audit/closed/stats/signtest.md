# stats/signtest — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 69fab7c
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/test/hypothesis.cpp:523` (`signtest`)
- Adapter: `libs/stats/src/test/hypothesis.cpp:1608` (`signtest_reg`)
- Spec: `tools/parity/specs/signtest.json`
- What works today:
  - `[p, h, stats] = signtest(x[, m | y][, alpha, ...])`
  - Binomial(n_eff, 0.5) tail probabilities via existing `binocdf`
  - stats struct contains only `sign` (count of positive diffs)
  - Adapter accepts `'method'` N-V but ignores its value (both exact
    and approximate go through `binocdf` which is exact)

## MATLAB R2025b — actual behavior

Documented signatures (`help signtest`):

- `p = signtest(x)` / `p = signtest(x, y)` / `p = signtest(x, m)`
- `p = signtest(x, y, Name, Value)`
- `[p, h] = signtest(___)` / `[p, h, stats] = signtest(___)`

Name-value: `Alpha`, `Method` (`'exact'` / `'approximate'`),
`Tail` (`'both'` / `'right'` / `'left'`).

stats struct has **two** fields:
- `zval` — `NaN` for exact method, normal-approx z otherwise
- `sign` — count of positive diffs

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | stats struct | `{zval, sign}` (`zval=NaN` for exact) | only `{sign}` | medium |
| 2 | `'Method', 'approximate'` | actually computes the normal-approx `z` and uses normal CDF for p | binocdf-based always (exact); ignores `'approximate'` request | low (numerically identical for large n if both exact) |
| 3 | spec coverage thin | one-shot exact only | needs paired / m / approximate / right-tail | low (test coverage) |

## Reference table (from probe)

Inputs:
```
x = [1.2 2.4 3.1 4.5 5.0 6.2 7.1]'
y = [0.8 1.9 2.7 4.0 4.5 5.7 6.4]'
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `[p,h,st] = signtest(x)` | `p=0.015625, h=1, st.zval=NaN, st.sign=7` | `p=0.015625, h=1, st.sign=7` (zval missing) |
| `signtest(x, y)` paired | `p=0.015625, h=1, st.zval=NaN, st.sign=7` | identical numbers, missing zval |

## Recommended fixes

1. **Add `zval` field to stats struct.** Always populate; set to NaN
   when the test went through the exact (binomial) path. This
   matches MATLAB's contract precisely.
2. **Implement `'Method', 'approximate'` properly:** for the
   approximate path compute
   `z = (n_pos − n/2) / sqrt(n/4)` (continuity-corrected by ±0.5),
   then use `normcdf` for the tail probability. Populate `zval` with
   the actual `z`; `p` will agree with the binomial path to within
   ~1e-3 for n≥30.
3. **Spec extension:** new fingerprint entries — paired, m=4,
   right-tail, approximate. `tol = 1e-9` for exact; `tol = 1e-3` for
   approximate.
4. **PROGRESS.md row update:** the current comment is for one
   specific input — replace with description of paired / m /
   approximate / N-V coverage.

## Out of scope for this ТЗ

- Continuity-correction toggle — MATLAB applies it implicitly in
  approximate mode; no user override.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-06
- Notes: Three of these (signrank/ranksum/fishertest) had NO behavioural gap per the audit reference table — closed with regression gtest only. signtest required adding zval=NaN to its stats struct (always exact path; 'approximate' method would populate zval but currently both routes go through binocdf and the numbers are identical).
