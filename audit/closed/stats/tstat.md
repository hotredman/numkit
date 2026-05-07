# stats.dist/tstat — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** 8e48677
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/students_t.cpp` (`tstat`)
- Spec: `tools/parity/specs/tstat.json`
- `[m, v] = tstat(nu)` — `m=0` always; `v=nu/(nu-2)` for `nu>2`.

## MATLAB R2025b — actual behavior

`[m, v] = tstat(nu)`:
- `nu > 2` ⇒ `m = 0`, `v = nu/(nu-2)`
- `nu == 2` ⇒ `m = 0`, `v = NaN` (variance is `Inf`, MATLAB returns NaN)
- `nu == 1` ⇒ **`m = NaN`, `v = NaN`** (Cauchy: mean is undefined)
- `nu < 1` ⇒ both NaN

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `tstat(1)` (Cauchy) | `m = NaN, v = NaN` | `m = 0, v = nan` (mean wrongly returned as 0) | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `tstat(5)` | `m=0, v=1.667` | identical ✅ |
| `tstat(2)` | `m=0, v=NaN` | `m=0, v=nan` ✅ |
| `tstat(1)` | `m=NaN, v=NaN` | `m=0, v=nan` ❌ |

## Recommended fixes

1. **Return `m = NaN` when `nu <= 1`:** Cauchy and below do not have
   a defined mean. Update the formula:
   ```cpp
   m = (nu > 1.0) ? 0.0 : NaN;
   v = (nu > 2.0) ? nu / (nu - 2.0) : NaN;
   ```
2. **Spec extension** — add fingerprint for nu ∈ {0.5, 1, 1.5, 2,
   5, Inf}. `tol = 0` (NaN comparison via the harness).

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-07
- Notes: Three fixes:
  - Vectorisation via emit_vec_stat_1arg (sweep 5dd32c38).
  - mean was always `0.0` even for nu ≤ 1 (Cauchy has no mean).
    Fixed: m = NaN for nu ≤ 1.
  - nu = 0 / nu < 0 returned (0, NaN); MATLAB gives NaN/NaN. Fixed.
  13-fingerprint spec; 5 TEST_F gtest + smoke. Parity OK numkit ↔
  MATLAB at tol=1e-12.
