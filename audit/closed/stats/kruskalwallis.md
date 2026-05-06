# stats/kruskalwallis — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** medium
**Audited at commit:** 69fab7c
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/anova/anova.cpp:99` (`kruskalwallis`)
- Adapter: `libs/stats/src/anova/anova.cpp:265` (`kruskalwallis_reg`)
- Spec: `tools/parity/specs/kruskalwallis.json`
- What works today:
  - `[p, tbl, stats] = kruskalwallis(y, group[, 'off'])`
  - Tie-corrected H-statistic, p via χ²(k-1)
  - tbl is a 4×6 cell array {Source, SS, df, MS, Chi-sq, Prob>Chi-sq}
  - stats struct: `chi2stat`, `df`

## MATLAB R2025b — actual behavior

Documented signatures (`help kruskalwallis`):

- `p = kruskalwallis(x)` — matrix `x` (each column = group)
- `p = kruskalwallis(x, group)`
- `p = kruskalwallis(x, group, displayopt)` — `'on'` (default) / `'off'`
- `[p, tbl, stats] = kruskalwallis(___)`

stats struct has **five** fields:
- `gnames` — k×1 cell of group names (string form of unique labels)
- `n` — k×1 vector of per-group sample sizes
- `source` — fixed string `'kruskalwallis'`
- `meanranks` — k×1 vector of group mean ranks
- `sumt` — sum of `t³ - t` over tie-groups (used by `multcompare`)

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | stats struct | `{gnames, n, source, meanranks, sumt}` | `{chi2stat, df}` | medium (consumers like `multcompare` rely on the documented fields) |
| 2 | `kruskalwallis(x)` matrix-only form | infers groups from columns | adapter requires `args.size() >= 2` ⇒ throws | medium |

## Reference table (from probe)

Inputs:
```
xg = [3 5 4 7 8 6 9 10 11]'
g  = [1 1 1 2 2 2 3 3 3]'
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `[p, tbl, st] = kruskalwallis(xg, g, 'off')` | `p=0.0273237224`; tbl Chi-sq cell = `7.2`; df cell = `2` | identical p / tbl ✅ |
| `st.n` | `[3 3 3]` | missing — numkit's stats has `chi2stat=7.2`, `df=2` only |
| `st.meanranks` | `[2 5 8]` | missing |
| `st.gnames` | `{'1';'2';'3'}` (3×1 cell) | missing |
| `st.source` | `'kruskalwallis'` | missing |
| `st.sumt` | `0` (no ties in this input) | missing |

## Recommended fixes

1. **Extend `kruskalwallis` to compute and return** the five MATLAB
   stats fields. The data is already at hand inside the C++ impl:
   - `gnames`: derive from the bucket labels (sort ascending, format
     as strings — match MATLAB's `num2str` for numeric labels and
     pass-through for char/string).
   - `n`: per-bucket count.
   - `source`: literal `'kruskalwallis'`.
   - `meanranks`: `R[g] / ng[g]` (the per-group sum of ranks divided
     by group size).
   - `sumt`: `Σ (t³ - t)` over the recorded `tieGroupSizes`.
2. **Matrix-only form:** when `args.size() == 1` and `args[0]` is a
   matrix, expand into `(values, group)` where `group(i) = col-index
   of x(i)` and dispatch.
3. **Spec extension:** existing `kruskalwallis.json` doesn't capture
   stats fields. Add fingerprint:
   `[p, tbl_chisq, tbl_df, n(1), n(2), n(3),`
   ` meanranks(1), meanranks(2), meanranks(3),`
   ` sumt]`. `tol = 1e-9`.
4. **PROGRESS.md row update:** add note about full stats struct
   coverage and matrix-input form.

## Out of scope for this ТЗ

- The `'on'` displayopt path (renders ANOVA table figure). numkit
  console output remains text-only; gracefully ignore `'on'`.
- `multcompare` integration — separate function (currently ❌ in
  PROGRESS).

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-06
- Notes: `kruskalwallis_reg` extended with matrix-only form (infer
  groups from columns of `args[0]`) + full stats struct emission
  (`{gnames, n, source, meanranks, sumt}` plus retained legacy
  `chi2stat`, `df`). 5 TEST_F gtest + smoke .m added.
