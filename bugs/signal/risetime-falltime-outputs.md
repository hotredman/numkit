# signal.risetime / signal.falltime — only the first of up to 5 outputs

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing outputs)
- **Found:** 2026-06 via signal.* DEEP-PROBE sweep

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
