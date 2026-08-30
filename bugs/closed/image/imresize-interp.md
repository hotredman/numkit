# image.imresize — 'bilinear'/'bicubic' diverge from MATLAB (grid + boundary + antialias)

- **Status:** ✅ FIXED (2026-06-18) — pixel-centre map + mirror boundary + antialiasing
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

## Fix (2026-06-18)
Rewrote the 2-D `imresize` interpolation path to MATLAB's separable algorithm
(reconstructed + validated against MATLAB before porting — the same machinery the
already-correct `imresize3` uses, kept self-contained in geom.cpp's first anon
namespace so the 3-D path is untouched):
1. **Pixel-centre map:** output pixel `o` (1-based) ← input `u = o/scale +
   0.5·(1 − 1/scale)`.
2. **Mirror boundary:** out-of-range kernel taps reflect across the edge
   (`rkMirror`), and taps folding onto the same input sample sum — NOT
   zero-pad/clamp. (Clamp gave bicubic corner 0.789; mirror gives MATLAB's
   0.71875.)
3. **Antialiasing on shrink:** when `scale < 1` the kernel is stretched
   (`scl = scale`, width `/= scale`) and renormalised — `h_aa(t) = scale·h(scale·t)`.
4. Kernels: triangle (`bilinear`), Keys a=−0.5 cubic (`bicubic`); **default
   method is now `bicubic`** (`imresize_reg` was defaulting to `bilinear`; MATLAB
   defaults to bicubic — this was the downscale-repro mismatch). `nearest`
   unchanged. Scale form uses the scalar scale in the map; size form uses
   `outLen/inLen`.

Verified vs MATLAB R2025b (parity `imresize.json` → OK; full matrices):
bilinear x2 `[[1 1.25 1.75 2];…[3 3.25 3.75 4]]`; bicubic x2 (1,1)=0.71875;
downscale `[1 2 3 4 5 6]→[1 3]` = `[1.44922 3.5 5.55078]` (bicubic + antialias).
Guard: `known_bugs_test.cpp` (`ImresizeBilinear`, promoted live); smoke
`imresize_interp_smoke.m`.

## References
- `src/toolboxes/image/src/geom/geom.cpp` (`rkBuild`/`rkMirror`/`imresizeKernel`
  + the two `imresize` overloads),
  `src/bundle/src/register/image/geom/geom_reg.cpp` (default method → bicubic).
- `tools/parity/specs/imresize.json`.
- MATLAB `doc imresize`
