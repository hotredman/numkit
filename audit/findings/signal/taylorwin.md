# signal/taylorwin — ТЗ for completion

**Status:** open
**Priority:** **critical**
**Effort:** medium
**Audited at commit:** 69ef496
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/windows/windows.cpp` (`taylorwin`)
- Adapter: `libs/signal/src/windows/windows.cpp:599` (`taylorwin_reg`)
- Spec: `tools/parity/specs/taylorwin.json` (PROGRESS:
  `correctness=MISMATCH`)
- What works today:
  - `w = taylorwin(N[, nbar=4[, sll=-30]])` accepted by adapter
  - Output is **inverted**: peak at edges, dip at centre

## MATLAB R2025b — actual behavior

Documented signatures (`help taylorwin`):

- `w = taylorwin(L)` — defaults: `nbar=4`, `sll=-30`
- `w = taylorwin(L, nbar)`
- `w = taylorwin(L, nbar, sll)` — sll = sidelobe level in dB
  (negative)

Taylor window: tapered window with a cosine-modulated correction;
peak at the centre, zero-ish at the edges. Used in radar pulse-
compression for low-sidelobe Taylor weighting.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | shape | tapered, peak at centre | **inverted** — peak at edges | **critical** |
| 2 | normalisation | max-value `1.52` for `taylorwin(8, 4, -30)` | numkit's max-value is `1.0` (incorrect normalisation) | high |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `taylorwin(8)` (= taylorwin(8,4,-30)) | `[0.435 0.802 1.242 1.520 1.520 1.242 0.802 0.435]` | `[1.000 0.765 0.484 0.307 0.307 0.484 0.765 1.000]` (inverted!) ❌ |
| `taylorwin(8, 4, -40)` | `[0.279 0.730 1.298 1.692 1.692 1.298 0.730 0.279]` | `[1.000 0.738 0.408 0.179 0.179 0.408 0.738 1.000]` ❌ |
| `taylorwin(8, 6, -30)` | `[0.449 0.804 1.236 1.511 1.511 1.236 0.804 0.449]` | `[1.000 0.771 0.493 0.315 0.315 0.493 0.771 1.000]` ❌ |

## Recommended fixes

1. **Reimplement taylorwin per MATLAB algorithm.** Standard formula:
   - Compute `R = 10^(-sll/20)`, `A = (1/π)·acosh(R)`, `σ² = nbar²
     / (A² + (nbar - 0.5)²)`.
   - For `m = 1..nbar-1`, compute `Fm = ((-1)^(m+1)·prod_{n=1..nbar-1}
     (1 − m²/(σ² · (A² + (n-0.5)²)))) / (2 · prod_{n=1, n≠m..nbar-1}
     (1 − m²/n²))`.
   - For `i = 1..N`, compute
     `w(i) = 1 + 2·Σ_{m=1..nbar-1} Fm · cos(2π·m·(i - (N+1)/2)/N)`.
2. The numkit current output looks like `1 - cosTaper(...)` instead
   of `1 + cosTaper(...)` — most likely a sign bug in the cosine
   sum, OR an off-by-one in `(i - (N+1)/2)/N` index.
3. **Normalisation:** MATLAB does NOT normalise to peak=1; the peak
   value depends on (nbar, sll). Don't add a divide-by-max step.
4. **Spec extension:** regenerate `taylorwin.json` after the fix.
   Fingerprint over (N=8/16/64) × (nbar=4/6/8) × (sll=-30/-40/-60).
   `tol = 1e-9`.

## Out of scope for this ТЗ

- The single-precision `typeName` form — secondary.
