# signal.impinvar — wrong numerator for repeated poles

- **Status:** 🔴 OPEN
- **Severity:** P1 (wrong result for repeated-pole inputs)
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

## References
- `libs/signal/src/.../impinvar*`
- shipped: `residue` (poles/residues with multiplicity)
- MATLAB `doc impinvar`
