# math.gamma — NaN at negative-integer poles (should be +Inf)

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P3 (wrong value at the pole points; gamma is common)
- **Kind:** bug
- **Found:** 2026-06-05 via DEEP-PROBE (special-math pole/edge sweep, cycle 54)

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 54),
  `src/math/src/special/special.cpp` (`gammaFn`). The kernel was
  `std::tgamma(v)`, which returns NaN at NEGATIVE integers (it returns +Inf
  only at 0). MATLAB returns **+Inf** at every non-positive integer pole
  (0, -1, -2, …) and also `gamma(-Inf)=Inf`.
- Fix: map `v <= 0 && v == floor(v)` to `+Inf` (this also covers `-Inf`, since
  `floor(-Inf)==-Inf`); `+Inf` (v>0) and `NaN` fall through to `tgamma`
  unchanged. Non-integer negatives (e.g. `gamma(-0.5)=-3.5449`) are untouched.
- Verified vs MATLAB R2025b:
  `gamma([-1 -2 -3 0 0.5 -0.5 5])` = `[Inf Inf Inf Inf 1.7725 -3.5449 24]`;
  `gamma(-Inf)=Inf`, `gamma(Inf)=Inf`, `gamma(NaN)=NaN`.
- Live guard: tightened `SpecialFuncsTest.GammaNegativeIntegerIsPole` (was a
  loose `!isfinite`; now asserts +Inf at -1 and -2) +
  `BuiltinKnownBug.GammaNegativeIntegerPoles`. Parity:
  `tools/parity/specs/gamma_poles.json` (correctness=OK). Smoke:
  `tests/builtin/smoke/gamma_poles_smoke.m`.

## Symptom
`gamma` of a negative integer returns NaN; MATLAB returns +Inf (pole).

## Repro
```matlab
gamma(-1)    % numkit: NaN;  MATLAB: Inf
gamma(-2)    % numkit: NaN;  MATLAB: Inf
gamma(-Inf)  % numkit: NaN;  MATLAB: Inf
gamma(0)     % Inf on both;  gamma(-0.5) = -3.5449 on both
```

## Root cause
`gammaFn` delegated straight to `std::tgamma`, whose C++-standard behaviour at
negative integers is NaN (not the +Inf MATLAB reports for the poles).

## Related (NOT fixed — separate / murky)
- `psi(0)` returns NaN; MATLAB returns -Inf (digamma pole). And `psi` of a
  NEGATIVE argument: MATLAB ERRORS ("X must be nonnegative") while numkit
  returns NaN — so psi's domain handling differs in two ways. Left for a
  separate cycle (the negative-domain error semantics need their own decision).
- `gammaln(-1)`: MATLAB ERRORS ("Input must be nonnegative"); numkit returns
  Inf (lenient). Separate.

## References
- `src/math/src/special/special.cpp` (`gammaFn`)
- MATLAB `doc gamma` (poles at non-positive integers → Inf)
