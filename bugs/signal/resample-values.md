# signal.resample — wrong output values

- **Status:** ✅ FIXED (2026-06-19) — MATLAB firls/kaiser/upfirdn pipeline
- **Severity:** P1 (wrong result)
- **Kind:** bug
- **Found:** 2026-06 via DEEP-PROBE
- **Note:** part of the deferred MULTIRATE gap (decimate/resample/interp
  delay compensation), recorded here with a concrete repro.

## Symptom
`resample(x, p, q)` returns values that do not track the input — the
antialiasing/polyphase filtering (and edge/delay compensation) is wrong.

## Repro
```matlab
resample([1 2 3 4 5 6], 3, 2)
% numkit: [0.00448 0.01466 -0.07709 0.11110 0.80764 1.51393 2.17249 2.82780 3.49960]
%         (sum 10.87 — ramps up from ~0, lags the signal)
% MATLAB: [1.00061 1.80791 2.16807 3.00182 3.94099 3.96567 5.00303 6.56811 4.24029]
%         (sum 31.70 — tracks the 1..6 ramp at the new rate)
```

## Root cause
The resample implementation's polyphase FIR (Kaiser-window antialiasing
filter design + the group-delay/edge compensation MATLAB applies) is
incorrect, so the resampled samples are mis-scaled and mis-aligned.

## Fix (2026-06-19)
The old `resample` used a custom Hamming windowed-sinc FIR with no
group-delay compensation or proper trim — hence the garbage output.
Rewrote it as MATLAB `resample.m` (`multirate.cpp`), reusing the shipped
**firls / kaiser / upfirdn** (all bit-exact with MATLAB):

```
g = gcd(p,q);  p/=g;  q/=g;                       % reduce first
N = 10;  beta = 5;  pqmax = max(p,q);  fc = 1/(2*pqmax);  L = 2*N*pqmax+1
h = p * firls(L-1,[0 2fc 2fc 1],[1 1 0 0]) .* kaiser(L,beta) / sum(...)   % sum(h)=p
Lhalf  = (L-1)/2;  nz = q - mod(Lhalf,q);  hh = [zeros(1,nz) h]
yfull  = upfirdn(x, hh, p, q)
delay  = floor((Lhalf+nz)/q);  Ly = ceil(Lx*p/q)
y      = yfull(delay+1 : delay+Ly)              % group-delay trim
```

The one subtlety that made the filter exact: MATLAB normalises by
`sum(firls.*kaiser)` over **all** taps (so `sum(h) = p`), not by
`sum(h(1:p:end))` over the polyphase branch — the two differ by a constant
≈1.0006 factor that was the entire filter discrepancy.

Verified vs MATLAB R2025b — **bit-exact** (maxdiff 0): the repro
`resample([1..6],3,2) = [1.00061 1.80791 2.16807 3.00182 3.94099 3.96567
5.00303 6.56811 4.24029]` (sum 31.6965, len 9); plus 2/1, 1/2, a 50-sample
sine 3/2, DC preservation, GCD reduction (4/2 == 2/1), column orientation.
Parity `resample.json` (strengthened from the old length-only spec) → OK.

## References
- `src/toolboxes/signal/src/multirate/multirate.cpp` (`resample`)
- reused (all bit-exact w/ MATLAB): `firls`, `kaiser`, `upfirdn`
- `tools/parity/specs/resample.json`,
  `src/toolboxes/signal/tests/resample_test.cpp` (value cases),
  `known_bugs_test.cpp` (`ResampleValues`, promoted live),
  smoke `tests/smoke/resample_multirate_smoke.m`
- MATLAB `doc resample`
