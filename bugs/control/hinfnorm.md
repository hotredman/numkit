# control.hinfnorm — H-infinity norm of an LTI system missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE (split out of control/lqr-dlqr-gram on 2026-06-19)

## Symptom
`hinfnorm(sys)` — the peak gain `‖G‖∞ = sup_ω σ_max(G(jω))` of a stable
LTI system — is not registered. (Split from the original
lqr/hinfnorm/dlqr/gram cluster; lqr/dlqr/gram are fixed, see
control/lqr-dlqr-gram.md.)

## Repro
```matlab
hinfnorm(ss([0 1;-1 0],[0;1],[1 0],0))
% MATLAB: Inf  (poles at ±i are on the jω axis → norm is unbounded)
hinfnorm(ss(-1,1,1,0))
% MATLAB: 1   (G(s)=1/(s+1), peak |G(jω)| at ω=0 is 1)
```

## Root cause
Not implemented.

## Suggested fix
The exact value is computed by the **Boyd–Balakrishnan / Bruinsma–Steinbuch
Hamiltonian test** with bisection on γ: for a candidate γ, form

```
M(γ) = [ A + B R⁻¹ Dᵀ C      B R⁻¹ Bᵀ
        −Cᵀ(I + D R⁻¹ Dᵀ)C   −(A + B R⁻¹ Dᵀ C)ᵀ ],   R = γ²I − DᵀD
```

γ is an upper bound on `‖G‖∞` iff `M(γ)` has **no purely imaginary
eigenvalues**; bisect γ until convergence. numkit now has a general real
eigensolver (`francisSchur` / `eig`, see linalg/schur-nonsymmetric), so the
imaginary-axis test is feasible (check `|Re(λ)| < tol` over `eig(M(γ))`).

Edge cases to match MATLAB:
- **Inf** when the system has poles on the jω axis (unstable / marginally
  stable) — detect via the closed-loop/`A` eigenvalues before bisecting.
- The static-gain and `D≠0` terms (the `R = γ²I − DᵀD` correction).

A frequency-grid maximum of `σ_max(G(jω))` is only a *lower* bound (misses
sharp resonances between grid points) — fine for a sanity check, **not** for
parity, so the Hamiltonian test is required.

## References
- new entry point under `src/toolboxes/control/src/...` (freq or a new
  `norm`/`hinf` TU); cf. `sigma` (already computes σ_max(G(jω)) on a grid).
- shipped: `eig`/`schur` general (linalg), `care`/`dare`, `ss`.
- related: control/lqr-dlqr-gram.md (the rest of the original cluster)
- MATLAB `doc hinfnorm`, `doc norm` (for LTI systems)
