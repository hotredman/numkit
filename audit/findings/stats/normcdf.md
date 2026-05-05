# stats.dist/normcdf — ТЗ for completion

**Status:** open
**Priority:** **high**
**Effort:** small
**Audited at commit:** 8e48677
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/normal.cpp:~70` (`normcdf`)
- Adapter: `libs/stats/src/distributions/normal.cpp:165` (`normcdf_reg`)
- Spec: `tools/parity/specs/normcdf.json`
- `p = normcdf(X[, mu, sigma])` — matches MATLAB; `'upper'` flag
  silently ignored

## MATLAB R2025b — actual behavior

- `p = normcdf(x)`
- `p = normcdf(x, mu, sigma)`
- `p = normcdf(___, 'upper')` — returns `1 − Φ(x)` directly, with
  better numerical accuracy in the upper tail than computing
  `1 - normcdf(x)`

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | `normcdf(x, mu, sigma, 'upper')` — silently returns lower-tail value (extra arg ignored at adapter level) | **high — silent default divergence** |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `normcdf(0)` | `0.5` | `0.5` ✅ |
| `normcdf(1.96)` | `0.9750021049` | identical ✅ |
| `normcdf(1.96, 0, 1, 'upper')` | `0.0249978951` | `0.9750021049` ❌ |

## Recommended fixes

1. **Detect trailing `'upper'` string** in `normcdf_reg`. New
   parser:
   ```cpp
   bool upper = false;
   for (size_t i = 1; i < args.size(); ++i) {
     if (args[i].isChar() || args[i].isString()) {
       std::string s = lower(args[i].toString());
       if (s == "upper") upper = true;
       // else (mu, sigma already consumed positionally)
     }
   }
   ```
   When `upper`, compute `0.5 · erfc(z / √2)` directly (avoids the
   `1 − Φ(x)` subtraction loss in the upper tail).
2. **Spec extension** — add fingerprint with the `'upper'` flag.
   `tol = 1e-15` for normal range; `tol = 1e-300`-relative for
   far-upper-tail to test the precision improvement.

## Out of scope for this ТЗ

- N/A.
