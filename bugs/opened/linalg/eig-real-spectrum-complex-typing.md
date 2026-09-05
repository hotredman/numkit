# linalg.eig — real-spectrum problems return complex-typed V/D with ~1e-16 imaginary dust (MATLAB returns real)

- **Status:** 🔴 OPEN
- **Severity:** P3 (values are CORRECT; only the stored class differs — affects strict class comparisons and display)
- **Kind:** bug
- **Found:** 2026-09-05 during the AHP hunt verification (129-matrix .mat diff:
  every `w` flagged class c vs r; values 0/129 diffs after real-cast)

## Symptom

```matlab
clear;
A = [1 9 3; 0.1111111111111111 1 8; 0.3333333333333333 0.125 1];
[V, D] = eig(A);
disp(isreal(V))
% numkit:  0   (complex-typed, max|imag| = 0)
% MATLAB R2025b: 1
```

Values are bit-correct after a real cast; the complex-Schur pipeline
(eig_general_VD) leaves zero-or-dust imaginary parts that the strict
`narrow_if_real` (imag == 0.0 exactly) does not collapse.

## Root cause

`narrow_if_real` requires exactly-zero imaginary parts; the QR back-
substitution dust is ~1e-16. A tolerance-based narrowing was attempted
in eig_general_VD (real_spectrum && vImax <= 1e3*tol) but does not
trigger — investigation pending (the block appears unreachable for
this path; possibly the SVD/roots real-branch or the wrapper layer
re-widens).

## Suggested fix

Narrow V and D to double when the whole spectrum's |imag| is within
n*eps*(1+||A||) — mirroring LAPACK dgeev's real output for real
problems. Ensure the narrowing happens at the FINAL return of every
[V,D] path (roots+SVD real branch included — it already returns real,
verify; complex branch — fix).

## References

- **Guard:** deferred — values are pinned by EigAHPReciprocal3x3Spectrum
  (already green); add an isreal(V) assertion when fixed.
