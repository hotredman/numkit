# runtime.exist — `exist('butter')` / `exist('hamming')` return 0 for registered toolbox functions (top level, no filter)

- **Status:** 🟡 OPEN
- **Severity:** P3 (probes and feature-detection code like
  `if exist('chebwin')` mis-report toolbox functions as absent while
  direct calls work)
- **Kind:** bug
- **Found:** 2026-09-12, Play portion D — checking the beamforming
  gallery script's window functions before first run.

## Symptom

`exist('butter')`, `exist('hamming')`, `exist('fft')` → **0**, while
`butter(...)`/`hamming(8)`/`fft(x)` execute fine in the same session.
Core builtins report correctly (`exist('sin')` → 5).

## Repro (self-contained)

```matlab
clear;
disp(exist('sin'));      % numkit: 5  ✓ (builtin)
disp(exist('butter'));   % numkit: 0  ✗ MATLAB R2025b: 2 (file-on-path)
disp(exist('hamming'));  % numkit: 0  ✗ MATLAB: 2
w = hamming(8); disp(numel(w));   % numkit: 8 — the call WORKS
```

## Root-cause hypothesis

Same opcode/external split as the exist-file bug fixed in portion 33
(bugs/closed/runtime/exist-file-function-scope.md): the no-filter
probe path checks core builtins but not the registered-toolbox
external surface; the toolbox functions register under their namespace
+ bare names, and exist's fallback does not consult the bare-name
index.

## References

- **Guard:** `DISABLED_ExistToolboxFunctionNoFilter` in
  `src/bundle/tests/known_bugs_test.cpp`.
