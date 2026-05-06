# stats/vartest2 — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** 69fab7c
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/test/hypothesis.cpp:263` (`vartest2`)
- Adapter: `libs/stats/src/test/hypothesis.cpp:1305` (`vartest2_reg`)
- Spec: **none**
- What works today:
  - `[h, p, ci, F] = vartest2(x, y[, alpha, tail])` — vector input
  - 4th output: scalar F-statistic
  - Throws for `nx<2 || ny<2 || var(y)==0`

## MATLAB R2025b — actual behavior

Documented signatures (`help vartest2`):

- `h = vartest2(x, y)` / `h = vartest2(x, y, Name, Value)`
- `[h, p] = vartest2(___)` / `[h, p, ci, stats] = vartest2(___)`

Name-value: `Alpha`, `Dim`, `Tail`. `stats` is a struct
{`fstat`, `df1`, `df2`}. Matrix `x`/`y` ⇒ per-column.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | 4th output | struct {fstat, df1, df2} | scalar `F` | high |
| 2 | `'Alpha', 0.01` N-V | uses α=0.01 | likely ignored (adapter has same shape as `ttest_reg`) | high |
| 3 | matrix input | per-column results | vector only | medium |

## Reference table (from probe)

Inputs:
```
x = [1.2 2.4 3.1 4.5 5.0 6.2 7.1]'
y = [0.8 1.9 2.7 4.0 4.5 5.7 6.4]'
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `[h,p,ci,st] = vartest2(x, y)` | `h=0, p=0.9298315564, ci=[0.1852 6.2727]` | identical numbers ✅ |
| `st.fstat / st.df1 / st.df2` | `1.0778318277 / 6 / 6` | only scalar `F=1.0778318277` |

## Recommended fixes

1. **Replace 4th output with struct** {fstat, df1, df2}.
2. **Fix N-V parsing** — proper key/value loop accepting Alpha/Tail.
3. **Matrix input: per-column reduction.**
4. **Spec creation:** `tools/parity/specs/vartest2.json` — fingerprint
   over basic / right-tail / left-tail / alpha=0.01.
5. **PROGRESS.md row update:** drop the terse "F-test for equality of
   variances"; note struct output, N-V, matrix support.

## Out of scope for this ТЗ

- N-D input.

## Closed (partial)
- Closed in commit: PENDING (joint vartest/vartest2 fix)
- Closed date: 2026-05-06
- Notes: Adapter rewritten with proper Name-Value parsing for Alpha/Tail (case-insensitive). 'Dim' N-V throws with parity-gap message. Verified vs MATLAB R2025b on 10 fingerprints (3-engine match). 4th output remains scalar (struct deferred); matrix input + Dim still not supported.
