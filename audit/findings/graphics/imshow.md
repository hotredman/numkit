# imshow — feature gaps

Status: open. Function shipped 2026-05-10 covering grayscale (uint8 /
double / single / logical, with `[lo hi]` / `[]` overrides) and RGB
M×N×3 (uint8 / double, with `*255` cast for floats). The following
MATLAB-documented forms remain unimplemented; pick them up as the
need surfaces.

## 1. `imshow(filename)` — file path input

MATLAB:
```matlab
imshow('peppers.png');
imshow('image.tif');
```
Need: PNG / JPEG / TIFF decoder. Out-of-scope for v1 because:
- emscripten's libpng / libjpeg are sizeable (>500 KB extra WASM)
- file I/O contract differs between desktop (real fs) and WASM (Mem-FS)
- no other graphics builtin currently reads files

When picked up: probably gate on `numkit::FileIO::available()` and
defer to the host's existing image-decode path (Electron has built-in
`electron.nativeImage` for desktop; WASM may want stb_image).

## 2. Name-Value parameters

Currently NOT parsed (lost silently if user passes them):

- `'DisplayRange', [lo hi]` — equivalent to positional `imshow(I, [lo hi])`,
  trivially implementable
- `'Colormap', cmap` — sets axes colormap; today user has to call
  `colormap(...)` separately
- `'XData', xv` / `'YData', yv` — span the image over `[xv(1), xv(end)]`
  rather than `[1, nC]` / `[1, nR]`
- `'InitialMagnification', factor | 'fit'` — controls display scale; we
  always fit-to-panel which matches `'fit'`
- `'Border', 'tight' | 'loose'` — controls axes margins; partly
  redundant with `axis image`
- `'Reduce', true | false` — auto-downsample for huge images. We
  already mean-pool when > 2M pixels; this option would be the
  user-facing toggle

Parser pattern: same as `plot`'s `parsePlotArgs` (key/value pairs
after the data args). Look at `libs/graphics/src/library.cpp:100`
for the template.

## 3. RGBA (4-channel) input

MATLAB accepts M×N×4 with the 4th channel as alpha. We currently
require `dims[2] == 3`. Easy extension: when `dims[2] == 4`, copy
the 4th channel into `imgData.data[p+3]` instead of forcing 255. The
RGB packer in `imshowImpl` already iterates `k = 0..2`; widen to
`k = 0..3` and store a flag on `DatasetInfo` so the renderer knows
to use the per-pixel alpha.

## 4. `imshow(I, RI)` — spatial-referencing object

`imref2d(...)` / `imref3d(...)` from the Image Processing Toolbox.
Probably last on the list — niche, and represents 5 lines of MATLAB
that scripts almost never use.
