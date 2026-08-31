# signal.freqs — two-arg form `freqs(b, a)` (auto frequency grid) rejected: "requires (b, a, w)"

- **Status:** ✅ FIXED (2026-08-31)
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


## Resolution addendum (2026-08-31, follow-up "идентично MATLAB")

The grid was upgraded from our own heuristic to the CLASSIC freqint
algorithm (Andy Grace 7-6-90, rev 1996 — the ancestor still live inside
MATLAB's freqs; source preserved in the Marine Systems Simulator
`HYDRO/utils/freqs.m`), reverse-verified against R2025b with 40+ probes:

- extremes: `low = round(log10(0.1·min(|Re ez| + 2·Im ez)) − 0.5)`,
  `high = round(log10(max(3·|Re ez| + 1.5·Im ez)) + 0.5)` over the
  upper-half roots ez (poles ∪ zeros<1e5); round is half-away-from-zero —
  which is what produces the "anomaly" c=100 → lower 10 (round(0.5)=1);
  no poles → synthetic pole at −1000, which DERIVES the documented
  [100, 1e4] default instead of hardcoding it;
- long grid: logspace(low, high, 200 + (P−Z) + 10·[any real-dominant
  zero]), with refinement windows [0.8·Im−3|Re|, 1.2·Im+4|Re|] replacing
  base points around oscillatory roots (Im > |Re|), then resampled to
  exactly 200 points by linear-in-log10 interpolation at evenly spaced
  INDEX positions.

Verified: 29-case endpoint sweep — 20/29 cases fully bit-identical, ALL
endpoints bit-exact; the 9 remaining differ only in the 2nd grid point at
1–2 ulp (MSVC pow/log10 vs MATLAB's libm through the log→interp→pow
chain — the physical cross-libm limit, far below the R4 1e-9 comparison
threshold). Guard: exact endpoint assertions across every rule branch.
Parity spec: freqs_2arg (correctness=OK).
