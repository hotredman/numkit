# image.watershed — function missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`watershed` (watershed transform for segmentation) is not registered.

## Repro
```matlab
L = watershed(magic(5))
% numkit: Error — VM: undefined function 'watershed'
% MATLAB: label matrix with 0 ridge lines separating catchment basins
```

## Root cause
Not implemented.

## Suggested fix
Meyer's flooding watershed: sort pixels by intensity, grow basins from
regional minima via a priority queue, mark watershed ridges (label 0) where
distinct basins meet. Default connectivity 8 (2-D). Larger algorithm; pairs
naturally with the distance-transform watershed workflow
(`bwdist` + `imhmin` + `watershed`, all of which numkit partially has).
Validate the label matrix + ridge placement vs MATLAB on small inputs
(note: MATLAB's exact tie-breaking / labeling order must be matched).

## References
- new file under `libs/image/src/...`
- shipped: `bwdist`, `imhmin`, `bwlabel`, `imreconstruct`
- MATLAB `doc watershed`
