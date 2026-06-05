# signal.impinvar — wrong numerator for repeated poles

- **Status:** 🔴 OPEN
- **Severity:** P1 (wrong result for repeated-pole inputs)
- **Kind:** bug
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`impinvar` (impulse-invariance analog→digital) gives the wrong **numerator**
when the analog filter has REPEATED poles. The denominator (poles) is
correct, and the DISTINCT-pole case is fully correct.

## Repro
```matlab
[bz, az] = impinvar(1, [1 2 1], 10)      % double pole at s = -1
% numkit: bz = [-5.73738006  5.20212755]   (WRONG)
% MATLAB: bz = [ 0            0.00904837]
%         az = [1 -1.80967484 0.81873075]  (correct on both)

[bz, az] = impinvar(1, [1 3 2], 10)      % distinct poles -1, -2
% numkit == MATLAB:  bz = [0 0.00861067]  (distinct case OK)
```

## Root cause
The partial-fraction expansion's REPEATED-pole branch is wrong. For a pole
of multiplicity m, impulse invariance must map each term
`r/(s-p)^k` to the z-domain via the k-th impulse-invariant transform (which
for k≥2 involves the derivative / `z`-domain repeated-pole kernels), scaled
by `1/fs`. numkit's residue/kernel for k≥2 is incorrect.

## Suggested fix
Use a partial-fraction expansion that returns residues WITH multiplicity
(`residue` gives these), then for each repeated term apply the correct
impulse-invariant z-transform kernel (e.g. the standard table:
`1/(s-p)^2 → (T·z·e^{pT})/(z-e^{pT})^2`, etc.), summing back to `bz/az`.
Moderate (the repeated-pole kernels + reassembly). Distinct-pole path is
already correct, so this is isolated to the multiplicity≥2 branch. Validate
vs MATLAB for double/triple poles.

## Investigation (2026-06-05, c40) — coupled to a residue gap
- **`builtin::residue` is ALSO gapped for repeated poles** — it throws
  "repeated-pole case not yet supported (v1 distinct-poles only)". So this fix
  is COUPLED: impinvar needs repeated-pole residues, which residue can't yet
  provide. Either fix `residue` first (well-specified clean-room: Taylor-series
  division B(p+u)/Ã(p+u) to order M-1, where Ã = A with the (s-p)^M factor
  removed; residue for (s-p)^-m is the coeff of u^{M-m}), or compute the
  multiplicity residues inline in impinvar.
- The current impinvar uses the SIMPLE-pole residue r_k = b(p_k)/a'(p_k); at a
  repeated pole a'(p_k)=0 → r_k = ∞/NaN → the observed garbage numerator.
- **Verified clean-room z-kernel** (matches MATLAB): impulse invariance maps
  r/(s-p)^m to r·T^m/(m-1)!·Σ_{n≥0} n^{m-1} α^n z^{-n} with α=e^{pT}, and
  Σ n^{m-1} α^n z^{-n} = N_{m}(α z^{-1})/(1-α z^{-1})^m where the numerator
  satisfies N_1=1, N_{m+1}(w)=w·[(1-w)·N_m'(w)+m·N_m(w)] (Eulerian:
  N_2=w, N_3=w+w², N_4=w(1+4w+w²)). Reassemble over the common denominator
  a_d = ∏(1-α_k z^{-1})^{M_k} (numkit already builds this correctly). For the
  double-pole repro 1/(s+1)² @ fs=10: H_d = T²·α·z^{-1}/(1-α z^{-1})² with
  T²α = 0.00904837, giving bz=[0, 0.00904837] — EXACTLY MATLAB. numkit's
  output order (low→high z^{-1}) already equals MATLAB's bz, so no reordering.

## References
- `libs/signal/src/filter_design/analog_filters.cpp` (`impinvar`)
- BLOCKER: `builtin::residue` repeated-pole gap (libs/builtin/.../polynomials)
- MATLAB `doc impinvar`, `doc residue`
