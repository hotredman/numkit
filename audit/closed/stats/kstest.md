# stats/kstest — ТЗ for completion

**Status:** open
**Priority:** high
**Effort:** medium
**Audited at commit:** 69fab7c
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/test/hypothesis.cpp:362` (`kstest`) plus
  helpers `ks_pvalue` / `ks_critical` / `interp_cdf` at lines 317–358
- Adapter: `libs/stats/src/test/hypothesis.cpp:1324` (`kstest_reg`)
- Spec: **none**
- What works today:
  - `[h, p, ksstat, cv] = kstest(x[, cdf_matrix, alpha, tail])`
  - CDF accepted as a **positional** 2nd arg (a 2-column matrix or
    empty for standard-normal default)
  - Asymptotic Smirnov-series for `p`; bisection on the same series
    for `cv`
  - `Tail` parsed as positional ('both'/'right'/'left')
  - Throws on empty `x`

## MATLAB R2025b — actual behavior

Documented signatures (`help kstest`):

- `h = kstest(x)`
- `h = kstest(x, Name, Value)` — **CDF is a name-value parameter**,
  not positional
- `[h, p] = kstest(___)` / `[h, p, ksstat, cv] = kstest(___)`

Name-value:
- `Alpha`
- `CDF` — matrix or `ProbabilityDistribution` object
- `Tail` — `'unequal'` (default), `'larger'`, `'smaller'`. **Not**
  `both/right/left`!

`p` is computed by exact one-sample distribution (small `n`) or the
asymptotic series (large `n`); the small-sample table values differ
from numkit's pure-asymptotic result.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `kstest(x, 'Tail', 'larger')` | runs one-sided test | adapter parses `'Tail'` as a tail-name (unknown ⇒ Both) AND interprets it as a positional `cdf` matrix (a 4-char string passes `cdf.numel() >= 2`); result: D=97 nonsense, p≈0 | **critical** |
| 2 | `kstest(x, 'CDF', M)` | uses M as reference CDF | adapter sees `'CDF'` string at position 1 → tries `args[1].toScalar()` (via the `cdf` capture path) and throws `Cannot convert double to scalar` | **critical** |
| 3 | Tail values `'larger'`/`'smaller'` | maps to D⁺ / D⁻ | numkit's `parse_tail` only knows `both/right/left`; unrecognised ⇒ silently `Both` | high |
| 4 | exact `p` for small `n` | uses table | always asymptotic — slight numeric divergence; e.g. n=10 basic call: numkit p=0.2970 vs MATLAB p=0.2421 | medium |
| 5 | empty `x` | needs probe — `kstest([])` MATLAB error: "Input 'X' must be a vector" | throws too ✅ | none |
| 6 | NaN handling | drops NaN | needs probe | unknown |

## Reference table (from probe)

Inputs:
```
xn = [-0.5 0.3 0.7 1.1 -0.2 0.1 -0.4 0.8 -0.1 0.5]'
xx = (-3:0.1:3)'
cdfM = [xx normcdf(xx, 0.5, 1.0)]
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `[h,p,D,cv] = kstest(xn)` | `h=0, p=0.2421, D=0.3085, cv=0.4093` | `h=0, p=0.2970, D=0.3085, cv=0.4295` (D matches; p / cv from asymptotic series, MATLAB uses table) |
| `kstest(xn, 'Tail', 'larger')` | `h=0, p=0.6377, D=0.1357, cv=0.3687` | `h=1, p=0, D=97, cv=0.4295` ❌ corruption |
| `kstest(xn, 'Tail', 'smaller')` | `h=0, p=0.1211, D=0.3085, cv=0.3687` | (same corruption likely) |
| `kstest(xn, 'CDF', cdfM)` | `h=0, p=0.3377, D=0.2821, cv=0.4093` | THROWS: `Cannot convert double to scalar` |
| `kstest(xn, cdfM)` (positional) | n/a — MATLAB rejects | numkit accepts: `h=0, p=0.4038, D=0.2821` |
| `kstest(xn, 'Alpha', 0.01)` | `h=0, p=0.2421, D=0.3085, cv=0.4889` | needs probe — likely Alpha N-V also broken |

## Recommended fixes

1. **Rewrite `kstest_reg` N-V parser.** Drop positional CDF / alpha /
   tail support entirely (it is non-MATLAB and is the root cause of
   the corruption). New shape:
   ```
   Value cdf;            // empty default
   double alpha = 0.05;
   TestTail tail = TestTail::Both;
   for (size_t i = 1; i + 1 < args.size(); i += 2) {
     name = lower(args[i].toString());
     if      (name == "alpha") alpha = args[i+1].toScalar();
     else if (name == "cdf")   cdf   = args[i+1];
     else if (name == "tail")  tail  = parse_kstest_tail(args[i+1].toString());
   }
   ```
   Note: MATLAB's `kstest` rejects positional `cdf` — but for backward
   compatibility within numkit it can be kept *only* if `args[1]` is a
   numeric matrix, not a string (the current bug is the reverse: any
   non-numeric is mistreated).
2. **New `parse_kstest_tail` helper:** accept `unequal` ⇒ Both,
   `larger` ⇒ Right (D⁺), `smaller` ⇒ Left (D⁻). Keep `both/right/left`
   as legacy aliases.
3. **Small-sample exact `p`:** add the Birnbaum table or the exact
   one-sample distribution for `n ≤ 35`. This is what produces the
   `0.2421` vs `0.2970` divergence on `n=10`.
4. **Spec creation:** `tools/parity/specs/kstest.json` —
   fingerprint over basic / Tail=larger / Tail=smaller / CDF-NV /
   Alpha=0.01. `tol = 1e-7` (asymptotic vs exact may persist a small
   gap on `p` for medium `n`).
5. **PROGRESS.md row update:** drop "one-sample KS via asymptotic
   Smirnov series"; note Tail-name + CDF-NV + small-n exact coverage.

## Out of scope for this ТЗ

- `ProbabilityDistribution` object form for `CDF` — numkit doesn't
  have the OOP distribution type yet.

## Closed
- Closed in commit: PENDING (joint kstest/kstest2 fix)
- Closed date: 2026-05-06
- Notes: parse_tail extended with kstest aliases (unequal/larger/smaller -> Both/Right/Left). kstest_reg + kstest2_reg rewritten for proper Name-Value parsing ('Alpha', val | 'Tail', val) plus legacy positional alpha+tail. The 4th `cv` output is preserved (extra vs MATLAB; harmless to scripts using only first 3 outputs). 9 gtest + 3-engine parity confirm.
