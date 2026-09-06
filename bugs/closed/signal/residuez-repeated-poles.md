# signal.residuez — repeated z-poles throw "not yet supported" (stub); the s-domain residue now supports repeats

- **Status:** ✅ FIXED (9caf0d286, 2026-09-06)
- **Kind:** stub
- **Severity:** P2 missing feature
- **Found:** 2026-09-06 while adding repeated-pole support to `residue` — `residuez` shares the concept but has its own distinct-pole-only path

## Symptom

`residuez` throws for repeated poles. The s-domain `residue` was
upgraded the same day (Taylor-series method, Schroeder pole polish,
guards `ResidueDoublePole*` / `ResidueTriplePole`); `residuez` still
computes via the Oppenheim distinct-pole formula and keeps the
explicit stub throw.

## Repro

```matlab
clear;
[r, p, k] = residuez([1], [1 -2 0.25]);   % 1/(1 - 0.5 z^-1)^2
% numkit:  error "residuez: repeated-pole case not yet supported"
% MATLAB R2025b:  r/p for the (1 - p z^-1)^1..2 expansion
```

## Fix

Full port of MATLAB's residuez.m (read via `type`): the impulse/S-matrix
method — poles grouped by mpoles semantics (desc |p|, groups compacted
consecutively, tol 1e-3), simple poles by cover-up at 1/p over FLIPPED
polynomials (the flip matters: evaluating the descending build directly
differs by (1/p)^deg — a sign on odd degrees), repeated-pole residues by
least squares over the cumulative-filter basis; direct terms for
IMPROPER TFs via deconv on flipped polys (the old improper-TF throw is
gone too). DK conjugate-dust on real multiple roots is snapped to the
real axis (1e-4, consistent with the mpoles tolerance) so the pipeline
stays real-typed for real coefficients — MATLAB's own probe values
carry root-finder dust (its triple pole prints spread 7e-6), ours come
out cleaner. Live guards: ResiduezRepeatedPoles (4 probed cases) +
ResiduezImproperDirectTerm (k=[-2 0], r=4, probed).

## References

- Guard: `ResiduezRepeatedPoles` (live) + `ResiduezImproperDirectTerm`
  in `src/builtin/tests/residue_test.cpp`
- The s-domain sibling: `residueS` (Taylor + Schroeder), same file
