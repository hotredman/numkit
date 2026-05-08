# signal/chebwin — ТЗ for completion

**Status:** open
**Priority:** **critical**
**Effort:** medium
**Audited at commit:** 69ef496
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/windows/windows.cpp` (`chebwin` ≈ near
  line 416 area)
- Adapter: `libs/signal/src/windows/windows.cpp:573` (`chebwin_reg`)
- Spec: `tools/parity/specs/chebwin.json` (PROGRESS:
  `correctness=MISMATCH`)
- What works today:
  - `w = chebwin(N[, R])` accepted by adapter, default `R=100`
  - Output is **broken**: returns all-ones for even N; for odd N
    returns wrong-shape window

## MATLAB R2025b — actual behavior

Documented signatures (`help chebwin`):

- `w = chebwin(L)` — sidelobe attenuation `R = 100` dB by default
- `w = chebwin(L, r)` — `r` is sidelobe attenuation in dB
- `w = chebwin(___, typeName)` — `'double'`/`'single'`

Dolph-Chebyshev window: `W(k) = T_{N-1}(β · cos(πk/N)) / T_{N-1}(β)`
where `T_n` is the Chebyshev polynomial of the first kind and
`β = cosh((1/(N-1)) · acosh(10^(R/20)))`. Computed via inverse FFT
of the magnitude spectrum.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `chebwin(8)` (even N, default R=100) | `[0.036 0.225 0.624 1 1 0.624 0.225 0.036]` | `[1 1 1 1 1 1 1 1]` (degenerate all-ones!) | **critical** |
| 2 | `chebwin(8, 60)` (even N, lower R) | `[0.068 0.303 0.687 1 1 0.687 0.303 0.068]` | all-ones | **critical** |
| 3 | `chebwin(7, 100)` (odd N) | `[0.057 0.317 0.760 1 0.760 0.317 0.057]` | `[0.311 0.200 0.622 1 0.622 0.200 0.311]` (wrong shape) | **critical** |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `chebwin(8, 100)` | `[0.0364 0.2254 0.6242 1 1 0.6242 0.2254 0.0364]` | `[1 1 1 1 1 1 1 1]` ❌ |
| `chebwin(8, 60)` | `[0.0685 0.3032 0.6868 1 1 0.6868 0.3032 0.0685]` | `[1 1 1 1 1 1 1 1]` ❌ |
| `chebwin(8, 30)` | `[0.262 0.519 0.812 1 1 0.812 0.519 0.262]` | (likely all-ones) ❌ |
| `chebwin(7, 100)` | `[0.0565 0.3166 0.7601 1 0.7601 0.3166 0.0565]` | `[0.311 0.200 0.622 1 0.622 0.200 0.311]` ❌ |

## Recommended fixes

1. **Replace the chebwin implementation.** The standard MATLAB-
   compatible algorithm:
   - Compute `β = cosh((1/(N-1)) · acosh(10^(R/20)))`.
   - Build the magnitude spectrum at `M = ceil(N/2)+1` (or `N`)
     points: `W_k = T_{N-1}(β · cos(πk/N))` for `k = 0..N-1`,
     evaluated using the recurrence
     `T_n(x) = cosh(n·acosh(x))` if `|x|≥1`,
     `T_n(x) = cos(n·acos(x))` if `|x|≤1`.
   - Inverse FFT to get the time-domain window.
   - Normalise so the max equals 1 (centre point).
   - For even N, the spectrum has different symmetry handling than
     odd N — this is where most chebwin bugs live; the all-ones
     result on even N suggests a divide-by-zero or normalisation
     collapse.
2. **Spec extension:** PROGRESS notes `correctness=MISMATCH`.
   Regenerate `chebwin.json` after the algorithm fix with
   fingerprint covering N ∈ {7, 8, 16, 64} × R ∈ {30, 60, 100, 120}.
   `tol = 1e-9`.
3. **Add `typeName` arg parsing** — same shape as the simple
   windows ТЗ (see `audit/findings/signal/hann.md`).

## Out of scope for this ТЗ

- The 'symmetric'/'periodic' sflag — chebwin is symmetric by
  definition; no sflag.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: **CRITICAL bug fix.** Closes gaps #1, #2, #3.

  **Root cause:** previous FFT-based implementation had a half-bin
  symmetry bug. For even N (anti-symmetric Dolph-Chebyshev
  spectrum), the (-1)^k weighting + zero-padded FFT collapsed to a
  degenerate all-ones output. For odd N the IFFT centering was
  off-by-one, producing a wrongly-shifted shape.

  **Fix:** rewrote to use a direct cosine-IDFT formula:

      w(n) = (1/N) · [W(0) + 2 · Σ_{k=1}^{K} W(k) · cos(2π·k·(n-N₀)/N)]

  with K = floor((N-1)/2), N₀ = (N-1)/2, and W(k) = T_M(β·cos(πk/N))
  using the branch-wise Chebyshev evaluation (cos for |x|≤1,
  cosh+sign for |x|>1). For even N the k=N/2 term has T_M(0) = 0
  (M is odd) so it drops naturally. The (n-N₀) offset centers the
  window at the midpoint without needing a separate circular shift
  for even vs odd N.

  Direct O(N²) — windows are small (N ≤ ~few thousand), so this is
  fine; ditched the FFT path entirely.

  Spec extended from 1 to 12 fingerprints (N ∈ {1, 7, 8, 16, 64} ×
  R ∈ {30, 60, 100, 120}). Parity OK numkit ↔ MATLAB ↔ Octave at
  tol=1e-9. Existing 3 chebwin gtests (Symmetric / PeakIsOne) still
  pass; added 4 new (even-N reference, odd-N reference, lower-R,
  single-point). 34 windows-suite tests all pass — no regression.

  `typeName` arg parsing (recommendation #3) deferred — same
  cross-cutting refactor as gausswin's `'single'` typeName follow-
  up.
