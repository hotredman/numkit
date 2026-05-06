# stats/ttest2 — ТЗ for completion

**Status:** open
**Priority:** high
**Effort:** medium
**Audited at commit:** 69fab7c
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/test/hypothesis.cpp:132` (`ttest2`)
- Adapter: `libs/stats/src/test/hypothesis.cpp:1241` (`ttest2_reg`)
- Spec: **none**
- What works today:
  - `[h, p, ci, t] = ttest2(x, y[, alpha])` (positional alpha)
  - N-V `Tail`, `Vartype`/`VarType`, `Alpha` (correctly parsed in this
    adapter — unlike `ttest_reg`)
  - **Default `Vartype = "unequal"`** (Welch)
  - 4th output is scalar `t`-statistic
  - Throws when `nx < 2 || ny < 2`

## MATLAB R2025b — actual behavior

Documented signatures (`help ttest2`):

- `h = ttest2(x, y)` / `h = ttest2(x, y, Name, Value)`
- `[h, p] = ttest2(___)` / `[h, p, ci, stats] = ttest2(___)`

**Default `Vartype = 'equal'`** (pooled-variance), NOT Welch.

`Alpha`, `Dim`, `Tail`, `Vartype` are name-value only.

`stats` is a struct:
- `tstat`, `df`, `sd` — `sd` is **scalar pooled** when `Vartype='equal'`,
  **2-vector** `[sd_x sd_y]` when `Vartype='unequal'`.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | Default `Vartype` | `'equal'` (pooled) | `'unequal'` (Welch) | **high** — silent numeric divergence on every default call |
| 2 | 4th output | `stats` struct {tstat, df, sd} | scalar `t` | high |
| 3 | matrix input | per-column | not supported | medium |
| 4 | `Dim` N-V | per-dim test | not supported | low |
| 5 | `nx < 2 \|\| ny < 2` | returns `NaN` | throws | medium |

## Reference table (from probe)

Inputs:
```
x = [1.2 2.4 3.1 4.5 5.0 6.2 7.1]'
y = [0.8 1.9 2.7 4.0 4.5 5.7 6.4]'
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `[h,p,ci,st] = ttest2(x, y)` (default) | `h=0, p=0.6589083016, ci=[-1.9070 2.9070], tstat=0.4526030251, df=12, sd=2.0667434702` | `p=0.6589194678, ci=[-1.9074 2.9074], tstat=0.4526030251` (Welch path; df=11.98 internally) |
| `ttest2(x, y, 'Vartype', 'equal')` | matches MATLAB default above | `p=0.6589083016, ci=[-1.9070 2.9070]` ✅ |
| `ttest2(x, y, 'Vartype', 'unequal')` | `p=0.6589194678, ci=[-1.9074 2.9074], tstat=0.4526030251, df=11.9831861962, sd=[2.1051 2.0277]` | matches Welch numbers ✅ |

Note the divergence is **small** here (samples almost identical), but
when group sizes differ a default-call mismatch produces visibly
different `p`-values. Default-mode silent divergence is the most
dangerous parity bug class.

## Recommended fixes

1. **Flip default `Vartype` to `'equal'`** (pooled-variance). This
   restores parity with MATLAB on every default call. Confirm via probe
   reference: `(x,y)` ⇒ `p=0.6589083016`.
2. **Replace 4th output with struct** {tstat, df, sd}. `sd` is scalar
   when pooled, 2-vector when unequal — match the per-mode shape.
3. **Matrix input: per-column reduction.** Loop columns and emit
   row-vector outputs.
4. **`nx < 2 \|\| ny < 2`:** return `NaN` instead of throwing.
5. **Spec creation:** `tools/parity/specs/ttest2.json` does not exist;
   create with fingerprint over `(x,y)` default, `Vartype='equal'`,
   `Vartype='unequal'`, `Tail` variants, alpha overrides, plus the
   per-mode `sd` shape. `tol = 1e-9`.
6. **PROGRESS.md row update:** the trailing comment mentions Welch as
   default — revise to say `equal` is the default per MATLAB; both
   modes supported.

## Out of scope for this ТЗ

- The N-D array input case (more than matrix). Defer until matrix
  form is in place.

## Closed (partial)
- Closed in commit: PENDING (joint ttest/ttest2 fix)
- Closed date: 2026-05-06
- Notes: ttest_reg now detects paired form (2nd arg is non-scalar vector) and runs paired t on x-y vs m=0. Both adapters now parse Alpha/Tail/Vartype as Name-Value (case-insensitive). ttest2 default Vartype switched from 'unequal' to 'equal' (pooled, matches MATLAB R2025b documented default). Dim N-V throws with parity-gap note.

REMAINING gaps (deferred):
- 4th output is still scalar tstat (not struct {tstat, df, sd})
- matrix / N-D input not supported (single-vector tests only)
- Dim Name-Value not implemented
- n < 2 throws (MATLAB returns NaN)
