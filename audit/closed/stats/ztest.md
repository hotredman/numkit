# stats/ztest — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** 69fab7c
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/test/hypothesis.cpp:178` (`ztest`)
- Adapter: `libs/stats/src/test/hypothesis.cpp:1266` (`ztest_reg`)
- Spec: **none**
- What works today:
  - `[h, p, ci, z] = ztest(x, m, sigma[, alpha, tail])` — vector input
  - 4th output is the scalar `z`-value (matches MATLAB's `zval` —
    NOT a stats struct)
  - Throws for `sigma <= 0` or `n < 1`

## MATLAB R2025b — actual behavior

Documented signatures (`help ztest`):

- `h = ztest(x, m, sigma)`
- `h = ztest(x, m, sigma, Name, Value)`
- `[h, p] = ztest(___)` / `[h, p, ci, zval] = ztest(___)`

Name-value: `Alpha`, `Dim`, `Tail`. The 4th output is a **scalar**
named `zval` — no stats struct (unlike `ttest`). Matrix `x` ⇒
per-column.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `ztest(x, m, sigma, 'Alpha', 0.01)` | uses α=0.01 | **silently ignored** (same N-V parsing bug as `ttest`) | high |
| 2 | matrix input | per-column results | not supported (vector only) | medium |
| 3 | `Dim` N-V | per-dim test | not supported | low |
| 4 | `n == 0` | returns NaN | throws | low |

## Reference table (from probe)

Inputs:
```
x = [1.2 2.4 3.1 4.5 5.0 6.2 7.1]', m = 4, sigma = 1.5
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `[h,p,ci,zv] = ztest(x, 4, 1.5)` | `h=0, p=0.7054569861, ci=[3.1031 5.3255], zval=0.3779644730` | identical ✅ |
| `ztest(x, 4, 1.5, 'Tail', 'right')` | `p=0.3527284931, ci=[3.2817 Inf]` | identical ✅ (positional `'right'` works) |
| `ztest(x, 4, 1.5, 'Alpha', 0.01)` | wider CI | needs probe — Alpha N-V likely ignored as in `ttest_reg` |

## Recommended fixes

1. **Replace string-scan with proper N-V loop** (mirror `runstest_reg`
   pattern). The current code:
   ```
   double alpha = parse_alpha(args, 3, 0.05);
   for (size_t i = 3; i < args.size(); ++i)
     if (args[i].isChar() || args[i].isString())
       tail = parse_tail(args[i].toString(), TestTail::Both);
   ```
   `parse_alpha` skips strings, so `('Alpha', 0.01)` is dropped; the
   for-loop would also try to `parse_tail("Alpha")` which silently
   maps to `Both`. Switch to a proper key/value loop accepting
   `Alpha`, `Tail`, `Dim`.
2. **Matrix input: per-column reduction.**
3. **Edge `n == 0`:** return NaN instead of throwing.
4. **Spec creation:** `tools/parity/specs/ztest.json` — fingerprint
   over basic / right-tail / left-tail / alpha=0.01 / matrix-input.
5. **PROGRESS.md row update:** drop the terse "known-σ z-test"; note
   N-V coverage and matrix support.

## Out of scope for this ТЗ

- The `Dim` keyword for non-default reduction — covered conceptually
  by the matrix-input gap, can be added incrementally.

## Closed (partial)
- Closed in commit: PENDING (ztest NV fix)
- Closed date: 2026-05-06
- Notes: Adapter rewritten with proper Name-Value parsing for Alpha/Tail (case-insensitive). 'Dim' N-V throws with parity-gap message. 4th output is scalar zval (already matches MATLAB; no struct here). Verified vs MATLAB R2025b on 7 fingerprints (3-engine match). Matrix input + Dim still not supported.
