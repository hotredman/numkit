# stats/signrank — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 69fab7c
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/test/hypothesis.cpp:1078` (`signrank`)
- Adapter: `libs/stats/src/test/hypothesis.cpp:1562` (`signrank_reg`)
- Spec: `tools/parity/specs/signrank.json`
- What works today:
  - `[p, h, stats] = signrank(x[, m | y][, alpha[, 'tail', t, 'method', m]])`
  - Default exact iff `n_eff <= 15`
  - Tie-corrected normal approximation when n_eff > 15
  - stats struct: `signedrank`, plus `zval` when method=approximate
  - Properly distinguishes `m` (scalar) vs `y` (vector) via
    `args[1].isScalar()`

## MATLAB R2025b — actual behavior

Documented signatures (`help signrank`):

- `p = signrank(x)`
- `p = signrank(x, y)` (paired)
- `p = signrank(x, m)` (vs hypothesised median)
- `p = signrank(x, y, Name, Value)` etc.
- `[p, h] = signrank(___)` / `[p, h, stats] = signrank(___)`

Name-value: `alpha`, `method`, `tail`. stats struct has `signedrank`
and (when approximate) `zval`.

## Gaps (numkit vs MATLAB)

**No major behavioural gap detected.** Numbers and struct shapes
match across all probed forms.

| # | Gap | Severity |
|---|---|---|
| 1 | Existing spec covers only basic; need paired / m-arg / right-tail / approximate / large-n entries | low (test coverage) |

## Reference table (from probe)

Inputs:
```
x = [1.2 2.4 3.1 4.5 5.0 6.2 7.1]'
y = [0.8 1.9 2.7 4.0 4.5 5.7 6.4]'
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `[p,h,st] = signrank(x)` | `p=0.0156250, h=1, signedrank=28` | identical ✅ |
| `signrank(x, y)` paired | `p=0.0156250, h=1, signedrank=28` | identical ✅ |
| `signrank(x, 4)` vs m | `p=0.8125000, h=0, signedrank=16` | identical ✅ |

## Recommended fixes

1. **Spec extension** — add fingerprint entries for paired, m=4,
   right-tail, large-n approximate (e.g. n=20 random sample under a
   fixed seed to force the normal-approx path). `tol = 1e-9`.
2. **PROGRESS.md row update:** unchanged — already accurate.

## Out of scope for this ТЗ

- The `'sign'` test variant — separate function (`signtest`).
