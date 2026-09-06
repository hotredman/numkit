# signal.freqs — auto-grid resample reads one-past-the-end (Debug assert; benign-but-UB in Release)

- **Status:** ✅ FIXED (54a1f3f07, 2026-09-06)
- **Kind:** bug
- **Severity:** P3 latent UB (wrong-value risk nil — t == 0 — but a real out-of-bounds read)
- **Found:** 2026-09-06 while re-running the filter-design suite in a Debug-config gtest build (MSVC `_ITERATOR_DEBUG_LEVEL=2` assert: "vector subscript out of range")

## Symptom

Every `freqs(b, a)` two-arg (auto-grid) call aborted the Debug build with
the MSVC iterator assert. Release builds ran fine — which is why the
fidelity-pass suite never saw it.

## Repro

```matlab
clear;
[~, w] = freqs([1], [1 sqrt(2) 1]);
% numkit (Debug build, pre-fix): process aborts, "vector subscript out of range"
% numkit (Release, pre-fix): correct w (the read is multiplied by t == 0)
% MATLAB R2025b: w = 0.1 .. 10 (200 points)
```

## Root cause

`freqsAutoGridVec` (analog_filters_reg.cpp) resamples the merged grid to
exactly 200 points; for the LAST sample it sets the 1-based index to
`f.size()` on purpose (so the result is exactly the final knot) but then
still evaluates `lw[j + 1]` with `j + 1 == lw.size()` — a one-past-the-end
read. Its value is multiplied by `t == 0`, so Release produced correct
results by luck.

## Fix

Clamp the upper knot index to the last element (`std::min(j + 1,
lw.size() - 1)`); at the clamped sample `t == 0`, so the value is
bit-identical. Guard: the existing `FilterDesignTest.FreqsTwoArgAutoW`
now runs green in Debug builds (it crashed before the fix — that IS the
reproduction).

## References

- `src/bundle/src/register/signal/filter_design/analog_filters_reg.cpp`
  (freqsAutoGridVec resample loop)
