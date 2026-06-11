# signal.impinvar — wrong numerator for repeated poles

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P1 (wrong result for repeated-pole inputs)
- **Kind:** bug
- **Found:** 2026-06 via DEEP-PROBE

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 41),
  `toolboxes/signal/src/filter_design/analog_filters.cpp` (`impinvar`). Rewrote the
  whole partial-fraction → z-domain path to handle ANY pole multiplicity
  (distinct case reduces to the old formula and is unchanged). NO change to
  `builtin::residue` (kept residues inline — partial-fraction ORDER is
  irrelevant for the final bz/az sum, avoiding the fragile residue-ordering
  match).
- Algorithm (per the Investigation notes below, all verified bit-exact):
  cluster the roots of `a` (mpoles-style, rel tol 1e-3), take each cluster
  CENTROID and Newton-refine it on `a^{(m-1)}` (an m-fold root of `a` is a
  SIMPLE root of `a^{(m-1)}`, so `s -= t_{m-1}/(m·t_m)` converges to the exact
  pole — fixes the `roots()` spread that made the triple/quad cases imprecise);
  residues with multiplicity via Taylor-series division of `B(p+u)/Ã(p+u)`
  (`Ã` = `a` Taylor about `p` shifted by `M`), `r_m = c_{M-m}`; then the
  impulse-invariant Eulerian z-kernel `r_m·Tᵐ/(m-1)!·N_m(α z⁻¹)·(1-α z⁻¹)^{M-m}`
  times the co-factor, reassembled over `a_d = ∏(1-α z⁻¹)^M`.
- Verified vs MATLAB R2025b: double `1/(s+1)²` → `bz=[0 0.00904837418]`,
  `az=[1 -1.809674836 0.8187307531]`; triple `1/(s+1)³` →
  `bz=[0 0.000452418709 0.0004093653765]`; quadruple `(s+1)⁴`; mixed
  `[1 2]/((s+1)²(s+2))` → `bz=[0 0.00904837418 -0.007408182207]`; distinct
  `1/((s+1)(s+2))` unchanged (regression).
- Live guard: `toolboxes/signal/tests/impinvar_test.cpp` (5 TEST_F) + flipped
  `SignalKnownBug.ImpinvarRepeatedPoles` live. Parity:
  `tools/parity/specs/impinvar.json` strengthened with real bz/az fingerprints
  (was numel-only; correctness=OK). Smoke: `toolboxes/signal/tests/smoke/impinvar_smoke.m`.

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
- `toolboxes/signal/src/filter_design/analog_filters.cpp` (`impinvar`)
- BLOCKER: `builtin::residue` repeated-pole gap (toolboxes/builtin/.../polynomials)
- MATLAB `doc impinvar`, `doc residue`
