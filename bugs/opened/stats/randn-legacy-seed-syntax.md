# stats.randn — legacy seed syntax `randn('seed', n)` rejected ("Cannot convert char to scalar")

- **Status:** 🔴 OPEN
- **Severity:** P2 (works in MATLAB, refused in numkit; classic textbook code depends on it)
- **Kind:** stub (documented MATLAB option not supported)
- **Found:** 2026-08-31 via fieldtest portion 1 (mdadams book, example_11.m)

## Symptom

MATLAB's legacy RNG-state syntax throws a conversion error instead of
seeding the stream. Real-world textbook code (pre-`rng` era) uses it
ubiquitously.

## Repro (self-contained)

```matlab
clear;
randn('seed', 5);
x = randn(2, 1);
disp(x(1))
% numkit:  Error: Cannot convert char to scalar (in call to 'randn')
% MATLAB R2025b: 0.58185  (legacy seed 5, first draw; x(2) = -0.30514)
```

The same applies to `rand('seed', n)` (and `'state'`, `sum
('state', …)`-style legacy forms) — verify each when fixing.

## Root cause

`randn` (and `rand`) argument parsing expects numeric args only; the
`('seed'|'state', value)` char-flag form is not recognised.

## Suggested fix

Accept the legacy state forms in the RNG builtins: `randn('seed', n)` /
`randn('state', n)` seed the default stream (MATLAB semantics: legacy
generator, documented in `rng` docs "Legacy" section). Minimal honest
behaviour if exact legacy-generator parity is out of scope: seed the
Mersenne stream with n and document the divergence — but the call must
not error. Exact legacy sequence parity is a separate, lower-priority gap.

## References

- **Guard:** `DISABLED_RandnLegacySeedSyntax` in
  `src/toolboxes/stats/tests/randg_mvnrnd_test.cpp` (asserts the call
  runs and returns a draw; exact value parity with the legacy generator
  is NOT asserted — sequence parity is a stretch goal, erroring is the
  bug).
