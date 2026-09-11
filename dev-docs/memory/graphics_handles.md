# graphics handles — architecture (portion 29, 2026-09-07)

## Problem

MATLAB graphics is handle-object based: `h = plot(...)` returns an object
with `class(h)`, `h.LineWidth`, `set/get`, identity `==`, and `delete`.
numkit's plot family returned nothing; the first accessor attempt
(portion 28) crashed EVERY engine construction with
`duplicate function registration: delete` because two TUs registered the
core name `delete` (graphics' handle-delete vs general_reg's file-delete).

## Solution — four cooperating layers

1. **Payload (`src/graphics/include/numkit/graphics/graphics_handle.hpp`)**
   — `GraphicsHandlePayload : NativePayload` carrying just the registry
   id + `graphicsHandleIdOf(Value)`. Lives in the GRAPHICS layer
   (value-layer only, no Engine) so three consumers share it without
   layering knots: core-free plot bodies (`close(f)`), the bundle
   accessor hub, and the builtin layer's unified delete.
   Probe is PAYLOAD-based, never class-name-based — `class(h)` reports
   the MATLAB class (`matlab.graphics.chart.primitive.Line`).

2. **Registry (`FigureManager`, figure layer)** — `HandleRecord
   {type, className, figureId, axesIndex, datasetIndex, props, deleted}`;
   chart records point at `axes[axesIndex].datasets[datasetIndex]`,
   axes/figure records use `datasetIndex = kNoDataset`. Invalidation is
   KIND-AWARE (`invalidateHandles(figId, axesIdx, includeAxes,
   includeFigure)`): close kills the whole tree, clf keeps the figure
   record, prepareForPlot (hold off) kills ONLY that axes' charts —
   getting this wrong resurrects the "figure 1 pre-exists" numbering bug
   (an unconditional `currentAxes()` in the adapter materialises figure 1
   before `figure`'s body runs, so the first figure numbers itself 2).

3. **Accessor hub (`src/bundle/src/register/graphics/graphics_library.cpp`)**
   — the only Engine-coupled graphics TU: adapter post-hook binds handles
   (chart calls snapshot the base via `fm.beginChartCall()` so a
   prepareForPlot wipe mid-call cannot zero the new-dataset count);
   handle-aware set/get/isgraphics/ishandle/isvalid registered under
   `""`+`compat` with the table's noop entries skipped; one BuiltinClass
   per MATLAB class name routes dot-syntax through propGet/propSet and
   `==`/`~=` through `ops["eq"|"ne"]` — NOTE the ops convention:
   `args` carries (lhs, rhs); `self` is a copy of the dominant operand,
   NOT an extra argument (comparing args[0] vs self compares the operand
   with itself and returns always-true).

4. **Unified delete (`general_reg.cpp`)** — ONE core name, dispatched by
   argument type exactly like MATLAB: handle → registry (figure →
   closeFigureNotify, axes → removeAxes, chart → deleteHandle+reindex);
   char/string → file unlink.

## Property flow

`rec->props` (string map, %.17g for exact stod round-trip — std::to_string
TRUNCATES to 6 digits and corrupts user values) is the source of truth for
user-set values; `get` falls back to MATLAB-documented defaults (LineWidth
0.5, MarkerSize 6, Color [0 0.447 0.741]…). Render-effect writes the live
`DatasetInfo`: LineWidth/MarkerSize direct fields, LineStyle/Marker/Color
rebuild `ds.style` into the kv dialect (`color=#hex;lineStyle=--;marker=o`)
which the renderer's `parseLineSpec` understands, `Visible` → `ds.visible`
(emitted as `"visible":false`), XData/YData replace xJson/yJson (dropping
the downsample state and re-deciding). Axes props map to the AxesState
fields (xlimJson/labels/Title/scales). Every mutation marks the figure
modified and emits.

## Results (verified)

- dual-engine suite `TW_VM/GraphicsHandleTest.*` 38 tests green (TW+VM);
- parity `tools/parity/specs/graphics_handle.json` correctness=OK vs
  MATLAB R2025b (defaults, set/get, YData, isgraphics/ishandle/isa);
- smoke `src/graphics/tests/smoke/graphics_handle_smoke.m` all-expect;
- found+fixed en route: `plot(x,y,x2,y2)` dropped all series after the
  first (bugs/closed/graphics/plot-multi-series-dropped.md); found+filed:
  `gcf.Number` dotted-rvalue VM/TW divergence — FIXED in portion 30
  (compileFieldAccess mirrors TW: unregistered dotted name + callable
  head → 0-arg CALL of the head + FIELD_GET chain;
  bugs/closed/lang/dotted-rvalue-on-call-result.md).

## Gotchas for the next session

- `Value::toString()` THROWS "Not a char array" on numerics — any property
  encoder must format doubles itself.
- Statement calls (`plot(1:5);`) bind the result into `ans` — the post-hook
  writes `outs[0]` unconditionally; both engines always pass a ≥1-slot
  outs span for single-output calls, but CALL_MULTI with explicit [a,b]
  destructure sizes outs by nout, so "Too many output arguments" emerges
  naturally for `[a,b] = plot(...)`.
- Deferred surface tracked in
  bugs/opened/graphics/handle-surface-gaps.md (title/legend handles, root,
  Children arrays, chart-class construction, ZData, set-listing,
  axes/figure prop breadth, Text-handle labels).


## Portion 31 addendum — Text/Legend/Root + Children (2026-09-11)

Slot-bound record kinds extend the registry: `type="text"` carries
`slot` ∈ {title,subtitle,sgtitle,xlabel,ylabel,zlabel} with LIVE String
(the AxesState field is the source of truth — ylabel routes to ylabel2
under yyaxis-right); `type="legend"` reads Location/Box/Visible from the
LIVE axes fields BEFORE rec->props (a stored prop would shadow a later
`legend show` command — caught by the smoke); `type="root"` is ONE
global record (figureId 0 + kNoAxes: never matches a real figure, so no
sweep invalidates it). recordGetProp/recordSetProp are now the SINGLE
body shared by get()/set() and the BuiltinClass propGet/propSet hooks
(strict flag: dot-syntax errors on unknown props, external get stays
lenient "").

**MATLAB facts learned by probing R2025b directly (assumptions were
wrong twice):** class(groot) is `matlab.ui.Root` (NOT matlab.Root), it
has no Number; legend is `matlab.graphics.illustration.Legend` (NOT
chart.legend.Legend); Text FontSize default is 11 (NOT 10). Probe, don't
recall. Also: MATLAB FORBIDS dot-indexing a call result without parens
(`get(gca,'T').String` is a parse error; legal form `gcf().Parent`) —
numkit deliberately keeps evaluating the bare `gcf.Parent` form
(permissive superset, TW heritage; documented in the parity spec
comment).

setSubplot now ADOPTS the lone default axes instead of ghosting it
(bugs/closed/graphics/subplot-replaces-default-axes.md — the ghost
appeared even for subplot-first scripts, because setSubplot itself
materialises the default axes). `legend off/show/hide/on` are
visibility-only (labels+location preserved) — the old pre-scan CLEARED
labels on `legend show`.

The INDEXED dotted chain `gca.Parent.Children(2)` needed the portion-30
fix extended into the CALL paths: compileCall's qualified branches
(single+multi) fall through to CALL_METHOD (property-then-index) when
nothing dotted is registered but the head is a plain callable; TW's
execCall mirrors via execIndexAccess.


## Portion 32 addendum — engine-side figure export (2026-09-11)

saveas/exportgraphics/print now render FIGURES IN-ENGINE: a pixel-space
display list built from FigureState (figrender in figure_render.hpp —
STL-only, header-only like the rest of the figure layer) with two
backends: SVG (text as <text>) and a raster RGB buffer (Bresenham +
scanline fill + the classic public-domain 5x7 bitmap font). The bundle
export_reg.cpp owns the MATLAB argument forms and encodes PNG via the
image toolbox's imwriteToBytes.

GOTCHA that cost an hour: Value is COLUMN-MAJOR — filling matrix3d(H,W,3)
from a row-major RGB buffer linearly scrambles the image into diagonal
noise; transpose into d[r + c*H + ch*H*W]. Visual verification (render a
PNG, inspect it with the image analyzer) is now the acceptance gate for
renderer changes — the SVG backend doubles as a text-mode display-list
debugger (grep polyline points).

Found+filed en route: exist(absolutePath,'file') returns 0 INSIDE a
function but 2 at top level (vfsExists swallows a scope-dependent throw —
bugs/opened/runtime/exist-file-function-scope.md; parity specs running in
isolated local functions hit it, probe files with numel(dir(p)) instead).
Export surface gaps tracked in
bugs/opened/graphics/figure-export-gaps.md (pdf/eps, imagesc/3D/polar
rendering, axes-scoped exportgraphics).

## Portion 34 — axes property depth + the raster font lesson

Probe-first table (MATLAB R2025b, all measured not recalled) drove the
implementation: Text.FontSize reads back as axes FontSize × 1.1; setting
AXES FontSize/FontWeight/FontName RESETS even explicitly-set title/xlabel
fonts (they follow the axes until individually overridden); auto ticks =
10 intervals via the 1/2/5 ladder (breakpoints ≤1/≤2/≤5, step =
pow(10,floor(log10(range/10))) then ×2/×5/×10 — [0,10] gives 11 ticks);
CLim auto is [0 1]; axes LineWidth default 0.5. The same tick rule now
lives in three places that MUST stay in sync: figure_render.hpp
(niceTicks, engine export), ide/src/components/plot/plotTicks.js, and
this paragraph.

BUG WORTH REMEMBERING (bugs/closed/graphics/raster-font-table-shifted.md):
the 5x7 font table in figure_render.hpp had 92 of 95 rows — the array was
declared F[95][5] so C++ zero-filled the gap silently and every glyph
after it rendered +3 shifted ("time" → "wlph"). Fix: table replaced 1:1
with the canonical Adafruit glcdfont rows 0x20..0x7E (byte-verified
against the fetched reference, never hand-typed), array now unsized F[][5]
+ static_assert(count == 95) so a dropped row is a compile error. Guard:
FigureExportTest.RasterFontGlyphAlignment.

VERIFICATION LESSON: the image-analyzer MCP read a STALE cached CDN
artifact twice (same path → same signed URL → old bytes) and
"confirmed" the bug as still present after the fix; an ASCII decode of
the PNG pixels (zlib + unfilter, print #/. bitmap) proved the fix worked
within minutes. When a vision check contradicts mechanics, decode the
pixels — and for renderers, prefer the ASCII-decode probe as the primary
gate, the vision model as the secondary "does it look right" pass.

## Portion 35 — imagesc/imshow/image export + colormap ground truth

Engine-side export now renders image datasets (Cmd::IMAGE: quantized
indices + clim + palette, or imshow RGBA quads) in BOTH backends — the
raster blits a shared imageCmdRgb() buffer and the SVG embeds it as a
base64 PNG through an injectable SvgImageEmbedder (the figure layer
stays codec-free; export_reg supplies imwriteToBytes+base64). Geometry
follows MATLAB probes: image edges = centers ± spacing/2 (default
[1,C] → ±0.5), edge vectors as-is; auto axes limits pad by
span/(2·(cells−1)); image-only axes skip the 5% breathing room;
YDir reverse rides the existing transform; axis off drops decorations;
axis image/equal shrinks the plot rect to the data aspect.

COLORMAP GROUND TRUTH (probe, never port from memory): scaled CData v
maps to palette entry round(t·(N−1))+1 (1-based, N = colormap size) with
t=(v−clo)/(chi−clo) clamped; NaN renders as ENTRY 1 (the first color,
not white/transparent!). Named tables for the engine (figure_render.hpp
kNamedPaletteHex) and the IDE (colormapTables.js) are generated from the
SAME MATLAB R2025b dump feval(<name>,256) — byte-identical across
engine/IDE. MATLAB R2025b has NO viridis() (exist==0): the matplotlib-
approx stops are the shared ground truth there.

MATLAB QUIRK worth remembering: in this environment MATLAB's own
imshow+saveas renders an all-black image and exportgraphics gives a
7×6 black/white oddity (headless-render quirk, reproducible); the
parity spec therefore uses imagesc+colormap(gray)+axis ij off, which
MATLAB renders correctly and which exercises the same mapping chain.

Bugs found en route (all fixed same-portion except the last):
- base64 encoder: (char)0xFF << 16 sign-extends → every PNG byte ≥0x80
  corrupted (SVG images were silently broken from day one).
- parseCustomPalette: parseFlatDoubles stops at the first ']' (flat
  arrays) → nested colormap(M) JSON parsed ONE triplet → all cells
  entry-1. Strip brackets first.
- DatasetInfo comment lied: rgbBytes are RGBA QUADS (4 bytes/px,
  alpha synthesized), not RGB triplets — stride 4 in the renderer.
- numkit's 254-level imagesc quantization can round a value to the
  ADJACENT palette entry (half-integer cases, e.g. v=128 gray): pixel
  guards use ±3/channel, parity tol=1.
- OPEN bugs/opened/runtime/dir-nargout-in-argument.md: a call in
  ARGUMENT position inherits the outer call's nargout — statement
  context (fprintf(...)) makes dir print its listing and return
  nothing. Guard: DirNargoutInsideCallArgument (DISABLED_).
