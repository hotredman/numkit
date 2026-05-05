# signal/triang — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 0e043c5
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/windows/windows.cpp:136` (`triang`)
- Adapter: `libs/signal/src/windows/windows.cpp:537` (`triang_reg`)
- Spec: `tools/parity/specs/triang.json`
- 2nd arg silently ignored.

## MATLAB R2025b — actual behavior

Same surface as `bartlett`:

- `w = triang(L)` / `(L, typeName)` — only `'double'`/`'single'`,
  no sflag.

`triang` differs from `bartlett` in endpoint values (triang's
endpoints are non-zero; bartlett's are zero).

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | `'single'` typeName ignored | low |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `triang(8)` | `[0.125 0.375 0.625 0.875 0.875 0.625 0.375 0.125]` | identical ✅ |

## Recommended fixes

Apply the joint typeName-acceptance fix (see
`audit/findings/stats/bartlett.md`). `tol = 1e-7` for single.

## Out of scope for this ТЗ

- N/A — joint fix.
