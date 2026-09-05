# linalg.eig — complex INPUT matrices rejected ("Not a double array")

- **Status:** ✅ FIXED (2026-09-05)
- **Severity:** P1 (a documented core API shape refused entirely; complex
  spectra code paths in the wild feed complex(A) directly)
- **Kind:** bug
- **Found:** 2026-09-05 via fieldtest portion 10 (MethodQR_Wshift.m —
  "Not a double array (in call to 'eig')")

## Symptom

`eig` rejects a complex matrix outright. MATLAB computes the
decomposition normally.

## Repro (self-contained)

```matlab
clear;
[V, D] = eig(complex([1 2; 3 4]));
disp(diag(D))
% numkit:  Error: Not a double array (in call to 'eig')
% MATLAB R2025b: -0.372281 and 5.37228
```

## Root cause (hypothesis)

The eig dispatch (eigVDAuto / eigValuesAuto) funnels into the symmetric
(double) Jacobi or a double-only general path; the complex branch of
schur_general exists (complexSchurQR) but the entry checks reject the
complex input before reaching it. Note the complex pipeline itself is
sound — the bulge-chase fix of 2026-09-05 exercises it via
complexified real inputs.

## References

- **Guard:** `DISABLED_EigComplexInput` in
  `src/toolboxes/linalg/tests/eig_test.cpp` (diag = -0.372281, 5.37228) —
  built and verified RED under --gtest_also_run_disabled_tests.


## Resolution (2026-09-05)

Root cause was exactly the hypothesis: eigVDAuto/eigValuesAuto called
isSymmetricApprox, which reads doubleData() and throws on complex
storage before any path choice. Fix: complex input routes directly to
the general path (its complex-Schur pipeline was already sound — the
same one hardened by the same-day bulge-chase fix). Verified:
eig(complex([1 2;3 4])) = {-0.372281, 5.372281} matching MATLAB;
guard live (EigComplexInput).
