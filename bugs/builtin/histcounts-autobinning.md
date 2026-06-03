# builtin.histcounts — automatic binning unsupported (edges required)

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing input form)
- **Kind:** stub
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`histcounts(x)`, `histcounts(x, nbins)`, and the `'BinWidth'` / `'BinLimits'`
forms throw — numkit requires explicit bin edges. MATLAB auto-selects bins.

## Repro
```matlab
[N, e] = histcounts([1 2 2 3 3 3])
% numkit: Error — histcounts: bin edges required — automatic binning
%         (nbins / 'BinWidth' / 'BinLimits') is not supported
% MATLAB: N = [1 2 3],  edges = [0.5 1.5 2.5 3.5]
[N, e] = histcounts([1 2 3 4 5 6 7 8 9 10], 3)
% MATLAB: N = [3 4 3]
```

## Root cause
The auto-binning rule is not implemented; only the explicit-edges path
exists.

## Suggested fix
Implement MATLAB's automatic bin selection: the default rule picks a "nice"
bin width (a variant of the Freedman–Diaconis / Scott estimate snapped to
1/2/5·10^k, integer-aware for integer data), plus the `nbins` and
`'BinWidth'`/`'BinLimits'` forms. Getting the exact MATLAB edges is the
fiddly part — moderate. The downstream count + 'Normalization' code already
works once the edges exist. Validate edges + counts vs MATLAB across
integer and continuous inputs.

## References
- `libs/builtin/src/.../histcounts*`
- MATLAB `doc histcounts` (automatic binning algorithm)
