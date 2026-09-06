# linalg.qr — sign convention of the trailing Householder column differs from MATLAB/LAPACK (Q·R still a valid QR)

- **Status:** ✅ CLOSED (d889c7060, 2026-09-06) — trailing-column convention FIXED; the remainder is not-a-defect (1-ulp matmul rounding amplified by iteration chaos)
- **Kind:** bug
- **Severity:** P3 convention divergence (valid factorization, different signs — visible in workspace compares)
- **Found:** 2026-09-06 via the springer-math compare group — 6 Numerical_Linear_Algebra scripts (HessenbergQR, InverseIteration, MethodOrtIter, MethodQR_iter, MethodQR_shift, MethodQR_Wshift) flagged workspace-mismatch at max rel exactly 2.0 (sign flips) after their num2str/eig blockers were fixed

## Status 2026-09-06 (portion 18): trailing-column HALF FIXED

The root cause was our Householder loop reflecting the trailing 1x1
block; LAPACK dlarfg leaves it UNTOUCHED when the subdiagonal tail is
zero (tau=0, no reflector). Fixed in qrFullHouseholder AND
qrPivotedHouseholder (decompositions.cpp). qr is now element-exact vs
MATLAB on probed matrices: hilb(10), rand(4), [0 -5 2; 6 0 -12; 1 3 0],
[3 7 8 9; 5 -7 4 -7; 1 -1 1 -1; 9 3 2 5]. 2 of the 6 corpus scripts
flipped to PASS (MethodOrtIter, MethodQR_iter); live guard
`QrLastColumnSignConvention`.

REMAINING 4 scripts — CLOSED as not-a-defect (distillate 2026-09-06):
the Hessenberg output and the FIRST qr call are identical to 10
digits; iteration traces stay identical to 12 digits for 10 RQ steps;
the FIRST divergence is 1 ulp at iteration 11 (A(2,1) ...054599 vs
...054598) — matmul rounding order (numkit vs MATLAB's MKL). Over the
remaining ~990 chaotic iterations that 1-ulp seed decorrelates the
trajectory and the converged 2x2 Schur block lands with the opposite
sign layout: identical spectrum, identical |values|, deterministic
within each engine (numkit's wasm double-run in the compare harness
proves self-consistency). Per the playbook rule 5 (summation-order
variance is not a bug) — matching would require a bit-identical BLAS,
which is out of scope by design. The 4 scripts carry a
`# known: numerical-iteration-chaos` compare annotation.

## Original symptom

## Repro

```matlab
clear;
[Q1, R1] = qr([0 -5 2; 6 0 -12; 1 3 0]);
% MATLAB R1(3,3) = 2.7164,  Q1(:,3) = [0.5093; -0.1415; 0.8489]
% numkit R1(3,3) = -2.71638, Q1(:,3) = [-0.5093; 0.1415; -0.8489]
% (the other columns/rows match; A = Q1*R1 holds in both)
% Control: 50 RQ iterations converge to the SAME A in both engines.
```

## Root cause

Our Householder construction picks the opposite sign for the trailing
reflector than LAPACK's dgeqrf (which chooses β = −sign(x₁)·‖x‖ the same
way for interior columns but the final 1×1/last column lands differently).
Needs a targeted comparison of the reflector sign choice per column
against LAPACK, then alignment of ours.

## Suggested fix

Align the Householder sign choice in the qr factorization
(src/toolboxes/linalg + the LAPACK-matching convention) so Q/R are
element-exact vs MATLAB for double inputs; cover with a parity spec on
several matrices (including this one). Verify the orthogonality /
A-reconstruction guards stay green.

## References

- Guard: `QrLastColumnSignConvention` (LIVE) pins the fixed
  trailing-column convention; the chaos remainder has no assertable
  guard by nature (both forms are correct).
- Affected corpus scripts: see `fieldtest/compare/groups/springer-math.txt`
  annotations `# known: linalg/qr-last-column-sign-convention`
