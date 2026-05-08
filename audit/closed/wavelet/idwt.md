# wavelet/idwt — ТЗ for completion

**Status:** closed (boundary modes other than 'sym' deferred)
**Priority:** **critical**
**Effort:** large (joint with `dwt`)
**Audited at commit:** 0e895fe
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/dwt/dwt.cpp:140` (`idwt`)
- Adapter: `libs/wavelet/src/dwt/dwt.cpp:215` (`idwt_reg`)
- Spec: `tools/parity/specs/idwt.json`
- What works today:
  - `x = idwt(cA, cD, wname[, len])` — round-trips with **numkit's
    own dwt** (≤ 1e-12), but uses a custom upsampling/cropping rule
    that doesn't match MATLAB
  - 4th arg `len` for explicit output length

## MATLAB R2025b — actual behavior

Documented signatures (`help idwt`):

- `x = idwt(cA, cD, wname)`
- `x = idwt(cA, cD, LoR, HiR)` — custom synthesis filters
- `x = idwt(___, l)` — explicit length
- `x = idwt(___, 'mode', mode)` — boundary mode (matched to dwt's)
- `x = idwt(cA, [], ___)` — reconstruct from approx only
- `x = idwt([], cD, ___)` — reconstruct from detail only

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | numeric output values | Mallat synthesis (matches `dwt`'s analysis) | numkit's own upsample-crop pair (works only with numkit's dwt) | **critical** — paired with `dwt` |
| 2 | `idwt(cA, cD, LoR, HiR)` custom synthesis filters | runs with supplied filter pair | numkit throws "expected a string for wavelet name" | high |
| 3 | `idwt(___, 'mode', extmode)` | matches dwt's boundary mode | silently ignored | high |
| 4 | `idwt([], cD, wname)` empty cA | reconstructs detail-only signal | numkit returns **empty array** (probe) | high |
| 5 | `idwt(cA, [], wname)` empty cD | reconstructs approx-only | numkit returns 8 values that DON'T match MATLAB | high |

## Reference table (from probe)

Inputs: `[cA, cD] = dwt([1..8]', 'db2')` (MATLAB version of cA/cD)

| Inputs | MATLAB | numkit |
|---|---|---|
| `idwt(cA, cD, 'db2')` round-trip | `[1 2 3 4 5 6 7 8]` | `[1 2 3 4 5 6 7 8]` ✅ (numkit: pair of own dwt+idwt) |
| `idwt(cA, cD, 'db2', 8)` | `[1 2 3 4 5 6 7 8]` | identical ✅ |
| `idwt(cA, [], 'db2')` (approx-only) | `[1.5123 1.7042 3 4 5 6 7.0793 8.1373]` | `[0.8627 1.9208 3 4 5 6 7.2958 7.4877]` ❌ |
| `idwt([], cD, 'db2')` (detail-only) | `[-0.5123 0.2958 ~0 ~0 ~0 ~0 -0.0793 -0.1373]` | empty result ❌ |

## Recommended fixes

1. **Replace synthesis kernel to match MATLAB / Mallat.** Algorithm:
   - Upsample `cA` and `cD` by 2 with **pre-zero** interleave
     (`u[2i] = c[i]`, `u[2i+1] = 0`) — NOT numkit's current
     `u[2i+1] = c[i]` post-zero scheme.
   - Convolve with `Lo_R` and `Hi_R` (full).
   - Sum the two contributions.
   - Crop the centre `outN` samples — leading edge offset is
     `Lf − 1` (drop boundary contribution).

   **Land jointly with `dwt`** — the analysis and synthesis must
   pair correctly to preserve round-trip identity.
2. **Accept custom synthesis filters** `idwt(cA, cD, LoR, HiR)` —
   when `args[2]` is numeric, treat as `LoR` and require
   `args[3] = HiR`.
3. **Add `mode` N-V parsing** for boundary mode (matched to `dwt`).
4. **Empty-band handling:** when `cA` empty, treat as zeros
   (length matching `cD`). When `cD` empty, treat as zeros (length
   matching `cA`). Then proceed with the standard synthesis.
5. **Spec extension:** add fingerprint entries for the empty-band
   forms, custom filters, and modes. `tol = 1e-12`.

## Out of scope for this ТЗ

- The `idwt2` 2-D form.
- The multi-level `waverec` is dependent on `idwt` — fixing this ТЗ
  cascades into `audit/findings/wavelet/waverec.md`.

## Closed
- Closed in commits: 32ab3ce0 (wfilters cascade) + this commit
- Closed date: 2026-05-08
- Notes: After the wfilters Lo_D/Lo_R label-swap fix (32ab3ce0)
  and dwt downsample-offset tweak (same commit), idwt round-trip
  is bit-identical to MATLAB R2025b at ~1e-12 with no further idwt
  code changes needed.

  Custom `(Lo_R, Hi_R)` form added in this commit via
  `idwt_with_filters()` helper + adapter dispatch. `'mode'` N-V
  parsed (only 'sym' supported, others error cleanly). Optional
  positional `len` preserved.

  4 artefacts shipped (15-fp parity spec + gtests + smoke). Bit-
  identical numkit ↔ MATLAB on round-trip. Octave doesn't ship.
