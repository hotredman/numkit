# signal/bohmanwin — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 0e043c5
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/windows/windows.cpp:438` (`bohmanwin`)
- Adapter: `libs/signal/src/windows/windows.cpp:618` (`bohmanwin_reg`)
- Spec: `tools/parity/specs/bohmanwin.json`
- 2nd arg silently ignored.

## MATLAB R2025b — actual behavior

Same surface as `bartlett`:

- `w = bohmanwin(L)` / `(L, typeName)` — only `'double'`/`'single'`,
  no sflag.

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | `'single'` typeName ignored | low |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `bohmanwin(8)` | `[0 0.0707 0.4375 0.9104 0.9104 0.4375 0.0707 0]` | identical ✅ |

## Recommended fixes

Apply the joint typeName-acceptance fix (see
`audit/findings/stats/bartlett.md`). `tol = 1e-7` for single.

## Out of scope for this ТЗ

- N/A — joint fix.
