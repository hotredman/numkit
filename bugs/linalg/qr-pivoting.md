# linalg.qr — column-pivoting 3rd output (P) unsupported

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing output)
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`[Q,R,P] = qr(A)` (column-pivoting QR) throws "Too many output arguments".
numkit's `qr` returns only `[Q,R]` (plain Householder, no pivoting).

## Repro
```matlab
[Q,R,P] = qr([1 2; 3 4; 5 6]);
% numkit: Error — Too many output arguments
% MATLAB: P = [0 1; 1 0]  (A*P = Q*R; columns ordered by decreasing norm)
%         R(1,1) = -7.483315
[Q,R,p] = qr([1 2; 3 4; 5 6], 'vector');   % p = [2 1]  (permutation vector)
```

## Root cause
`libs/linalg/src/decompositions.cpp` implements unpivoted Householder QR;
no column-pivoting path and the adapter emits only `outs[0..1]`.

## Suggested fix
Householder QR **with column pivoting**: at each step pick the remaining
column of largest norm, swap, accumulate the permutation. Emit `P` as a
permutation matrix (default) or a vector (`'vector'` option). NOT trivial —
it's a distinct algorithm from the unpivoted path. Validate `A*P = Q*R` and
the column order vs MATLAB.

## References
- `libs/linalg/src/decompositions.cpp` (qr)
- MATLAB `doc qr`
