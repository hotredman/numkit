# stats/ttest — ТЗ for completion

**Status:** open
**Priority:** high
**Effort:** medium
**Audited at commit:** 69fab7c
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/test/hypothesis.cpp:79` (`ttest`)
- Adapter: `libs/stats/src/test/hypothesis.cpp:1222` (`ttest_reg`)
- Spec: **none** (no `tools/parity/specs/ttest.json` exists)
- What works today:
  - `[h, p, ci, t] = ttest(x[, m, alpha, tail])` — vector input, scalar `m`
  - 4th output is the scalar `t`-statistic
  - `Tail` recognised as a positional string ('both'/'right'/'left')
  - Throws when `n < 2`

## MATLAB R2025b — actual behavior

Documented signatures (`help ttest`):

- `h = ttest(x)`
- `h = ttest(x, y)` — **paired-sample**: `y` is a vector matching `x`
- `h = ttest(x, y, Name, Value)` / `h = ttest(x, m, Name, Value)`
- `h = ttest(x, m)` — vs hypothesised mean
- `[h, p] = ttest(___)` / `[h, p, ci, stats] = ttest(___)`

**Name-value:** `Alpha`, `Dim`, `Tail` ('both'|'right'|'left').

**Inputs:** `x` may be a vector, matrix, or N-D array — when matrix, the
test is applied along the first non-singleton dimension (or `Dim`).

**Outputs:** the 4th output is a **structure** with fields `tstat`,
`df`, `sd`. For matrix input these fields hold per-column row vectors.

**Edge:** `n < 2` ⇒ returns `h=NaN, p=NaN` (does NOT throw); `NaN` in
data is dropped from the test (per-column when matrix).

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `ttest(x, y)` paired form (y vector) | runs paired t-test | adapter calls `args[1].toScalar()` ⇒ throws `Cannot convert double to scalar` | high |
| 2 | 4th output type | `stats` struct {tstat, df, sd} | scalar `tstat` | high |
| 3 | `ttest(x, 4, 'Alpha', 0.01)` | uses α=0.01 | **silently ignored** — α stays 0.05; CI is the 0.05-CI | high |
| 4 | matrix input | per-column vector outputs | only vector input supported (matrix currently treated as a single flat vector via `x.numel()`) | medium |
| 5 | `Dim` N-V | per-dim test | not supported | low |
| 6 | `n < 2` | returns `NaN`, `NaN` | throws | medium |

## Reference table (from probe)

Inputs:
```
x = [1.2 2.4 3.1 4.5 5.0 6.2 7.1]'
y = [0.8 1.9 2.7 4.0 4.5 5.7 6.4]'
M = [1 4 7; 2 5 8; 3 6 9; 4 7 10; 5 8 11]   % 5×3
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `[h,p,ci,st] = ttest(x)` | `h=1, p=0.0018357004, ci=[2.2674 6.1612]` | identical numbers ✅ |
| `st.tstat / st.df / st.sd` (basic) | `5.2966480891 / 6 / 2.1050958580` | only scalar `tstat=5.2966480891` (df, sd missing) |
| `[h,p,ci,st] = ttest(x, y)` paired | `h=1, p=1.1528e-5, ci=[0.4075 0.5925], tstat=13.2287565553` | THROWS |
| `[h,p,ci,st] = ttest(x, 4)` | `h=0, p=0.7967048575, ci=[2.2674 6.1612], tstat=0.2693210893` | identical ✅ |
| `ttest(x, 4, 'Alpha', 0.01)` ci | `[1.2645 7.1641]` | `[2.2674 6.1612]` ❌ (α ignored) |
| `ttest(M)` (matrix) | `h=[1 1 1], p=[0.0132 0.0011 0.0002], tstat=[4.243 8.485 12.728]` | misleading single-vector reduction |
| `ttest([1 2 3 NaN 5 6]')` | `h=1, p=0.0214606344, tstat=3.6663142889, df=4` | likely propagates NaN — needs probe |
| `ttest([3])` (n=1) | `h=NaN, p=NaN` | THROWS |

## Recommended fixes

1. **Fix paired form parsing.** When `args[1]` is a vector with
   `numel == numel(x)` (and not a scalar), treat it as `y` and compute
   paired diff `x - y` instead of `(x - m)`. MATLAB's heuristic: if
   `args[1]` is a non-scalar with the same shape as `x`, paired; if
   scalar (or empty), it's `m`.
2. **Replace 4th output with a struct.** Build a `Value::structure` and
   populate fields `tstat`, `df`, `sd`. For backward compatibility with
   any internal consumer that read it as a scalar — there isn't one in
   `libs/`, so a clean swap is fine.
3. **Fix Alpha/Tail name-value parsing.** Currently the adapter treats
   *all* string args from position 2 onward as positional `Tail`
   values, so `'Alpha', 0.01` parses as two failed `Tail` lookups and
   `Alpha` is never read. Switch to a proper N-V loop (mirror the
   pattern already used in `runstest_reg` / `ranksum_reg`):
   ```
   while (i + 1 < args.size()) {
     name = lower(args[i].toString());
     if      (name == "alpha") alpha = args[i+1].toScalar();
     else if (name == "tail")  tail  = parse_tail(args[i+1].toString(), Both);
     else if (name == "dim")   dim   = (int)args[i+1].toScalar();
     i += 2;
   }
   ```
4. **Matrix input: per-column reduction.** Detect when `x` has rows>1
   and cols>1, then loop columns and emit `h`, `p`, `ci`, struct
   fields as 1×ncols row vectors.
5. **Edge `n < 2`:** return `(h=NaN, p=NaN, ci=[NaN NaN])` instead of
   throwing.
6. **Spec creation:** `tools/parity/specs/ttest.json` does not exist
   yet — create it with fingerprint covering basic / paired / m-arg /
   Alpha / right-tail / matrix-input / NaN-data / n=1. `tol = 1e-9`.
7. **PROGRESS.md row update:** drop the trailing "returns (h, p, ci,
   tstat)" — replace with a description that includes the struct
   output, paired form, matrix-input, and Alpha/Tail/Dim N-V coverage.

## Out of scope for this ТЗ

- N-D array input (more than 2 dims) — deferred until matrix form lands.
- `Dim` keyword for non-default reduction — covered by gap #5 above
  but can be implemented incrementally.

## Closed (partial)
- Closed in commit: PENDING (joint ttest/ttest2 fix)
- Closed date: 2026-05-06
- Notes: ttest_reg now detects paired form (2nd arg is non-scalar vector) and runs paired t on x-y vs m=0. Both adapters now parse Alpha/Tail/Vartype as Name-Value (case-insensitive). ttest2 default Vartype switched from 'unequal' to 'equal' (pooled, matches MATLAB R2025b documented default). Dim N-V throws with parity-gap note.

REMAINING gaps (deferred):
- 4th output is still scalar tstat (not struct {tstat, df, sd})
- matrix / N-D input not supported (single-vector tests only)
- Dim Name-Value not implemented
- n < 2 throws (MATLAB returns NaN)
