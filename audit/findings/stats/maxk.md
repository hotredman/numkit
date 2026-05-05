# stats/maxk — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** ba142e6
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/descriptive/descriptive_extras.cpp:201`
  (`maxk`)
- Adapter: `libs/stats/src/descriptive/descriptive_extras.cpp:647`
  (`maxk_reg`)
- Spec: `tools/parity/specs/maxk.json`
- What works today:
  - `B = maxk(A, k[, dim])` — top-k along dim (or first non-singleton)

## MATLAB R2025b — actual behavior

Documented signatures (`help maxk`):

- `B = maxk(A, k)`
- `B = maxk(A, k, dim)`
- `B = maxk(___, 'ComparisonMethod', c)` — `'auto'` (default) /
  `'real'` / `'abs'`

`ComparisonMethod` controls how complex values are ordered:
- `'auto'`: ascending by `real` part if all-real-or-real-only-output,
  else by `abs`
- `'real'`: by real part
- `'abs'`: by magnitude

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `maxk(C, k, 'ComparisonMethod', 'abs')` | sort complex by `|z|` | adapter `args[2].toScalar()` ⇒ throws `Cannot convert char to scalar` | medium |
| 2 | complex input default ordering | `'auto'` (typically real-part) | needs probe; numkit's complex maxk returned `[4; 2]` from `[1+1i; 2-3i; 0.5; 4+0i]` — drops imaginary parts (or sorts by real?). MATLAB returns `[4+0i; 2-3i]` — same numbers but preserves complex type | low/medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `maxk([1 5 3 7 2 9 4]', 3)` | `[9; 7; 5]` | identical ✅ |
| `maxk(A, 2)` | `[5 8 11; 4 7 10]` | identical ✅ |
| `maxk(A, 2, 2)` | per-row top-2 | identical ✅ |
| `maxk([1+1i; 2-3i; 0.5; 4+0i], 2)` | `[4+0i; 2-3i]` | `[4; 2]` (loses imag) |
| `maxk(C, 2, 'ComparisonMethod', 'abs')` | `[2-3i; 4+0i]` | THROWS |

## Recommended fixes

1. **Adapter rewrite:** instead of `args[2].toScalar()` always,
   detect string `args[2]` and start a N-V loop:
   ```
   if (args[2].isChar() || args[2].isString()) {
     // start N-V loop here
   } else {
     dim = (int)args[2].toScalar();
     // then optional N-V from args[3..]
   }
   ```
2. **Implement `'ComparisonMethod'` for complex input.** When `'abs'`,
   sort by magnitude; when `'real'`, by real part; when `'auto'`,
   match MATLAB's heuristic (real-only ⇒ by real, mixed ⇒ by abs).
3. **Preserve complex type in output** — currently the result
   appears to be cast to real.
4. **Spec extension:** add fingerprint for ComparisonMethod paths,
   complex input. `tol = 0`.

## Out of scope for this ТЗ

- The 2-output `[B, I] = maxk(...)` index form (not in MATLAB doc).
