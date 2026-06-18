# control.hinfnorm — H-infinity norm of an LTI system missing

- **Status:** ✅ FIXED (2026-06-19) — Hamiltonian-test bisection
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE (split out of control/lqr-dlqr-gram on 2026-06-19)

## Symptom
`hinfnorm(sys)` — the peak gain `‖G‖∞ = sup_ω σ_max(G(jω))` of a stable
LTI system — is not registered.

## Repro
```matlab
hinfnorm(ss([0 1;-1 0],[0;1],[1 0],0))
% MATLAB: Inf  (poles at ±i are on the jω axis → norm is unbounded)
hinfnorm(ss(-1,1,1,0))
% MATLAB: 1   (G(s)=1/(s+1), peak |G(jω)| at ω=0 is 1)
```

## Root cause
Not implemented.

## Fix (2026-06-19)
Implemented `numkit::control::hinfnorm` (`analyze/hinfnorm.cpp`) by the
**Bruinsma–Steinbuch Hamiltonian test with bisection on γ**. For a
candidate γ, form

```
R    = γ²I − DᵀD
Ā    = A + B R⁻¹ DᵀC
M(γ) = [  Ā                    B R⁻¹ Bᵀ
         −Cᵀ(I + D R⁻¹ Dᵀ)C    −Āᵀ ]
```

γ is an upper bound on `‖G‖∞` **iff** `R ≻ 0` (γ > σ_max(D)) **and** `M(γ)`
has no purely imaginary eigenvalue. Bisect γ between a lower bracket (any
frequency-point gain ≤ `‖G‖∞` — here the DC gain `D − C A⁻¹ B` and the
ω→∞ gain `D`, σ_max'd by a lower-bound-safe Rayleigh power iteration) and
an upper bound found by doubling. **No frequency sweep** — the Hamiltonian
test is exact, so it catches sharp resonances a grid would miss between
samples. `M(γ)`'s spectrum comes from `charPoly → math::roots` (the same
path `pole` uses — all real arithmetic, no complex SVD). Returns `Inf`
when any pole sits on/right of the jω axis (`max Re(eig(A)) ≥ −1e-9`).

Verified vs MATLAB R2025b (parity `hinfnorm.json` → OK): `1/(s+1)`→1;
`±i` poles→Inf; lightly-damped resonance `1/(s²+0.1s+1)`→10.012523 (peak
near ω=1, **not** at a grid point); static peak `1/(s+2)+1/(s+3)`→0.83333;
`D=0.5`→1.5; `tf(1,[1 2 1])`→1. Guards: `hinfnorm_test.cpp` (8 TEST_F:
DC peak / resonance / static / feedthrough / marginal-Inf / unstable-Inf /
tf-input / discrete-throws), `known_bugs_test.cpp` (`Hinfnorm`, promoted
live); smoke `hinfnorm_smoke.m`.

Deferred: discrete-time systems (the unit-circle test differs — a Tustin
bilinear transform to continuous, which preserves `‖G‖∞`, would wire it
up; throws a clear error for now).

With this the original lqr/hinfnorm/dlqr/gram cluster is fully closed.

## References
- `src/toolboxes/control/src/analyze/hinfnorm.cpp`,
  `.../include/numkit/control/analyze/analyze.hpp` (`hinfnorm`),
  `src/bundle/src/register/control/analyze/analyze_reg.cpp` (`hinfnorm_reg`).
- `tools/parity/specs/hinfnorm.json`.
- related: control/lqr-dlqr-gram.md (the rest of the original cluster),
  control/care-dare.md, linalg/schur-nonsymmetric.md (general eig).
- MATLAB `doc hinfnorm`, `doc norm` (for LTI systems)
