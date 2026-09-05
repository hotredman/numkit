# linalg.eig — AHP real-world code: eigenvector selection diverges on some matrices (w1 = e1 basis, wrong CR); confirmed column-order divergence

- **Status:** ✅ FIXED (2026-09-05)
- **Severity:** P2 (silent wrong results in real AHP analyses)
- **Kind:** bug
- **Found:** 2026-08-31 via fieldtest mat-comparison (ahp_usage.m,
  report `fieldtest/reports/20260831-151200.json` — first `workspace-mismatch`)

## Symptom (recorded from the R4 workspace diff + stdout)

An AHP (analytic hierarchy process) helper ran three eig-based blocks;
numkit vs MATLAB R2025b:

- block 1 (8×8): `cr1` 0.0250929 vs 0.0251, `w1` matched — OK;
- **block 2 (8×8): numkit `cr1 = 0.101317`, `w1 = [1 0 0 0 0 0 0 0]`**
  (a canonical basis vector — not an AHP weight vector) vs MATLAB
  `cr1 = 0`, `w1 = [2.1875 −0.1875 …]`;
- **block 3 (3×3 consistent): numkit `cr1 = 0`, `w1 = [1.25 1.25 −1.5]`**
  vs MATLAB `cr1 = 1.2810e-15`, `w1 = [0.3846 0.3846 0.2308]` (the
  Perron weights).

Workspace diff also flagged `eigenvalue`, `x`, `y` as class real
(numkit) vs complex (MATLAB) — MATLAB's `eig` returns a complex-typed
decomposition here (tiny/zero imaginary parts), numkit a real one.

## Confirmed so far (self-contained)

1. **Column order differs across engines** (both legal — MATLAB
   guarantees no order):

```matlab
clear;
A = [1 1 5/3; 1 1 5/3; 3/5 3/5 1];      % consistent AHP matrix
[V, D] = eig(A);
[mm, idx] = max(diag(D));
% numkit: mm=3.000000000000000 idx=3
% MATLAB: mm=3.000000000000001 idx=1
% w1 = V(:,idx)/sum(V(:,idx)) matches (Perron weights) in BOTH
```

2. Selection by `[~, idx] = max(diag(D))` works on consistent matrices
   (guarded green: `EigAHPConsistentPerronSelection`).

## Unknown (needs the original 8×8 construction)

The `w1 = e1` result (block 2) indicates a matrix family where numkit's
decomposition itself is wrong or V/D are inconsistent (basis-vector
"eigenvector" with a nonzero CR). The legacy corpus copy of the script is
deleted (corpus switched to the catalog); AHP code is common in the
catalog's Chinese math-modeling books — the next batch re-encounters it
and the distillation completes from that script.

## Suggested fix

First reproduce: build 8×8 reciprocal pairwise matrices and diff `[V,D]`
element-wise vs MATLAB (workspace .mat compare — same method that found
this). Check whether V(:,i) fails `A*V == V*D` for the returned D, and
whether the complex-typed-vs-real D storage affects `max(diag(D))`
selection semantics (MATLAB `max` on complex compares ABS values).

## References

- **Guard:** deferred — the DISABLED_ guard needs the diverging 8×8
  construction (awaits corpus re-encounter). Interim green pin:
  `EigAHPConsistentPerronSelection` in
  `src/toolboxes/linalg/tests/eig_test.cpp` (the selection idiom real
  code relies on).
- Evidence: `fieldtest/reports/20260831-151200.json` (mat_diff lines +
  both engines' stdout).


## Resolution (2026-09-05) — the proactive AHP hunt found a 3x3, and the root was a one-line bulge-chase bug

Generated 129 reciprocal pairwise matrices (Saaty-scale + consistent
rank-1 + noise, n=3..10), diffed [V,D]/selection-invariant w/lam vs
MATLAB via the R4 .mat pipeline. 113/129 diverged on `lam` — proactive
generation beat waiting for a corpus re-encounter.

Distillation (3x3 `A = [1 9 3; 1/9 1 8; 1/3 1/8 1]`):
- the VALUES path `eig(A)` (real Francis Schur) was always CORRECT;
- the `[V,D]` path runs the COMPLEX Schur QR — whose bulge-chasing
  Givens read `h(k+1, k)` instead of the bulge position `h(k+1, k-1)`
  for k > l. The rotation never annihilated the bulge; the iteration
  budget ran out; the final forced lower-triangular zeroing then
  EMITTED GARBAGE EIGENVALUES silently ({1.54+3.66i, ...}).
- fix: `Complex g = (k == l) ? h(k+1, k) : h(k+1, k- 1);` — one line.
  Additionally the non-convergence path is now fail-closed
  (`eig: QR iteration failed to converge`) instead of silently
  zeroing, with a LAPACK-style 60*(n+1) budget.

Verified: all 129 hunt matrices — spectrum, A*V==V*D residual, and the
AHP Perron weights now match MATLAB (selection-invariant w/lam diffs:
0/129). Guard: EigAHPReciprocal3x3Spectrum (17-digit MATLAB-probed
spectrum + weights + residual). The eig column ORDER still differs
across engines (legal, MATLAB guarantees no order) — pinned by the
existing EigAHPConsistentPerronSelection.

Residual follow-up (filed separately): real-spectrum problems return
complex-typed V/D with ~1e-16 imaginary dust instead of MATLAB's real
matrices — linalg/eig-real-spectrum-complex-typing.
