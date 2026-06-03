# signal.dct / signal.idct — Type 1/3/4 not implemented

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing option)
- **Found:** 2026-06 via signal.* DEEP-PROBE sweep

## Symptom
`dct`/`idct` accept a `'Type'` name-value (DCT variants 1–4 in MATLAB) but
only Type 2 (the default) is implemented; Types 1, 3, 4 throw.

## Repro
```matlab
dct([1 2 3 4], 4, 'Type', 1)
% numkit: Error — dct: 'Type' values other than 2 are not yet implemented
% MATLAB: 4.927993 -2.140299 0.845510 -0.647395
dct([1 2 3 4], 4, 'Type', 3)
% MATLAB: 4.388955 -3.071930 1.071930 -0.388955
dct([1 2 3 4], 4, 'Type', 4)
% MATLAB: 3.599737 -3.339911 1.771408 -1.658012
```

## Root cause
`libs/signal/src/transforms/dct.cpp:337` (dct) and `:360` (idct) throw for
any Type ≠ 2.

## Suggested fix
Implement the four orthonormal DCT formulas (and their idct inverses).
Each Type is a closed-form cosine sum with a specific orthonormal scaling
— self-contained, no external deps. Moderate (6 formulas: dct I/III/IV +
idct I/III/IV). Verify each scaling factor against MATLAB.

## References
- `libs/signal/src/transforms/dct.cpp`
- MATLAB `doc dct` (Type definitions)
