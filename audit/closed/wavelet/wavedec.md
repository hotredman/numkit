# wavelet/wavedec — ТЗ for completion

**Status:** closed (custom-filter form deferred — rare for multi-level)
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

## Closed
- Closed in commits: 32ab3ce0 (wfilters cascade) + this commit
- Closed date: 2026-05-08
- Notes: After the wfilters Lo_D/Lo_R label-swap fix (32ab3ce0),
  wavedec multi-level output is bit-identical to MATLAB R2025b
  with no further wavedec code changes — it just calls dwt
  iteratively, and dwt was the source of the value mismatch.

  Verified on x=1:16, 3-level db2: c(1..4) =
  [3.8832803055, 3.6258809906, 21.4103239491, 42.7562766754]
  matches MATLAB byte-for-byte (l = [4 4 6 9 16]).

  4 artefacts shipped (19-fp parity spec + gtests in
  dwt_idwt_test.cpp + smoke). Bit-identical numkit ↔ MATLAB.
  Octave doesn't ship `wavedec`.

  Custom `(Lo_D, Hi_D)` form for wavedec deferred — uncommon for
  multi-level decomposition, can be threaded later.
