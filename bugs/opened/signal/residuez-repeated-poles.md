# signal.residuez — repeated z-poles throw "not yet supported" (stub); the s-domain residue now supports repeats

- **Status:** 🔴 OPEN
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

## Suggested fix

Reuse the residue machinery via the substitution the file already
documents — but note z-domain repeated-pole semantics are their own
derivation (terms r/(1−p z^−1)^i carry extra z^m polynomial factors vs
the s-domain partial fractions), so verify the MATLAB convention with
probes first, then implement.

## References

- Guard: `DISABLED_ResiduezRepeatedPoles` in
  `src/builtin/tests/residue_test.cpp`
- The s-domain implementation: `residueS` in
  `src/builtin/src/polyfun/polynomials.cpp` (Taylor + Schroeder)
