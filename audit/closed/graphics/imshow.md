# imshow — feature gaps

Status: ✅ CLOSED (2026-06-06) — resolved to the headless compute
runtime's scope; the remaining forms are display-only or graphics-object
forms that numkit does not model (see the **Closed** block at the bottom).
Function shipped 2026-05-10 covering grayscale (uint8 /
double / single / logical, with `[lo hi]` / `[]` overrides) and RGB
M×N×3 (uint8 / double, with `*255` cast for floats). The following
MATLAB-documented forms remain unimplemented; pick them up as the
need surfaces.

## 1. `imshow(filename)` — file path input ✅ Implemented (2026-05-10)

```matlab
imshow('peppers.png');
imshow('image.tif');     % stb_image handles only the formats below
```

Routes through `numkit::image::imread` (libs/image), which in turn
uses the vendored stb_image (third_party/stb). Supported formats:
PNG, JPG/JPEG, BMP, TGA, GIF (decode), HDR/PIC, PNM, PSD. TIFF is
**NOT** supported by stb_image — those calls will throw at decode.

Path resolution follows whatever the engine's host filesystem
exposes. In WASM the path lives in Emscripten's Mem-FS (the IDE
mounts `tempFS` / `localFS` there); in the native build it's a real
disk path.

## 2. Name-Value parameters

### Implemented (2026-05-10)

- `'DisplayRange', [lo hi]` — equivalent to positional `imshow(I, [lo hi])`. ✅
- `'XData', xv` / `'YData', yv` — image spans `[xv(1), xv(end)]` /
  `[yv(1), yv(end)]` instead of pixel-index baseline. ✅
- `'Colormap', name` — sets `ax.colormapName` directly. Wins over
  the default 'gray'. ✅

### Accepted but currently no-op (2026-05-10)

These keys are now parsed without error so user scripts don't
break, but the visual effect is BACKLOG:

- `'InitialMagnification', factor | 'fit'` — controls display scale.
  We always fit-to-panel which matches `'fit'`. The numeric form
  would set SVG image dimensions to `factor * pixel_count`; needs
  IDE-side viewport plumbing.
- `'Border', 'tight' | 'loose'` — controls axes margins; partly
  redundant with `axis image` (which we already apply).
- `'Reduce', true | false` — auto-downsample for huge images. We
  already mean-pool when > 2M pixels; this would be the user-facing
  toggle. Currently the cap is hard-coded.
- `'Parent', handle` — graphics handles aren't modelled.

Parser pattern: see `libs/graphics/src/library.cpp::imshowImpl`
N-V loop at the top (DisplayRange/XData/YData/Colormap implemented;
the four above match-and-ignore).

## 3. RGBA (4-channel) input ✅ Implemented (2026-05-10)

`imshow(M×N×4)` — 4th page is alpha. Wire format always carries
4 bytes per pixel (`[r, g, b, a]`). RGB inputs synthesize alpha=255
so the renderer is uniform. Float / logical inputs follow the same
*255 cast as RGB.

## 4. `imshow(I, RI)` — spatial-referencing object

`imref2d(...)` / `imref3d(...)` from the Image Processing Toolbox.
Probably last on the list — niche, and represents 5 lines of MATLAB
that scripts almost never use.

## Closed
- Closed in commit: pending (audit cleanup)
- Closed date: 2026-06-06
- Notes: imshow's headless-relevant forms are implemented and emit figure
  data consumed by the IDE/REPL — grayscale (uint8/double/single/logical with
  `[lo hi]`/`[]` overrides), RGB M×N×3, RGBA M×N×4, filename input (via
  `image::imread` + vendored stb_image), and the DisplayRange/XData/YData/
  Colormap name-values. The remaining MATLAB forms are **out of the compute
  runtime's scope**, not a compute defect: `InitialMagnification` (numeric),
  `Border`, and the `Reduce` toggle are display/IDE-viewport-only (numkit
  always fit-to-panel + auto mean-pools >2M px); `Parent` handle and
  `imshow(I, RI)` (imref2d/imref3d) are graphics-handle / spatial-referencing
  OBJECTS, which numkit does not model (same stance as the closed `plot.md`).
  Tracked as graphics backlog; no headless-actionable work remains.
