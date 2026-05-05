# signal/hilbert — ТЗ for completion

**Status:** open
**Priority:** **critical**
**Effort:** small
**Audited at commit:** 9bce106
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/transforms/...` (`hilbert`)
- Spec: `tools/parity/specs/hilbert.json`
- Returns analytic signal `H = X + i·H{X}`. Real part matches
  MATLAB exactly. **Imaginary part is sign-flipped.**

## MATLAB R2025b — actual behavior

- `H = hilbert(X)` — analytic signal (real(H) = X, imag(H) = Hilbert
  transform of X)
- `H = hilbert(X, n)` — pad/truncate to n

The standard Hilbert transform has a specific sign convention:
positive frequencies multiplied by `+i`, negative by `-i`. MATLAB
follows this convention; the imag part of `hilbert([1:8])` is
`[3.83 -1 -1 -1.83 -1.83 -1 -1 3.83]`.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | sign of imaginary part | `+H{X}` | `-H{X}` (sign-flipped) | **critical — analytic signal phase reversed; downstream demodulation, instantaneous frequency, envelope all see the wrong sign** |
| 2 | length override `n` | pad/truncate | likely not supported | medium |
| 3 | matrix input | column-wise transform | needs probe | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `real(hilbert([1:8]'))` | `[1 2 3 4 5 6 7 8]` | identical ✅ |
| `imag(hilbert([1:8]'))` | `[3.828 -1 -1 -1.828 -1.828 -1 -1 3.828]` | `[-3.828 +1 +1 +1.828 +1.828 +1 +1 -3.828]` ❌ (all signs flipped) |

## Recommended fixes

1. **Flip the sign convention.** Find the FFT-based implementation:
   the standard algorithm doubles the positive-frequency bins
   (indices `1..N/2-1`) and zeros the negative-frequency bins, then
   inverse-FFTs. If numkit zeros the **positive** bins instead, the
   sign comes out wrong. Single-line fix: swap which half gets
   doubled.
2. **Spec extension** — add fingerprint covering imag values
   (currently only real is probed). `tol = 1e-12`.
3. **Length override `n`** — accept 2nd arg.

## Out of scope for this ТЗ

- The downstream `envelope` / `instfreq` cascade — those will
  inherit a sign flip until `hilbert` is fixed. After the fix,
  re-probe the dependents.
