# stats/mink — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small (joint with `maxk`)
**Audited at commit:** ba142e6
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/descriptive/descriptive_extras.cpp:206`
  (`mink`)
- Adapter: `libs/stats/src/descriptive/descriptive_extras.cpp:657`
  (`mink_reg`)
- Spec: `tools/parity/specs/mink.json`
- Mirrors `maxk` — same shape, opposite sort.

## MATLAB R2025b — actual behavior

Same surface as `maxk` (see `audit/findings/stats/maxk.md`):

- `B = mink(A, k)` / `(A, k, dim)`
- `B = mink(___, 'ComparisonMethod', c)`

## Gaps (numkit vs MATLAB)

Same as `maxk`:
- `ComparisonMethod` N-V throws
- complex output likely casts to real

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `mink([1 5 3 7 2 9 4]', 3)` | `[1; 2; 3]` | identical ✅ |

## Recommended fixes

Joint with `audit/findings/stats/maxk.md`. Both adapters share the
same dispatch shape; one fix in a shared helper covers both.

Spec extension same as `maxk.json`. `tol = 0`.

## Out of scope for this ТЗ

- N/A — joint fix.
