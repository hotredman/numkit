# linalg.qr — sign convention of the trailing Householder column differs from MATLAB/LAPACK (Q·R still a valid QR)

- **Status:** 🔴 OPEN
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

REMAINING: HessenbergQR, InverseIteration, MethodQR_Wshift,
MethodQR_shift still diverge at max rel 2.0 (sign flips) — NOT the
trailing column (qr is exact on their input matrices, probed): the
divergence lives elsewhere in their pipelines (manual Hessenberg
reduction with sign(x(1))*norm reflectors / shifted iterations /
inverse-power solves). Needs a fresh distillate of the failing iterate.

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

- **Guard:** deferred — the REMAINING 4-script divergence has no
  reproducible guard until its distillate lands (needs the failing
  iterate matrices pinned). The FIXED trailing-column half is pinned by
  the live `QrLastColumnSignConvention` guard in
  `src/toolboxes/linalg/tests/known_bugs_test.cpp`.
- Affected corpus scripts: see `fieldtest/compare/groups/springer-math.txt`
  annotations `# known: linalg/qr-last-column-sign-convention`
