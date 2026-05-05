# stats/fishertest — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 69fab7c
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/test/hypothesis.cpp:575` (`fishertest`)
- Adapter: `libs/stats/src/test/hypothesis.cpp:1376` (`fishertest_reg`)
- Spec: `tools/parity/specs/fishertest.json`
- What works today:
  - `[h, p, stats] = fishertest(T[, 'Tail', t, 'Alpha', a])`
  - 2×2 contingency table, hypergeometric two-sided p-sums
    `P(X=k) ≤ P(X=obs)` rule
  - OR = a·d/(b·c); CI is Woolf log-OR ± z·SE
  - Returns stats struct with `OddsRatio`, `ConfidenceInterval`

## MATLAB R2025b — actual behavior

Documented signatures (`help fishertest`):

- `h = fishertest(x)` / `[h, p, stats] = fishertest(x)`
- `[___] = fishertest(x, Name, Value)`

Name-value: `Alpha`, `Tail` (`'both'` / `'right'` / `'left'`).
Output: 3 outs (h, p, stats); `stats` has fields `OddsRatio` and
`ConfidenceInterval`.

Input may be a 2×2 matrix or a 2×2 `table` object (categorical).

## Gaps (numkit vs MATLAB)

**No major behavioural gap detected.** Both basic and right-tail
modes produce identical numbers; struct shape matches.

| # | Gap | Severity |
|---|---|---|
| 1 | Input as 2×2 `table` object — numkit only accepts numeric matrix; MATLAB also accepts categorical `table` | low (not in core scope) |
| 2 | Existing spec covers only basic call; right-tail/left-tail/alpha-override not in fingerprint | low (test coverage) |

## Reference table (from probe)

Inputs:
```
T = [8 2; 1 5]
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `[h,p,st] = fishertest(T)` | `h=1, p=0.0349650350, OR=20, CI=[1.4162 282.4489]` | identical ✅ |
| `fishertest(T, 'Tail', 'right')` | `h=1, p=0.0244755245, OR=20` | identical ✅ |

## Recommended fixes

1. **Spec extension** — extend `fishertest.json` to add Tail=right
   and Tail=left and an Alpha=0.01 case. Fingerprint:
   `[p_b, OR_b, CIb(1), CIb(2),`
   ` p_r, p_l,`
   ` p_a, CIa(1), CIa(2)]`. `tol = 1e-9`.
2. **(Optional) Accept `table` input** — numkit core doesn't currently
   ship a `table` type with categorical columns, so this can be
   deferred until that infrastructure exists. Document the gap.
3. **PROGRESS.md row update:** unchanged — comment is already
   accurate.

## Out of scope for this ТЗ

- The `categorical`/`table` input form — deferred to broader OOP
  type support in core.
