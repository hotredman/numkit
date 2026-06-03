# image.imresize — 'bilinear'/'bicubic' diverge from MATLAB (grid + boundary + antialias)

- **Status:** 🔴 OPEN
- **Severity:** P2 (wrong values for the non-nearest methods)
- **Kind:** bug
- **Found:** 2026-06-04 via DEEP-PROBE (image-method sweep)
- **Note:** this is the deferred imresize interpolation gap (memory `feedback`
  / project notes: "imresize bilinear+bicubic ANTIALIASING (large)").

## Symptom
`imresize` with `'bilinear'` or `'bicubic'` produces values that differ from
MATLAB, for both upscaling (boundary + pixel-center grid) and downscaling (no
antialiasing filter). `'nearest'` is correct.

## Repro
```matlab
imresize([1 2; 3 4], 2, 'bilinear')
% numkit: (1,1)=0.5625, (1,2)=0.9375, (4,4)=2.25
% MATLAB: (1,1)=1,      (1,2)=1.25,   (4,4)=4
imresize([1 2; 3 4], 2, 'bicubic')
% numkit (1,1)=0.5625   MATLAB (1,1)=0.71875

imresize([1 2 3 4 5 6], [1 3])           % downscale (default bicubic+antialias)
% numkit:  [1.5      3.5  5.5]
% MATLAB:  [1.44922  3.5  5.55078]
```

## Root cause
Two compounding differences from MATLAB's `imresize`:
1. **Boundary + grid (upscale):** numkit appears to zero-pad outside the image
   and use a different sample-grid origin, so corner/edge outputs are far off
   (`(1,1)=0.5625 = 9/16` ⇒ bilinear weights against a 0 neighbour). MATLAB
   maps output pixel *centres* back to input coordinates and clamps to the
   edge (replicate).
2. **Antialiasing (downscale):** MATLAB applies a low-pass kernel whose width
   scales with the reduction factor; numkit does plain interpolation, so the
   interior matches but the edges differ.

## Suggested fix
Adopt MATLAB's pixel-centre coordinate mapping + edge-replicate boundary for
the kernel taps, and the antialiasing kernel (triangle for bilinear, cubic
for bicubic, widened by the scale factor when shrinking). Large — this is the
known deferred imresize gap. Validate the full output matrix vs MATLAB on
up- and down-scaling.

## References
- `libs/image/src/.../imresize*`
- MATLAB `doc imresize`
