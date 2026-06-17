# wavelet.wenergy / upcoef — 1-D wavelet energy + reconstruction helpers missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing functions)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep

## Symptom
Two 1-D Wavelet-Toolbox helpers that operate on a `wavedec` decomposition
are not registered: `wenergy` (percentage of energy per
approximation/detail level) and `upcoef` (direct reconstruction of
approximation/detail coefficients up N levels).

## Repro
```matlab
[c,l] = wavedec([1 2 3 4 5 6 7 8], 2, 'db1');
[Ea, Ed] = wenergy(c, l);
% MATLAB: Ea = 95.0980392157, Ed = [0.980392156863  3.92156862745]
%         (Ea + sum(Ed) == 100)
% numkit: Error — VM: undefined function 'wenergy'

y = upcoef('a', 5, 'db1', 2);   % lift one approx coeff up 2 levels
% MATLAB: numel(y) = 4, y(1) = 2.5
% numkit: Error — VM: undefined function 'upcoef'
```

## Root cause
Not implemented. numkit ships `wavedec`/`waverec`/`appcoef`/`detcoef`/
`wrcoef`/`detcoef` but not these two helpers.

## Suggested fix
- `wenergy(C,L)`: energy `Eapp = ‖a‖²`, `Edet(j) = ‖d_j‖²`; return as
  percentages of the total (`100·E/ΣE`). `[Ea,Ed]` = approx %, per-level
  detail % vector. Trivial — sum of squares of the `appcoef`/`detcoef`
  slices already extractable from `(C,L)`.
- `upcoef(O,X,wname,N)`: repeatedly upsample-and-convolve `X` with the
  reconstruction filter (`'a'`→lowpass, `'d'`→highpass for the first step)
  `N` times, keeping the central part. Reuses the `idwt`/`wrcoef` upsampling
  kernel. Small. Verify `Ea+ΣEd==100` and `upcoef` length/values vs MATLAB.

## References
- new file(s) under `src/toolboxes/wavelet/src/...`; reuse `wavedec`/`appcoef`/`idwt`
- MATLAB `doc wenergy`, `doc upcoef`
