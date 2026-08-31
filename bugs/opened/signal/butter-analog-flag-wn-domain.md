# signal.butter — analog flag `'s'` not honoured: `butter(30, 2000, 's')` errors "Wn must be between 0 and 1"

- **Status:** 🔴 OPEN
- **Severity:** P2 (works in MATLAB, refused in numkit; analog filter design is standard textbook material)
- **Kind:** stub (documented option ignored/rejected)
- **Found:** 2026-08-31 via fieldtest portion 1 (mdadams book, example_4.m)

## Symptom

The analog-filter form `butter(N, Wn, 's')` — where Wn is an angular
frequency in rad/s, valid for ANY positive value — is validated against
the DIGITAL Wn∈(0,1) constraint and rejected.

## Repro (self-contained)

```matlab
clear;
[a, b] = butter(30, 2000, 's');
disp(numel(a))
% numkit:  Error: butter: Wn must be between 0 and 1 (in call to 'butter')
% MATLAB R2025b: 31  (analog 30th-order lowpass, Wn = 2000 rad/s)
```

Followed in the source script by `freqs(a, b, linspace(0, 6000, 512))` —
the classic analog filter design + frequency response pattern.

## Root cause

The Wn-domain check runs before (or without consulting) the 's' flag;
in analog mode the constraint is Wn > 0, not 0 < Wn < 1.

## Suggested fix

Thread the analog flag into the Wn validation and the design path: for
`'s'`, design in the s-domain (no bilinear prewarp), Wn > 0. The 's' flag
also composes with filter types: `butter(N, Wn, 'high'/'stop', 's')`.

## References

- **Guard:** `DISABLED_ButterAnalogFlagWnDomain` in
  `src/toolboxes/signal/tests/filter_design_test.cpp`.
