# image.imfindcircles — function missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`imfindcircles` (circular Hough transform) is not registered.

## Repro
```matlab
[centers, radii, metric] = imfindcircles(img, [2 6])
% numkit: Error — VM: undefined function 'imfindcircles'
% MATLAB: detected circle centers / radii / strength, sorted by metric
```

## Root cause
Not implemented.

## Suggested fix
Circular Hough transform: edge/gradient map, accumulate votes in
(x, y, r) space (phase-coding or two-stage radius estimation per MATLAB's
'PhaseCode'/'TwoStage' methods), find peaks → centers + radii + metric.
Larger; needs the gradient-based accumulator + peak detection. Outputs:
`[centers, radii, metric]`, sorted by descending metric. Validate against
MATLAB on a synthetic image with known circles.

## References
- new file under `src/toolboxes/image/src/...`
- shipped: `imgradient`, `edge`, `hough` (line Hough — different accumulator)
- MATLAB `doc imfindcircles`
