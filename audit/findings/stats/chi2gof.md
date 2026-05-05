# stats/chi2gof — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** large
**Audited at commit:** 69fab7c
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/test/hypothesis.cpp:653` (`chi2gof`)
- Adapter: `libs/stats/src/test/hypothesis.cpp:1408` (`chi2gof_reg`)
- Spec: `tools/parity/specs/chi2gof.json`
- What works today:
  - `[h, p, stats] = chi2gof(x, 'Frequency', O, 'Expected', E[, 'NParams', np[, 'Alpha', a]])`
  - chi² = Σ(O−E)²/E; df = K−1−np
  - stats struct has fields `chi2stat`, `df`
  - Adapter throws if neither Frequency nor Expected supplied
    (auto-binned form NOT implemented; acknowledged in PROGRESS)

## MATLAB R2025b — actual behavior

Documented signatures (`help chi2gof`):

- `h = chi2gof(x)` / `h = chi2gof(x, Name, Value)`
- `[h, p] = chi2gof(___)` / `[h, p, stats] = chi2gof(___)`

Name-value:
- `NBins` (10), `Ctrs`, `Edges` — auto-binning controls
- `CDF` — pdf/cdf to fit (function handle, cell array `{@cdf, p1, p2}`,
  or `ProbabilityDistribution` object)
- `Expected` — explicit expected counts (in lieu of CDF)
- `NParams` — number of fitted parameters (default: derived from CDF
  spec)
- `EMin` (5) — minimum expected count per bin (auto-merging of
  small-count tail bins)
- `Frequency` — explicit per-bin counts
- `Alpha` (0.05)

Output `stats` struct has:
- `chi2stat`, `df`, **`edges`**, **`O`**, **`E`** — six fields total
  (numkit produces only the first two).

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `chi2gof(x)` auto-binned | bins data into 10 default cells, fits N(μ̂, σ̂²) by default, computes O/E and reports test | adapter throws | high (acknowledged) |
| 2 | `'CDF', @fn` or `'CDF', {@fn, p1...}` | uses fn / parametrised fn for E | not supported | high |
| 3 | `'Edges'` / `'NBins'` / `'Ctrs'` | controls binning | silently ignored | medium |
| 4 | `'EMin'` auto-merge | merges low-count tail bins | not supported | medium |
| 5 | `stats.edges` / `stats.O` / `stats.E` | populated | missing | medium (spec coverage) |

## Reference table (from probe)

Inputs:
```
xn2 = randn(200, 1)    % under fixed seed (rng(42))
O = [10 12 8 14 6], E = [10 10 10 10 10]
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `chi2gof((1:5)', 'Frequency', O, 'Expected', E)` | `h=0, p=0.4060058497, chi2=4, df=4`; stats.edges=`[1 1.8 2.6 3.4 4.2 5]`, O=`[10 12 8 14 6]`, E=`[10 10 10 10 10]` | `h=0, p=0.4060058497, chi2=4, df=4`; stats has only `chi2stat`, `df` ✅ numbers / ❌ struct fields |
| `chi2gof(xn2)` (auto) | `h=0, p=0.8893, chi2=2.3083, df=6`; stats has 9 bins after auto-merge | THROWS |
| `chi2gof(xn2, 'CDF', @(x) normcdf(x, mean(xn2), std(xn2)))` | `h=0, p=0.9701, chi2=2.3083, df=8` (CDF unparametrised → df penalty differs) | THROWS |

## Recommended fixes

1. **Implement auto-binning.** Algorithm (matches MATLAB's documented
   default):
   - Default `NBins=10`. Compute equal-prob bins under the
     hypothesised CDF (default standard normal of `(x-mean)/std`),
     OR equal-width bins on `[min(x), max(x)]`.
   - Honour `Edges` if supplied; else `NBins`; else `Ctrs`.
   - Apply `EMin` (default 5): merge tail bins with `E < EMin` into
     adjacent bins until all `E ≥ EMin`.
   - Report final `edges`, `O`, `E` in the stats struct.
2. **Implement `CDF` argument.**
   - Function-handle: `E_i = N · (CDF(edge_{i+1}) - CDF(edge_i))`.
   - Cell-array `{@fn, p1, p2}`: same, with parametric call.
   - When CDF parameters are estimated from the data, the user is
     expected to declare via `NParams`; if not declared and CDF is a
     `cell` of length > 1, infer `NParams = numel(cell) - 1`.
3. **Populate `edges`, `O`, `E` in stats struct.** Always — even in
   the explicit-Frequency form, `edges` should be derivable from the
   first arg (or default to `1:K`).
4. **Spec extension:** `chi2gof.json` should add fingerprint entries
   for the auto-binned and CDF forms — under `rng(42)` to make
   numkit's output deterministic. `tol = 1e-9` for explicit form;
   `tol = 1e-6` for auto-binned form (binning ties may differ by
   single-sample tie-break).
5. **PROGRESS.md row update:** drop "Auto-binned distribution-fit
   form intentionally not implemented in this release" — replace with
   coverage notes.

## Out of scope for this ТЗ

- `ProbabilityDistribution` object form for `CDF` — needs OOP
  distribution type.
- 2-D goodness-of-fit (chi² of independence) — that's `crosstab`
  territory.
