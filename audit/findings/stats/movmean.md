# stats/movmean — ТЗ for completion

**Status:** open
**Priority:** **critical**
**Effort:** large
**Audited at commit:** 4f021db
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/moving/moving.cpp:252` (`movmean`)
- Adapter: `libs/stats/src/moving/moving.cpp:450` (`movmean_reg`)
- Window decoder: `libs/stats/src/moving/moving.cpp:48` (`decodeWindow`)
- Spec: `tools/parity/specs/movmean.json`
- What works today:
  - `Y = movmean(X, k)` — symmetric window centred on each point
  - `Y = movmean(X, [kb kf])` — asymmetric window
  - `Y = movmean(X, k, dim)` — explicit dimension
  - Vector / 2-D / 3-D input
  - Default endpoint behaviour: **shrink** (windows near edges
    truncated to in-range elements) — matches MATLAB default
  - Default NaN behaviour: **NaN propagated** (any NaN in window ⇒
    output NaN at that position)

## MATLAB R2025b — actual behavior

Documented signatures (`help movmean`):

- `M = movmean(A, k)`
- `M = movmean(A, [kb kf])`
- `M = movmean(___, dim)`
- `M = movmean(___, nanflag)` — `'omitnan'` (**default since R2018a**)
  / `'includenan'`
- `M = movmean(___, Name, Value)` — `Endpoints`, `SamplePoints`

Name-Value:
- `Endpoints`: `'shrink'` (default), `'discard'` (output shorter by
  k-1), `'fill'` (NaN at edges), or scalar fill value
- `SamplePoints`: vector of non-uniform sample positions; window is
  measured in sample-position units, not index counts

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | **default NaN behaviour** | `'omitnan'` — drops NaN from each window | `'includenan'` — NaN poisons the window | **CRITICAL — silent default-mode divergence on every NaN-containing call** |
| 2 | `movmean(A, 3, 'omitnan')` explicit | drop NaN | `args[2].toScalar()` ⇒ throws `Cannot convert char to scalar` | high |
| 3 | `movmean(A, 3, 'includenan')` explicit | poison | throws | high |
| 4 | `'Endpoints', 'discard'` | output length = N - k + 1 | throws | high |
| 5 | `'Endpoints', 'fill'` | NaN at edges (no shrink) | throws | high |
| 6 | `'Endpoints', <scalar>` | scalar fill at edges | throws | high |
| 7 | `'SamplePoints', t` | non-uniform window | throws | medium |
| 8 | `movmean(X, 0)` | throws "Window length must be finite, positive" | accepts (returns identity) | low |

## Reference table (from probe)

Inputs:
```
A  = [1 3 2 5 4 6 NaN 8 7 10]'
A2 = (1:9)'
t  = [0 1 2 3 4 5 7 8 9]'   % non-uniform sample positions
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `movmean(A2, 3)` | `[1.5 2 3 4 5 6 7 8 8.5]` | identical ✅ |
| `movmean(A2, [1 1])` | `[1.5 2 3 4 5 6 7 8 8.5]` | identical ✅ |
| `movmean(A2, [2 0])` | `[1 1.5 2 3 4 5 6 7 8]` | identical ✅ |
| `movmean(A2, [0 2])` | `[2 3 4 5 6 7 8 8.5 9]` | identical ✅ |
| `movmean(A, 3)` (default — NaN data) | `[2 2 3.333 3.667 5 5 7 7.5 8.333 8.5]` (omit) | `[2 2 3.333 3.667 5 NaN NaN NaN 8.333 8.5]` (include) ❌ |
| `movmean(A, 3, 'omitnan')` | `[2 2 3.333 3.667 5 5 7 7.5 8.333 8.5]` | THROWS |
| `movmean(A, 3, 'includenan')` | `[2 2 3.333 3.667 5 NaN NaN NaN 8.333 8.5]` | THROWS |
| `movmean(A2, 3, 'Endpoints', 'discard')` | length 7: `[2 3 4 5 6 7 8]` | THROWS |
| `movmean(A2, 3, 'Endpoints', 'fill')` | `[NaN 2 3 4 5 6 7 8 NaN]` | THROWS |
| `movmean(A2, 3, 'Endpoints', 0)` | `[1 2 3 4 5 6 7 8 5.667]` | THROWS |
| `movmean(A2, 2, 'SamplePoints', t)` | `[1 1.5 2.5 3.5 4.5 5.5 7 7.5 8.5]` | THROWS |

## Recommended fixes

1. **Flip the default NaN behaviour to `omitnan`** (MATLAB R2018a+
   default). This is the highest-impact fix: every script that
   processes NaN-containing data currently gets a different result
   than MATLAB silently. Implementation: change the default reducer
   to skip NaN inputs; the explicit `'includenan'` mode preserves
   the current poison behaviour.
2. **Rewrite `movmean_reg` (and the eight sibling adapters) to parse
   trailing strings.** The new shape:
   ```cpp
   int dim = 0;
   bool include_nan = false;     // default omit
   EndpointMode ep = EndpointMode::Shrink;
   double ep_fill = std::numeric_limits<double>::quiet_NaN();
   const Value *sample_points = nullptr;
   size_t i = 2;
   // optional positional dim
   if (i < args.size() && (args[i].isScalar() || args[i].isEmpty())
       && !args[i].isChar()) { dim = (int)args[i].toScalar(); ++i; }
   // optional positional nanflag
   if (i < args.size() && (args[i].isChar() || args[i].isString())) {
     std::string s = lower(args[i].toString());
     if (s == "omitnan") { include_nan = false; ++i; }
     else if (s == "includenan") { include_nan = true; ++i; }
     // else fall through to N-V loop (might be 'Endpoints')
   }
   // N-V loop
   while (i + 1 < args.size()) {
     std::string name = lower(args[i].toString());
     if (name == "endpoints") parse_endpoints(args[i+1], ep, ep_fill);
     else if (name == "samplepoints") sample_points = &args[i+1];
     i += 2;
   }
   ```
3. **Implement `Endpoints` modes:**
   - `discard`: output shape shrinks (1-D: length N - kb - kf,
     matrix: along reduced dim).
   - `fill` / scalar: skip the window-shrink at edges; use the
     supplied fill value (NaN if `'fill'`, the scalar otherwise).
4. **Implement `SamplePoints`:** translate the `[kb kf]` window from
   index units into sample-position units. For each output index `i`
   the window includes all `j` where
   `t(i) - kb_pos ≤ t(j) ≤ t(i) + kf_pos`.
5. **`k=0`** ⇒ throw with MATLAB-matching message ("Window length
   must be finite, positive, real scalar or 2-element vector of
   finite, nonnegative, real scalars.").
6. **Spec extension:** `movmean.json` covers only one input today.
   Add fingerprint entries for default-NaN, omitnan, includenan,
   Endpoints=discard/fill/scalar, SamplePoints, asymmetric `[kb kf]`,
   and the k=0 error path. `tol = 1e-9`.
7. **PROGRESS.md row update:** drop the "1M-pt window=5" perf note
   (or keep it but add coverage notes).

## Out of scope for this ТЗ

- Distributing the same fixes to `movsum`, `movmin`, `movmax`,
  `movprod`, `movmedian`, `movvar`, `movstd`, `movmad` — each has its
  own ТЗ (`audit/findings/stats/<name>.md`) but they all need the
  identical adapter rewrite. A factored helper
  (`parse_mov_extras(...)`) would let the eight adapters share the
  same code.
