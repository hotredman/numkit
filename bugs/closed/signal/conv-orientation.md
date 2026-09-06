# signal.conv — output orientation ignores the FIRST input (columns collapsed to rows)

- **Status:** ✅ FIXED (54a1f3f07, 2026-09-06)
- **Kind:** bug
- **Severity:** P1 wrong shape (silently mis-shaped result feeding downstream code)
- **Found:** 2026-09-06 via the springer-math compare group — pr2_4's `y` (book convint over `conv(x, h)` with column h) came out 1×801 vs MATLAB 801×1

## Symptom

`conv` always returned a ROW vector. MATLAB's result takes the FIRST
input's orientation.

## Repro

```matlab
clear;
y = conv([1; 2; 3], [1 2]);
% numkit (pre-fix): 1x4 row
% MATLAB R2025b:    4x1 column
% rule: LONGER vector wins, ties -> column (conv(row3,col2) -> 1x4 row)
```

## Root cause

Both output paths (real and complex) in
`src/toolboxes/signal/src/convolution/convolution.cpp` built the result
with a hardcoded `Value::matrix(1, outLen, …)`.

## Fix

Orientation-aware allocation per the probed MATLAB rule: the result
follows the LONGER vector; equal lengths (and column-vs-scalar) give a
COLUMN. Live guard: `ConvolutionTest.OutputOrientationFollowsLongerInput`
(seven orientation combinations incl. the tie + scalar + complex path).

Downstream effect: impulse(b,a,t) returning a proper column (this same
portion) made textbook `conv(x, h)` calls orientation-correct end-to-end
(pr2_4 now matches MATLAB's workspace).
