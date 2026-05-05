# stats/vartest — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** 69fab7c
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/test/hypothesis.cpp:214` (`vartest`)
- Adapter: `libs/stats/src/test/hypothesis.cpp:1286` (`vartest_reg`)
- Spec: **none**
- What works today:
  - `[h, p, ci, T] = vartest(x, v[, alpha, tail])` — vector input
  - 4th output: scalar χ² statistic
  - Throws for `v <= 0` or `n < 2`

## MATLAB R2025b — actual behavior

Documented signatures (`help vartest`):

- `h = vartest(x, v)` / `h = vartest(x, v, Name, Value)`
- `[h, p] = vartest(___)` / `[h, p, ci, stats] = vartest(___)`

Name-value: `Alpha`, `Dim`, `Tail`. `stats` is a struct
{`chisqstat`, `df`}. Matrix `x` ⇒ per-column.

`v` is documented as **nonnegative** (not strictly positive); MATLAB
accepts `v=0` and returns finite `(h=1, p=0)` since `var/0 = inf`.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | 4th output | struct {chisqstat, df} | scalar `T` | high |
| 2 | `'Alpha', 0.01` N-V | uses α=0.01 | likely ignored (same bug as `ttest_reg`) | high |
| 3 | matrix input | per-column results | vector only | medium |
| 4 | `v == 0` | returns `(h=1, p=0)` | throws | low |

## Reference table (from probe)

Inputs:
```
x = [1.2 2.4 3.1 4.5 5.0 6.2 7.1]', v = 2.5
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `[h,p,ci,st] = vartest(x, 2.5)` | `h=0, p=0.2006378241, ci=[1.8401 21.4884]` | identical numbers ✅ |
| `st.chisqstat / st.df` | `10.6354285714 / 6` | only scalar `T=10.6354285714`, df missing |
| `vartest(x, 2.5, 'Tail', 'left')` | `p=0.8996810880` | identical (positional 'left' works) |
| `vartest(x, 0)` | `h=1, p=0, ci=...` | THROWS |

## Recommended fixes

1. **Replace 4th output with struct** {chisqstat, df}.
2. **Fix N-V parsing** (proper key/value loop) — same fix-shape as
   `ttest_reg` and `ztest_reg`.
3. **Matrix input: per-column reduction.**
4. **Accept `v == 0`:** return `(h=1, p=0)` (or 1.0 left-tail) without
   throwing.
5. **Spec creation:** `tools/parity/specs/vartest.json` — fingerprint
   over basic / left-tail / right-tail / alpha=0.01 / v=0 edge.
6. **PROGRESS.md row update:** swap the terse "chi-squared one-sample
   variance test" for a description of struct output, N-V coverage,
   and matrix support.

## Out of scope for this ТЗ

- N-D input.
