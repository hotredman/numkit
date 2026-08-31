# signal.freqs — two-arg form `freqs(b, a)` (auto frequency grid) rejected: "requires (b, a, w)"

- **Status:** 🔴 OPEN
- **Severity:** P2 (works in MATLAB, refused in numkit; the quick-look form)
- **Kind:** stub
- **Found:** 2026-08-31 via fieldtest portion 1 (mdadams book, freqs_example_1.m)

## Symptom

`freqs(b, a)` — the documented no-w form that plots/returns the response
on an automatically chosen 200-point grid — is rejected; only the
explicit-w 3-arg form is accepted.

## Repro (self-contained)

```matlab
clear;
h = freqs([1], [1 sqrt(2) 1]);
disp(numel(h))
% numkit:  Error: freqs: requires (b, a, w) (in call to 'freqs')
% MATLAB R2025b: 200  (2nd-order Butterworth prototype, auto grid)
```

MATLAB's auto grid (documented): 200 points logarithmically spaced, chosen
around the filter's interesting frequency range. With an output argument
it returns h; without one it PLOTS the response — the no-output plot form
is part of the same gap.

## Root cause

Argument-count validation demands exactly (b, a, w).

## Suggested fix

Support `freqs(b, a)` (auto grid, 200 log-spaced points per MATLAB docs;
mirror MATLAB's grid-range heuristic — probe R2025b for the exact rule)
and the zero-output plotting form. Exact grid parity can follow; erroring
is the bug.

## References

- **Guard:** `DISABLED_FreqsTwoArgAutoW` in
  `src/toolboxes/signal/tests/filter_design_test.cpp`.
