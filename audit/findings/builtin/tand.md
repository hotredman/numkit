# builtin/tand — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** a6e4264
**Audit date:** 2026-05-06

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | exact angles (`30°`, `45°`, `60°`, `90°`) | special-cases to exact value (e.g. `sind(30) = 0.5` exactly) | computes via `sin(deg2rad(x))` — 1-ULP off (e.g. `sind(30) = 0.4999999999999999`) | low (1-ULP precision) |

## Reference table

| Inputs | MATLAB | numkit |
|---|---|---|
| `sind(30)` | `0.5` (exact) | `0.4999999999999999` |
| `cosd(60)` | `0.5` (exact) | `0.5000000000000001` |
| `tand(45)` | `1.0` (exact) | `0.9999999999999999` |

## Recommended fixes

1. **Add nice-angle special-casing:** MATLAB's degree-form trig
   functions check for exact-multiple-of-30 / -45 / -90 inputs and
   return exact values. Implementation:
   ```
   if (x == round(x / 30) * 30 && (x mod 90) is 0/30/60) return exact_value
   ```
2. **Spec extension** — fingerprint over the nice angles + general
   case. `tol = 0` for nice angles after fix; `tol = 1e-15` for
   general.

## Out of scope for this ТЗ

- N/A.
