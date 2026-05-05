# stats/cummin — ТЗ for completion

**Status:** open
**Priority:** high
**Effort:** medium
**Audited at commit:** 4f021db
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/builtin/src/language/arrays/matrix.cpp:1480` (`cummin`)
- Adapter: `libs/builtin/src/language/arrays/matrix.cpp:2170`
  (`NK_CUM_REG(cummin)`)
- Spec: `tools/parity/specs/cummin.json`
- What works today:
  - `M = cummin(A[, dim])` — vector / 2-D / 3-D
  - Scans skip NaN by default

## MATLAB R2025b — actual behavior

Documented signatures (`help cummin`):

- `M = cummin(A)` / `cummin(A, dim)`
- `M = cummin(___, direction)` — `'forward'` (default) / `'reverse'`
- `M = cummin(___, nanflag)` — `'omitnan'` (default) / `'includenan'`

Same surface as `cummax`; see `audit/findings/stats/cummax.md` for
the parser shape.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `cummin(A, 'reverse')` | right-to-left scan | throws `Cannot convert char to scalar` | high |
| 2 | `cummin(A, 'omitnan')` (explicit) | same as default | throws | high |
| 3 | `cummin(A, 'includenan')` | NaN propagated forward | throws | high |

## Reference table (from probe)

Inputs: `A = [1 3 2 5 4 6 NaN 8 7 10]'`

| Inputs | MATLAB | numkit |
|---|---|---|
| `cummin(A)` | `[1 1 1 1 1 1 1 1 1 1]` | identical ✅ |
| `cummin(A, 'reverse')` | `[1 2 2 4 4 6 7 7 7 10]` | THROWS |
| `cummin(A, 'includenan')` | `[1 1 1 1 1 1 NaN NaN NaN NaN]` | THROWS |

## Recommended fixes

Fix `NK_CUM_REG(cummin)` together with `cummax` and `cumprod` — the
same macro generates all three. See `audit/findings/stats/cummax.md`
"Recommended fixes" for the parser shape; `cummin` only differs in
which scalar op is bound (`std::min` vs `std::max`).

Spec extension same as `cummax.md` but with min semantics.

## Out of scope for this ТЗ

- `[M, I] = cummin(...)` 2-output index form — not documented.
