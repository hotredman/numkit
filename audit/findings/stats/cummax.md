# stats/cummax — ТЗ for completion

**Status:** open
**Priority:** high
**Effort:** medium
**Audited at commit:** 4f021db
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/builtin/src/language/arrays/matrix.cpp:1467` (`cummax`)
- Adapter: `libs/builtin/src/language/arrays/matrix.cpp:2169`
  (`cummax_reg` via `NK_CUM_REG` macro at line 2155)
- Spec: `tools/parity/specs/cummax.json`
- What works today:
  - `M = cummax(A[, dim])` — vector / 2-D / 3-D
  - Scans skip NaN by default (matches MATLAB R2018a+ `'omitnan'`)
  - SIMD prefix scan for `dim=1` contiguous case

## MATLAB R2025b — actual behavior

Documented signatures (`help cummax`):

- `M = cummax(A)`
- `M = cummax(A, dim)`
- `M = cummax(___, direction)` — `'forward'` (default) / `'reverse'`
- `M = cummax(___, nanflag)` — `'omitnan'` (default) / `'includenan'`

Both `direction` and `nanflag` are **positional strings** that may
appear in any order after the data + dim. Multiple may be combined,
e.g. `cummax(A, 'reverse', 'includenan')`.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `cummax(A, 'reverse')` | scans right-to-left | `args[1].toScalar()` ⇒ throws `Cannot convert char to scalar` | **high** |
| 2 | `cummax(A, 'omitnan')` (explicit) | same as default | throws | high |
| 3 | `cummax(A, 'includenan')` | propagates NaN forward | throws | high |
| 4 | combined `cummax(A, 'reverse', 'includenan')` | both | throws | high |

## Reference table (from probe)

Inputs: `A = [1 3 2 5 4 6 NaN 8 7 10]'`

| Inputs | MATLAB | numkit |
|---|---|---|
| `cummax(A)` | `[1 3 3 5 5 6 6 8 8 10]` | `[1 3 3 5 5 6 6 8 8 10]` ✅ |
| `cummax(A, 'reverse')` | `[10 10 10 10 10 10 10 10 10 10]` | THROWS |
| `cummax(A, 'omitnan')` | `[1 3 3 5 5 6 6 8 8 10]` | THROWS |
| `cummax(A, 'includenan')` | `[1 3 3 5 5 6 NaN NaN NaN NaN]` | THROWS |
| `cummax(A, 'reverse', 'includenan')` | `[NaN NaN NaN NaN NaN NaN NaN 10 10 10]` | THROWS |

## Recommended fixes

1. **Rewrite `cummax_reg` (and the `NK_CUM_REG` macro covering
   `cumprod`/`cummin`).** New parsing:
   ```cpp
   int dim = 0;
   bool reverse = false;
   bool includenan = false;  // default = omitnan
   for (size_t i = 1; i < args.size(); ++i) {
     if (args[i].isChar() || args[i].isString()) {
       std::string s = lower(args[i].toString());
       if      (s == "reverse")   reverse = true;
       else if (s == "forward")   reverse = false;
       else if (s == "includenan") includenan = true;
       else if (s == "omitnan")    includenan = false;
       else throw Error("cummax: unknown option '" + s + "'", ...);
     } else if (!args[i].isEmpty()) {
       dim = (int)args[i].toScalar();
     }
   }
   ```
2. **Implement `'reverse'`:** simplest — flip the slice along `dim`,
   run forward scan, flip again. Or add a reverse scan kernel.
3. **Implement `'includenan'`:** in the per-element op, propagate NaN
   instead of treating as identity. Once any NaN is seen along the
   axis, everything downstream is NaN.
4. **Spec extension:** `cummax.json` currently covers only the basic
   path. Add fingerprint for: forward+omit (= basic), reverse+omit,
   forward+include, reverse+include. `tol = 0` (exact integer/float
   reproducibility expected for integer input).
5. **PROGRESS.md row update:** if a comment exists, add the
   direction + nanflag coverage note.

## Out of scope for this ТЗ

- The 1-output-only API: MATLAB's `[M, I] = cummax(...)` 2-output
  form returning indices doesn't appear in the help — numkit can
  stay 1-output.
