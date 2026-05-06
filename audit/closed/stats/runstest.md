# stats/runstest — ТЗ for completion

**Status:** open
**Priority:** high
**Effort:** medium
**Audited at commit:** 69fab7c
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/test/hypothesis.cpp:832` (`runstest`)
- Adapter: `libs/stats/src/test/hypothesis.cpp:1479` (`runstest_reg`)
- Spec: `tools/parity/specs/runstest.json`
- What works today:
  - `[h, p, stats] = runstest(x[, v[, 'Alpha', a, 'Tail', t, 'Method', m]])`
  - Default `v = median(x)`; values equal to `v` are dropped
  - Exact distribution by default (combinatorial PMF); approximate
    uses continuity-corrected normal
  - stats struct: `nruns`, `n1`, `n0`, optionally `z` (when method
    is approximate)

## MATLAB R2025b — actual behavior

Documented signatures (`help runstest`):

- `h = runstest(x)`
- `h = runstest(x, v)` — explicit reference value
- `h = runstest(x, 'ud')` — **up-down** runs (consecutive ascents /
  descents instead of above/below threshold)
- `h = runstest(___, Name, Value)`
- `[h, p, stats] = runstest(___)`

Name-value: `Alpha` (0.05), `Method` (`'exact'` / `'approximate'`),
`Tail` (`'both'` / `'right'` / `'left'`).

`stats` struct: `nruns`, `n1`, `n0`, plus `z` when approximate. For
the `'ud'` form numkit's struct shape is identical.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `runstest(x, 'ud')` | up-down runs test (counts sign changes of `diff(x)`) | adapter sees `'ud'` as a string and enters the N-V-pair loop expecting `'ud', value`; `'ud'` is not a known name so silently ignored. Then `args.size() == 2` but `i+1 < args.size()` is false (i=2, args.size()=2), so loop exits. Test runs with default `v = median(x)`. | **high** — silent wrong test |
| 2 | `runstest(x)` empty data | NaN (probably) | needs probe; impl returns `n1==0 || n0==0 ⇒ NaN` ✓ likely match | low |

## Reference table (from probe)

Inputs:
```
x  = [1.2 2.4 3.1 4.5 5.0 6.2 7.1]'
xs = [-1 1 -1 1 -1 1 -1 1]'
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `[h,p,st] = runstest(xs)` | `h=0, p=0.0571428571, nruns=8, n1=4, n0=4, z=1.9094` | identical ✅ |
| `runstest(x, 'ud')` | `h=1, p=0.0007936508, nruns=1, n1=6, n0=0` | `h=0, p=0.2, nruns=4, n1=4, n0=3` (ran median-based test instead) ❌ |

## Recommended fixes

1. **Detect the `'ud'` positional flag** before entering the N-V loop.
   ```
   bool ud_mode = false;
   size_t i = 1;
   if (i < args.size() && (args[i].isChar() || args[i].isString())) {
     std::string s = args[i].toString();
     if (s == "ud" || s == "UD") { ud_mode = true; ++i; }
   } else if (i < args.size() && !args[i].isEmpty()) {
     v = args[i].toScalar(); ++i;
   }
   ```
   When `ud_mode` is true, dispatch to a separate `runstest_ud`
   implementation:
   - Convert `x` to `sign(diff(x))`: `+1` for ascent, `-1` for
     descent. Drop zero-diff entries.
   - Treat the resulting sequence as the binary input for the same
     runs-counting machinery (so `n1 = #ascents`, `n0 = #descents`).
   - Apply exact / approximate p-value the same way.
2. **Spec extension:** add `runstest(x, 'ud')` fingerprint:
   `[p_ud, nruns_ud, n1_ud, n0_ud]`. `tol = 1e-9`.
3. **PROGRESS.md row update:** add note that `'ud'` mode is
   supported; today's comment doesn't acknowledge this missing form.

## Out of scope for this ТЗ

- 'Method', 'approximate-tied' — the documented MATLAB API is just
  exact / approximate; no third method.

## Closed (partial)
- Closed in commit: PENDING
- Closed date: 2026-05-06
- Notes: Adapter rewritten to recognise positional 'ud' string for up-down test. Implementation: pre-transform x → sign(diff(x)) and run standard runs test with v=0. Standard cases match MATLAB. Edge case (monotonic input → n0=0 or n1=0) returns NaN p-value — MATLAB returns small finite p via exact pmf; current numkit impl short-circuits on degenerate counts. Documented as remaining gap.
