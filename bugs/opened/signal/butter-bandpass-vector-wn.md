# signal.butter — butter(N, [W1, W2]) throws 'Cannot convert double to scalar'

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing feature)
- **Kind:** bug
- **Found:** 2026-08-30 via interactive DSP filter design session

## Symptom
Calling `butter(N, [W1, W2])` or `butter(N, [W1, W2], 'bandpass')` / `butter(N, [W1, W2], 'stop')` fails with:
`Error: Cannot convert double to scalar (in call to 'butter')`.
In MATLAB, `butter` accepts a 2-element vector `[W1, W2]` to design a `2*N`-order bandpass or bandstop filter.

## Repro
```matlab
clear;
[b, a] = butter(2, [0.2, 0.5], 'bandpass');
% numkit: Error: Cannot convert double to scalar (in call to 'butter')
% MATLAB: b = [0.097631 0 -0.195262 0 0.097631], a = [1 -0.942809 0.333333 -0.188562 0.037037]
```

## Root cause
In `src/bundle/src/register/signal/filter_design/filter_design_reg.cpp:41`:
```cpp
const int N = static_cast<int>(args[0].toScalar());
const double Wn = args[1].toScalar(); // throws if args[1] is a 2-element vector
```
The registration wrapper unconditionally expects `Wn` to be a 1x1 scalar, and `numkit::signal::butter` only implements scalar cutoff frequencies (low/high).

## Suggested fix
1. In `butter_reg`:
   - Inspect `args[1].numel()`. If `args[1].numel() == 2`, extract `W1 = args[1].elemAsDouble(0)` and `W2 = args[1].elemAsDouble(1)`.
   - Default `type = "bandpass"` if 2 elements provided and no 3rd string argument passed.
2. In `src/toolboxes/signal/src/filter_design/butter.cpp`:
   - Support bandpass/bandstop transformation (2N poles).

## References
- **Guard:** `DISABLED_ButterBandpassVectorWn`
- `src/bundle/src/register/signal/filter_design/filter_design_reg.cpp:35-60`
- `src/toolboxes/signal/src/filter_design/butter.cpp`
- `src/toolboxes/signal/tests/known_bugs_test.cpp`
