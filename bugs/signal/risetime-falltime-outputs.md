# signal.risetime / signal.falltime — only the first of up to 5 outputs

- **Status:** ✅ FIXED (2026-06-03, lib-dev cycle c182)
- **Severity:** P2 (missing outputs) + **P1 value bug discovered while fixing**
- **Kind:** missing-output (+ bug)
- **Found:** 2026-06 via signal.* DEEP-PROBE sweep
- **Fix:** two parts.
  1. **VALUE bug** (the md's "R is correct" claim was WRONG): on a sharp
     single-sample edge the 10% and 90% reference levels both cross within
     one interval. `findTransitions` committed the transition one sample late
     and pinned the leading crossing to the following (flat) interval →
     `risetime([0 0 0 1 1 1 1],4)` gave **0.224** instead of MATLAB's
     **0.198**. Fixed by committing the transition immediately when a
     Below→Above (or Above→Below) jump crosses both boundaries in the same
     interval. Multi-sample ramps were already correct and are unchanged.
  2. **Outputs**: new `pulseRiseFall` helper + custom `risetime_reg`/
     `falltime_reg` emit `[R, LT, UT, LL, UL]` by `nargout` (LT/UT crossing
     times — for falls LT is last, UT first; LL/UL reference levels).
  Verified all 5 outputs (single + multi-transition, rise + fall) vs MATLAB
  R2025b. Guard: `libs/signal/tests/risetime_falltime_test.cpp`.

## Symptom
MATLAB `risetime` returns `[R, LT, UT, LL, UL]` (rise durations, lower/upper
transition times, lower/upper reference levels). numkit returns only `R`;
asking for more throws. `falltime` is almost certainly the same.

## Repro
```matlab
[R, LT, UT] = risetime([0 0 0 1 1 1 1], 4)
% numkit: Error — Too many output arguments
% MATLAB: R = 0.1980, LT = 0.5260, UT = 0.7240
```

## Root cause
`risetime`/`falltime` adapters in `libs/signal/src/measurements/` emit only
`outs[0]`. The transition crossing times (`LT`, `UT`) and the 10%/90%
reference levels (`LL`, `UL`) are computed internally to get `R = UT - LT`,
so the extra outputs are likely already available — just not wired to
`nargout`.

## Suggested fix
Thread `nargout`; emit LT/UT/LL/UL from the values already computed. Verify
the per-edge column shapes and the default 10%/90% percent-reference levels
against MATLAB. Likely cheap (missing-Nth-output, data already there).

## References
- `libs/signal/src/measurements/` (risetime/falltime)
- MATLAB `doc risetime`
