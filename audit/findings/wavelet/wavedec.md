# wavelet/wavedec — ТЗ for completion

**Status:** open
**Priority:** **critical**
**Effort:** medium (depends on `dwt` being fixed first)
**Audited at commit:** 0e895fe
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/dwt/multilevel.cpp:42` (`wavedec`)
- Adapter: `libs/wavelet/src/dwt/multilevel.cpp:218` (`wavedec_reg`)
- Spec: `tools/parity/specs/wavedec.json`
- What works today:
  - `[c, l] = wavedec(x, n, wname)` — multi-level decomposition
  - Composes `dwt` n times — inherits the `dwt` value-mismatch
    against MATLAB

## MATLAB R2025b — actual behavior

Documented signatures (`help wavedec`):

- `[c, l] = wavedec(x, n, wname)`
- `[c, l] = wavedec(x, n, LoD, HiD)` — custom analysis filters
- `[c, l] = wavedec(___, Mode=extmode)` — boundary mode

`c` is the concatenated coefficient vector (approx-coarsest first,
then details from coarsest to finest); `l` is the bookkeeping
length vector `[Lc_n, Ld_n, Ld_{n-1}, ..., Ld_1, N]`.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | numeric `c` values | follow MATLAB dwt convention | inherits numkit's dwt mismatch ⇒ all coefficient values diverge | **critical** (cascade from dwt) |
| 2 | `wavedec(x, n, LoD, HiD)` custom filters | runs with supplied filter pair | not supported | high |
| 3 | `wavedec(___, Mode=extmode)` (or `'mode', extmode`) | switches boundary | silently ignored | high |

## Reference table (from probe)

Inputs: `x = [1..16]'`, n=3, wavelet='db2'

| Inputs | MATLAB-derived | numkit |
|---|---|---|
| `l` bookkeeping | `[4 4 6 9 16]` | `[4 4 6 9 16]` ✅ — lengths match |
| `c` first 6 (under same input) | (probed on randn input — different baseline) | `[8.0933 32.79 44.53 42.45 5.30 -0.33]` |

Length structure matches MATLAB exactly. Numerics diverge because
each level's `dwt` call returns numkit-pair coefficients rather
than MATLAB coefficients.

## Recommended fixes

1. **Land after `dwt` is fixed** — `wavedec` is a thin
   `repeat(dwt, n)` loop; once the underlying `dwt` returns
   MATLAB-compatible coefficients, this function inherits parity
   automatically.
2. **Accept `(x, n, LoD, HiD)`** by detecting numeric `args[2]` and
   requiring numeric `args[3] = HiD`. Skip `wavelet_filters` lookup.
3. **Add `mode` N-V parsing** to forward to `dwt`.
4. **Spec extension:** new fingerprint covering both `c` and `l`,
   under the new MATLAB-matching convention. `tol = 1e-12`.

## Out of scope for this ТЗ

- Multi-level `wavedec2` (2-D) — not in this batch.
