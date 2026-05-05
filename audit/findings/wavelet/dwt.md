# wavelet/dwt — ТЗ for completion

**Status:** open
**Priority:** **critical**
**Effort:** large
**Audited at commit:** 0e895fe
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/dwt/dwt.cpp:79` (`dwt`)
- Adapter: `libs/wavelet/src/dwt/dwt.cpp:202` (`dwt_reg`)
- Spec: `tools/parity/specs/dwt.json`
- What works today:
  - `[cA, cD] = dwt(x, wname)` — single-level DWT, **numkit's
    downsampling rule** (calibrated for numkit's idwt round-trip)
  - Throws if 2nd arg is not a string
- Source code comment explicitly notes: "downsampling rule …
  verified by inverse round-trip identity against post-zero idwt".
  This is calibrated for self-consistency, NOT MATLAB output match.

## MATLAB R2025b — actual behavior

Documented signatures (`help dwt`):

- `[cA, cD] = dwt(x, wname)`
- `[cA, cD] = dwt(x, LoD, HiD)` — custom analysis filters
- `[cA, cD] = dwt(___, "mode", extmode)` — boundary mode

Boundary modes: `'sym'` (default), `'symw'`, `'asym'`, `'asymw'`,
`'zpd'`, `'sp0'`, `'sp1'`, `'ppd'`, `'per'`.

Output length: `floor((N + Lf − 1) / 2)` for `'sym'`/`'zpd'`/etc.;
`floor(N / 2)` for `'per'`.

The standard reference for the MATLAB downsampling alignment is
Mallat (1989) — symmetric extension, conv with analysis filter,
keep even indices starting from `Lf − 1`.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | **`cA`/`cD` numeric values** | follow Mallat / MATLAB R2007b convention | numkit uses a custom downsampling rule that pairs only with its own idwt; values diverge from MATLAB | **critical** |
| 2 | `dwt(x, LoD, HiD)` custom filters | run with the user-supplied analysis filter pair | numkit throws "expected a string for wavelet name" | high |
| 3 | `dwt(x, wname, 'mode', extmode)` | switch boundary extension | numkit silently ignores the `'mode'` N-V — the extra args don't reach the impl | high |
| 4 | per-band sign convention | `cD` for `[1..8]` with db2 has positive endpoints `+0.6124 / -0.6124` | numkit returns `+0.6124 / -0.6124` (sign-flipped at edges? probe shows opposite) — minor sign issue | medium |

## Reference table (from probe)

Inputs: `x = [1 2 3 4 5 6 7 8]'`, wavelet `'db2'`

| Inputs | MATLAB | numkit |
|---|---|---|
| `[cA, cD] = dwt(x, 'db2')` cA | `[1.7678, 2.3108, 5.1392, 7.9676, 10.9602]` | `[1.7678, 4.7603, 7.5887, 10.4171, 10.9602]` ❌ |
| `cD` | `[-0.6124, ~0, ~0, ~0, +0.6124]` | `[+0.6124, ~0, ~0, ~0, -0.6124]` (sign flip) ❌ |
| `dwt(x, Lo_D, Hi_D)` | matches `dwt(x, 'db2')` | THROWS |
| `dwt(x, 'db2', 'mode', 'per')` | cA = `[4.7603, 3.725, 6.5534, 10.4171]` (length 4) | numkit: ignores mode, returns the basic `[1.77, 4.76, 7.59, 10.42, 10.96]` ❌ |
| `dwt(x, 'db2', 'mode', 'zpd')` | cA = `[-0.0347, 2.3108, 5.1392, 7.9676, 10.0729]` | THROWS or returns basic |

## Recommended fixes

1. **Replace the downsampling rule with MATLAB's R2007b convention.**
   Algorithm (per Mallat 1989 / MATLAB doc):
   - Extend `x` symmetrically (whole-point) by `Lf − 1` samples on
     both sides → `xext`.
   - Convolve `xext` with `Lo_D` (full mode).
   - Take samples at indices `[Lf − 1, Lf + 1, Lf + 3, …]` (i.e.
     downsample by 2 starting at `Lf − 1`) — keep `outLen` samples.
   - Same for `Hi_D` for `cD`.

   **This will break numkit's existing self-paired idwt.** Both
   `dwt` and `idwt` need to land in the same commit; the existing
   round-trip-only convention must be replaced with the MATLAB
   convention end-to-end.
2. **Accept `(x, LoD, HiD)` form:** when `args[1]` is numeric (not
   a string), treat `args[1] = LoD` and require `args[2] = HiD`.
   Skip the `wavelet_filters(wname)` lookup and use the supplied
   filter pair directly.
3. **Add `mode` N-V parsing:** rewrite `dwt_reg` to accept a
   trailing `'mode', extmode` (MATLAB also accepts `Mode=extmode` —
   only if the parser supports struct-style N-V; current parser
   does not, so accept the legacy quoted form).
4. **Implement the boundary modes:** `'sym'` (already used as the
   internal default), `'per'` (periodic, output length `floor(N/2)`),
   `'zpd'` (zero pad, output length `floor((N + Lf − 1) / 2)`),
   `'ppd'` (true periodic), and the rest (`'symw'`/`'asym'`/
   `'asymw'`/`'sp0'`/`'sp1'`).
5. **Fix the `cD` sign** so it matches MATLAB (positive at the
   leading edge for the canonical test). This is likely a single
   sign flip in the `Hi_D` filter convention or in the convolution
   direction.
6. **Spec extension:** the existing `dwt.json` covers only one
   case. Add fingerprint entries for: `[cA, cD]` numeric values
   on `[1..8] / db2 / sym` (now matching MATLAB), `db4`, `coif1`,
   custom-filt form, `mode='per'`, `mode='zpd'`. `tol = 1e-12`.

## Out of scope for this ТЗ

- The 2-D `dwt2` form — separate (`audit/findings/wavelet/dwt2.md`
  if needed).
- Multi-level `wavedec` is fixed automatically once `dwt` lands —
  see `audit/findings/wavelet/wavedec.md`.
