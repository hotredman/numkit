# math.psi — psi(0) returns NaN (should be -Inf)

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P3 (wrong value at the 0 pole; psi/digamma is common in stats)
- **Kind:** bug
- **Found:** 2026-06-05 via DEEP-PROBE (special-math pole sweep, cycle 56)

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 56),
  `src/math/src/special/special.cpp` (`psiScalar`). The digamma kernel
  returned NaN at every non-positive integer; MATLAB returns **-Inf** at the
  pole `psi(0)`.
- Fix: special-case `x == 0.0 -> -Inf` before the generic non-positive-integer
  branch. Finite positive values are unchanged; negative integers still return
  NaN (see "Related" — MATLAB rejects negative input entirely, so its exact
  value there is moot and the lenient NaN is kept rather than introducing an
  error).
- Verified vs MATLAB R2025b: `psi(0)=-Inf`, `psi(1)=-0.57721566`,
  `psi(2)=0.42278434`, `psi(0.5)=-1.96351`, `psi(10)=2.2517526`,
  `psi([0 1 2])=[-Inf -0.577216 0.422784]`.
- Live guard: `SpecialFuncsTest.PsiZeroPole` (new) + `BuiltinKnownBug.PsiZeroPole`.
  Parity: `tools/parity/specs/psi_zero.json` (correctness=OK). Smoke:
  `src/math/tests/smoke/psi_zero_smoke.m`.

## Symptom
`psi(0)` returns NaN; MATLAB returns -Inf (digamma has a pole at 0).

## Repro
```matlab
psi(0)        % numkit: NaN;  MATLAB: -Inf
psi([0 1 2])  % numkit: [NaN -0.5772 0.4228];  MATLAB: [-Inf -0.5772 0.4228]
psi(1)        % -0.57721566 on both (unchanged)
```

## Root cause
`psiScalar` mapped every `x == floor(x) && x <= 0` to NaN; the 0 boundary is a
genuine pole that MATLAB reports as -Inf.

## Related (NOT fixed — separate / murky)
- `psi(NEGATIVE)`: MATLAB ERRORS ("X must be nonnegative"); numkit returns NaN
  (integers) or the reflection-formula value (non-integers). The negative-domain
  error semantics are a separate decision and left as-is.
- `gammaln(-1)` similarly: MATLAB errors, numkit lenient.

## References
- `src/math/src/special/special.cpp` (`psiScalar`)
- MATLAB `doc psi` (ψ(0) = -Inf; input must be nonnegative)
