# linalg.qr — column-pivoting 3rd output (P) unsupported

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (missing output)
- **Kind:** missing-output
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
`toolboxes/linalg/src/decompositions.cpp` implements unpivoted Householder QR;
no column-pivoting path and the adapter emits only `outs[0..1]`.

## Suggested fix
Householder QR **with column pivoting**: at each step pick the remaining
column of largest norm, swap, accumulate the permutation. Emit `P` as a
permutation matrix (default) or a vector (`'vector'` option). NOT trivial —
it's a distinct algorithm from the unpivoted path. Validate `A*P = Q*R` and
the column order vs MATLAB.

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 25),
  `toolboxes/linalg/src/decompositions.cpp` (`qrPivotedHouseholder`, `qr_pivoted`).
- New column-pivoted Householder path: at each step pivots to the remaining
  column of largest sub-column 2-norm (recomputed exactly per step → matches
  MATLAB's xGEQP3 order in non-degenerate cases), then applies the **same**
  reflector as the unpivoted path. `qr_reg` routes `nargout >= 3` here and emits
  `P` as a permutation matrix (default), a vector with `'vector'`, or a vector
  when `econ`/`0` is also given (MATLAB convention).
- Verified vs MATLAB R2025b: 3×2 repro → `P=[0 1;1 0]`, `p=[2 1]`,
  `R(1,1)=-7.483315`, `A*P=Q*R`; 3×3 → pivot order `[3 1 2]`, `|diag(R)|`
  rank-revealing decreasing; `qr(A,0)` 3-output → `P` is a vector, `Q` economy.
- **Sign note:** `Q`/`R` signs match MATLAB for **tall** matrices (m>n), but the
  *trailing 1×1* diagonal of a **square** matrix differs — a *pre-existing*
  numkit convention (numkit always applies the reflector; LAPACK skips the
  identity reflector when the sub-diagonal is already zero). This affects the
  unpivoted path too and is independent of pivoting. The QR is valid either way
  (`A*P=Q*R` holds); validation uses `abs(diag(R))` + reconstruction, as the
  existing qr parity spec already does.
- Live guard: `toolboxes/linalg/tests/qr_pivoting_test.cpp` (6 TEST_F) + flipped
  `LinalgKnownBug.QrColumnPivoting` live. Parity:
  `tools/parity/specs/qr.json` extended (correctness=OK). Smoke:
  `toolboxes/linalg/tests/smoke/qr_pivoting_smoke.m`.

## References
- `toolboxes/linalg/src/decompositions.cpp` (qr, qr_pivoted, qrPivotedHouseholder)
- MATLAB `doc qr`
