# runtime.hilbert — hilbert() fails "Not a complex array" for non-power-of-2 lengths

- **Status:** 🟡 OPEN
- **Severity:** P3 (needs pow2 padding workaround in user code; MATLAB
  accepts any length)
- **Kind:** bug
- **Found:** 2026-09-13, Play gallery section D (AM envelope demo).

## Repro

```matlab
clear;
z = hilbert([1 2 3 4]);        % OK (pow2)
a = 1:11;                      % 11 = not pow2
z2 = hilbert(a);               % numkit: "Not a complex array"
% MATLAB R2025b: numel(z2) == 11
```

## Notes

FFT-based implementation rejects non-pow2 lengths instead of falling
back to the general-N FFT path.

## References

- **Guard:** `DISABLED_HilbertNonPow2Length` in
  `src/bundle/tests/known_bugs_test.cpp`.
