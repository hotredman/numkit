# stats/vartestn — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** medium
**Audited at commit:** 69fab7c
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/test/hypothesis.cpp:698` (`vartestn`)
- Adapter: `libs/stats/src/test/hypothesis.cpp:1451` (`vartestn_reg`)
- Spec: `tools/parity/specs/vartestn.json`
- What works today:
  - `[p, stats] = vartestn(x, group[, 'Display', 'off'])`
  - Bartlett's k-sample variance equality test only
  - stats struct with `chisqstat`, `df`
  - Adapter silently ignores `TestType` and `Display` N-V
  - Throws if `x` and `group` lengths differ

## MATLAB R2025b — actual behavior

Documented signatures (`help vartestn`):

- `vartestn(x)` / `vartestn(x, Name, Value)`
- `vartestn(x, group)` / `vartestn(x, group, Name, Value)`
- `p = vartestn(___)` / `[p, stats] = vartestn(___)`

Name-value:
- `Display` — `'on'` (default) plots a box-plot, `'off'` suppresses
- `TestType` — `'Bartlett'` (default), `'LeveneQuadratic'`,
  `'LeveneAbsolute'`, `'BrownForsythe'`, `'OBrien'`

`stats` shape depends on TestType:
- Bartlett: `chisqstat`, `df`
- Levene/BF/OBrien: `fstat`, `df` (a 2-vector `[df_between df_within]`)

Input forms:
- `x` is a matrix (each column = group) when no `group` supplied
- `x` is a vector + grouping factor

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `'TestType', 'LeveneAbsolute'` | runs Levene's test (F-stat) | silently runs Bartlett — N-V key recognised but value ignored; numkit returns Bartlett's `chisqstat` | high |
| 2 | `'TestType', 'BrownForsythe'` | Brown-Forsythe variant of Levene (median-deviation) | silently runs Bartlett | high |
| 3 | `'TestType', 'OBrien'` | O'Brien's test | silently runs Bartlett | medium |
| 4 | `vartestn(x)` — matrix input, no group | tests across columns | not supported (adapter requires `args.size() >= 2`) | medium |
| 5 | stats struct shape per TestType | varies | always Bartlett shape | medium |

## Reference table (from probe)

Inputs:
```
xg = [3 5 4 7 8 6 9 10 11]'
g  = [1 1 1 2 2 2 3 3 3]'
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `[p,st] = vartestn(xg, g, 'Display', 'off')` | `p=1.0, chisqstat=0, df=2` | identical ✅ |
| `vartestn(xg, g, 'Display', 'off', 'TestType', 'LeveneAbsolute')` | `p=1.0, fstat=0, df=[2 6]` | `p=1.0, chisqstat=0, df=2` ❌ ran Bartlett |
| `vartestn(xg, g, 'Display', 'off', 'TestType', 'BrownForsythe')` | `p=1.0, fstat=0, df=[2 6]` | same incorrect Bartlett path |
| `vartestn(xg, g, 'Display', 'off', 'TestType', 'OBrien')` | `p=1.0, fstat=0, df=[2 6]` | same incorrect Bartlett path |

(Numerically zero in this contrived input; with real data the
wrong-test selection produces visibly different `p`.)

## Recommended fixes

1. **Implement Levene's test (Quadratic + Absolute):** for each group
   compute deviation `Z_ij = |x_ij - mean_i|` (Absolute) or
   `(x_ij - mean_i)²` (Quadratic), then run a one-way ANOVA on `Z`.
   Stat is `F`, df = `[k-1, N-k]`.
2. **Implement Brown-Forsythe:** as Levene-Absolute but with
   `Z_ij = |x_ij - median_i|` instead of mean.
3. **Implement O'Brien:** the documented O'Brien transformation —
   `Z_ij = ((n-1.5)·n·(x_ij - mean_i)² - 0.5·s_i²·(n-1)) / ((n-1)(n-2))`,
   then ANOVA on `Z`.
4. **Pluggable stats struct:** when TestType selects an F-based test,
   emit `{fstat, df}` where `df = [k-1, N-k]` as a 2-vector. Bartlett
   path keeps `{chisqstat, df}`.
5. **Matrix input form** — when `args.size() == 1` and `args[0]` is
   a matrix, treat each column as a group. Wrap `vartestn(x)` to
   internally reshape to `(values, group)` and dispatch.
6. **Spec extension:** existing `vartestn.json` only covers Bartlett.
   Add fingerprint entries for Levene-Quadratic, Levene-Absolute,
   Brown-Forsythe, O'Brien, plus the matrix-input form. `tol = 1e-9`.
7. **PROGRESS.md row update:** drop "Levene/BrownForsythe TestType
   deferred" — replace with full TestType coverage and matrix-input
   note.

## Out of scope for this ТЗ

- The `'Display','on'` box-plot path — MATLAB renders a figure;
  numkit's headless console doesn't, and that's fine (gracefully
  ignore `'on'` since `p` and `stats` are still returned).
