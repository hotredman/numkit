# stats.smoothdata — 'sgolay' / 'lowess' / 'loess' methods throw

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing option)
- **Kind:** stub
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`smoothdata` supports `movmean`/`movmedian`/`gaussian` but throws on the
regression-based methods `sgolay`, `lowess`, `loess`.

## Repro
```matlab
smoothdata([1 5 2 8 3 9 4], 'sgolay')
% numkit: Error — smoothdata: method 'sgolay' not supported
%         (supported: 'movmean', 'movmedian', 'gaussian')
smoothdata([1 5 2 8 3 9 4], 'lowess')
% numkit: Error — smoothdata: method 'lowess' not supported
```

## Root cause
The method dispatch in `smoothdata` only wires three methods.

## Suggested fix
- `sgolay`: reuse the existing `sgolay`/`sgolayfilt` Savitzky-Golay
  machinery (REWIRE-to-existing). The hard part is matching MATLAB's
  **default window length** heuristic (data-dependent) — reverse-engineer
  it, or require an explicit window first and defer the default.
- `lowess`/`loess`: local linear / quadratic regression over a moving
  window (larger). Probe MATLAB defaults carefully.
Start with `sgolay` (machinery exists). Moderate.

## References
- `toolboxes/stats/src/.../smoothdata*` (method dispatch)
- shipped: `sgolay`, `sgolayfilt`
- MATLAB `doc smoothdata`
