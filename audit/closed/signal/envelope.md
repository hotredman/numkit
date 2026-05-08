# signal/envelope — ТЗ for completion

**Status:** closed
**Priority:** **critical** (PROGRESS already MISMATCH)
**Effort:** medium
**Audited at commit:** 9bce106
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/transforms/...` (`envelope`)
- Spec: `tools/parity/specs/envelope.json` (PROGRESS:
  `correctness=MISMATCH`)
- What works today: returns 1 output (upper envelope only)

## MATLAB R2025b — actual behavior

- `[yupper, ylower] = envelope(x)` — analytic-signal envelope
- `envelope(x, fl)` — windowed Hilbert with filter length
- `envelope(x, np, 'analytic')` / `'rms'` / `'peak'` — multiple
  modes

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | 2nd output `ylower` | populated | numkit returns empty for 2nd out (probe: `lo = empty`) | **high** |
| 2 | numeric values | analytic envelope | diverge (PROGRESS MISMATCH); root cause likely the upstream `hilbert` sign flip + different filter-length default | high |
| 3 | `envelope(x, fl)` filter-length form | windowed | not supported | medium |
| 4 | `'rms'` / `'peak'` modes | non-analytic envelopes | not supported | medium |

## Reference table (from probe)

Inputs: `sig = sin(2π·0.1·(0:31)) · exp(-0.05·(0:31))`

| Inputs | MATLAB | numkit |
|---|---|---|
| `[up, lo] = envelope(sig)` up head | `[0.479 0.826 0.865 0.899 0.930 0.882]` | `[0.437 0.812 0.865 0.895 0.910 0.840]` ❌ |
| `lo` head | `[-0.397 -0.744 -0.783 -0.817 -0.848 -0.800]` | empty (only 1 out) |

## Recommended fixes

1. **Populate 2nd output `ylower`:** `ylower = -yupper` for the
   analytic-signal path is the symmetric pair (both wrap a real
   symmetric envelope). If numkit's adapter only writes `outs[0]`,
   add `if (nargout > 1) outs[1] = ...`.
2. **Fix the cascade from `hilbert`:** once
   `audit/findings/signal/hilbert.md` lands, re-probe envelope
   numerics — the sign flip in hilbert directly corrupts the
   analytic-signal magnitude calculation.
3. **Implement filter-length form `envelope(x, fl)`:** windowed
   Hilbert with FIR filter design.
4. **Implement `'rms'` and `'peak'` modes.**
5. **Spec extension:** PROGRESS notes MISMATCH; regenerate after
   the cascade fix. `tol = 1e-9`.

## Out of scope for this ТЗ

- N-D envelope.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-08
- Notes: **FULL MATLAB R2025b parity** for all four documented
  envelope signatures. All 24 fingerprints in
  `tools/parity/specs/envelope.json` match MATLAB bit-identical
  (`tol = 1e-9`).

  **All four modes implemented** (mirror `envelope.m` exactly):
  - **default** `[yu, yl] = envelope(x)` — FFT-based
    `xampl = |hilbert(x - mean(x))|`, then
    `yu = mean + xampl`, `yl = mean - xampl`. Re-uses the
    audit-fixed `hilbert(x)` (positive sign convention).
  - **`'analytic'`** `envelope(x, n, 'analytic')` (and the
    short-form `envelope(x, n)` which routes here) — n-tap
    Kaiser(beta=8)-tapered ideal Hilbert FIR with
    `t = ((1-n)/2 : (n-1)/2) / 2`,
    `firFilter = sinc(t).*exp(i·π·t).*kaiser(n,8)`,
    normalised so `sum(real(firFilter)) = 1`, applied via
    'same' complex convolution. DC removal applied.
  - **`'rms'`** `envelope(x, n, 'rms')` — sliding centered-window
    RMS on the mean-centered signal:
    `xampl = sqrt(movmean((x-mean).^2, n))`,
    `yu = mean + xampl`, `yl = mean - xampl`.
  - **`'peak'`** `envelope(x, n, 'peak')` — operates on the raw
    signal (no DC removal). Greedy `findpeaks` with
    `MinPeakDistance = n` for both maxima (upper) and minima
    (lower), then spline interpolation through the picked knots.
    For the **3-knot** edge case the implementation fits an
    exact parabola via Lagrange-style determinants — this matches
    MATLAB's `spline` behavior for n=3 (parabola, not natural BC)
    and was the last numerical gap to close.

  **Conv 'same' centering fix**: MATLAB's `conv2(x, h, 'same')`
  uses `pad = floor(n/2)` for the leading offset, not
  `(n-1)/2` — confirmed by bit-comparing analytic n=8 against
  MATLAB.

  **4 artefacts shipped:**
  - impl: `libs/signal/src/transforms/hilbert.cpp`
    (`envelope_full`, `ampl_fft_hilbert`, `ampl_fir`, `ampl_rms`,
    `env_peak`, `findpeaks_min_distance`)
  - parity spec: `tools/parity/specs/envelope.json` — 24
    fingerprints across all 4 modes, `tol = 1e-9`,
    `correctness=OK`
  - gtest: `libs/signal/tests/envelope_test.cpp` — 14 tests, all
    bit-identical assertions (one per mode + structural
    invariants like "default upper+lower = 2·mean", "peak mode
    preserves DC offset", "two-arg form aliases to analytic")
  - smoke: extends existing `libs/signal/tests/smoke/hilbert_smoke.m`

  Octave doesn't ship `envelope` (parity reports
  `octave correctness=N/A`), so MATLAB R2025b is the sole
  reference engine for this function.

