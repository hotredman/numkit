# stats/ranksum — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 69fab7c
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/test/hypothesis.cpp:934` (`ranksum`)
- Adapter: `libs/stats/src/test/hypothesis.cpp:1523` (`ranksum_reg`)
- Spec: `tools/parity/specs/ranksum.json`
- What works today:
  - `[p, h, stats] = ranksum(x, y[, alpha[, 'tail', t, 'method', m]])`
  - Default exact iff `nx_eff < 10 && ny_eff < 10`
  - Tie-corrected normal approximation with continuity correction
  - stats struct: `ranksum`, plus `zval` when method=approximate

## MATLAB R2025b — actual behavior

Documented signatures (`help ranksum`):

- `p = ranksum(x, y)`
- `[p, h] = ranksum(x, y)` / `[p, h, stats] = ranksum(x, y)`
- `[___] = ranksum(x, y, Name, Value)`

Name-value: `alpha`, `method`, `tail`. stats struct has `ranksum`
and (when approximate) `zval`.

## Gaps (numkit vs MATLAB)

**No major behavioural gap detected.** Numbers match; struct shape
matches; conditional zval matches.

| # | Gap | Severity |
|---|---|---|
| 1 | Existing spec covers only basic call; alpha / right-tail / left-tail / approximate-method / large-n not in fingerprint | low (test coverage) |
| 2 | NaN handling — MATLAB drops NaN; numkit also drops NaN ✓ — confirm via probe | none |

## Reference table (from probe)

Inputs:
```
x = [1.2 2.4 3.1 4.5 5.0 6.2 7.1]'
y = [0.8 1.9 2.7 4.0 4.5 5.7 6.4]'
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `[p,h,st] = ranksum(x, y)` | `p=0.6445221445, h=0, ranksum=56.5` | identical ✅ |
| `ranksum(x, y, 'tail', 'right')` | `p=0.3222610723, h=0, ranksum=56.5` | identical ✅ |

## Recommended fixes

1. **Spec extension** — add fingerprint entries for tail-right,
   tail-left, alpha=0.01, method='approximate' (force normal-approx
   on the same small-n inputs). `tol = 1e-9`.
2. **PROGRESS.md row update:** unchanged — already accurate.

## Out of scope for this ТЗ

- Adding the legacy positional `tail` 4th-arg form — MATLAB's docs
  only list N-V; numkit already supports both, no need to remove.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-06
- Notes: Three of these (signrank/ranksum/fishertest) had NO behavioural gap per the audit reference table — closed with regression gtest only. signtest required adding zval=NaN to its stats struct (always exact path; 'approximate' method would populate zval but currently both routes go through binocdf and the numbers are identical).
