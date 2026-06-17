# control.minreal — minimal realization missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep

## Symptom
`minreal(sys)` — cancel pole/zero pairs (transfer functions) or remove
uncontrollable/unobservable states (state space) to produce a minimal
realization — is not registered.

## Repro
```matlab
sysr = minreal(tf([1 1], [1 2 1]));   % (s+1)/(s+1)^2  ->  1/(s+1)
[n,d] = tfdata(sysr, 'v');
% MATLAB: n = [0 1], d = [1 1]
% numkit: Error — VM: undefined function 'minreal'
```

## Root cause
Not implemented. numkit has `tf`/`ss`/`tfdata`/`pole`/`zero` but no
cancellation pass.

## Suggested fix
- TF form: factor numerator/denominator (roots), cancel common roots within
  a tolerance, rebuild. numkit already has `roots`/`poly`.
- SS form: Kalman decomposition — drop uncontrollable then unobservable
  subspaces (uses `ctrb`/`obsv`, both shipped), or balance-and-truncate the
  near-zero Hankel-singular-value states. Medium. Verify the reduced
  num/den (or state count) vs MATLAB.

## References
- new file under `src/toolboxes/control/src/...`
- shipped: `tf`/`ss`/`tfdata`/`ctrb`/`obsv`/`roots`/`poly`
- MATLAB `doc minreal`
