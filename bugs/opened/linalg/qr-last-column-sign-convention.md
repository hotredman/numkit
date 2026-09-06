# linalg.qr — sign convention of the trailing Householder column differs from MATLAB/LAPACK (Q·R still a valid QR)

- **Status:** 🔴 OPEN
- **Kind:** bug
- **Severity:** P3 convention divergence (valid factorization, different signs — visible in workspace compares)
- **Found:** 2026-09-06 via the springer-math compare group — 6 Numerical_Linear_Algebra scripts (HessenbergQR, InverseIteration, MethodOrtIter, MethodQR_iter, MethodQR_shift, MethodQR_Wshift) flagged workspace-mismatch at max rel exactly 2.0 (sign flips) after their num2str/eig blockers were fixed

## Symptom

`[Q, R] = qr(A)` returns an equally valid QR factorization, but the sign
of the LAST reflector's column/row differs from MATLAB (LAPACK dgeqrf):
Q's final column and R's final row are negated relative to MATLAB. The
iterated RESULTS (RQ-iteration eigenvalue convergence, eigenvalue set)
match — only the saved Q/R workspace variables diverge, at max rel
exactly 2.000e+00 (a ↔ −a).

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

- Guard: `DISABLED_QrLastColumnSignConvention` in
  `src/toolboxes/linalg/tests/known_bugs_test.cpp`
- Affected corpus scripts: see `fieldtest/compare/groups/springer-math.txt`
  annotations `# known: linalg/qr-last-column-sign-convention`
