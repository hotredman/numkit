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

## Portion 36 — compat.* retired for good (rule + mechanical guard)

The compat namespace (duplicate flat-registration layer for pre-resolver
access) is now EMPTY everywhere outside two core namespace-machinery
tests. Removed: 10 graphics registrations (portions 29-35 had
re-introduced the old toolbox dual-registration pattern — the bare ""
registration and the bare-name resolver already covered flat access),
the dual_engine_fixture implicit `import compat.*` (touched every dual
test), 14 toolbox/core fixture imports, stale hub comments. Kept as
NO-OPS for old scripts: the `--compat` CLI flag and the wasm
`repl_set_compat_mode` bridge (the IDE still calls the latter from its
settings — a future IDE portion may drop the toggle).

ENFORCEMENT so it cannot return: bugs_audit.py R6 fails the audit on
any `registerFunction("compat", ...)` outside the allowlist (verified
to fire on an injected probe). AGENTS.md carries the user rule
(2026-09-12) under "Public API conventions": register the namespaced
name + (only where needed) the bare "" name; never compat.

GOTCHA for batch edits: deleting "import compat.*;" from multi-line
concatenated C++ eval literals with a regex ate the SETUP line next to
it in 3 image test fixtures (suite caught it: 20 failures, "Undefined
variable 'A'") — mechanical edits over string-concatenation literals
need a diff review of every hunk, not just a green compile.

## Portion 37 — compat legacy fully excised (follow-up to 36)

Portion 36 left tolerated no-ops; this pass removed even those: the
`--compat` CLI flag (parse branch + compatMode threading + usage line —
an old script passing it now hits the unknown-flag fallthrough and is
ignored), the wasm `repl_set_compat_mode` bridge + its emscripten
registration, the IDE "MATLAB Compatibility Mode" preferences toggle +
settings default + engine.js calls, and the whole core implicit-import
machinery (`addImplicitImport` / `clearImplicitImports` /
`implicitImports_` / `restoreImplicitImports` + its 7 call sites across
workspace.cpp clear-paths and debug_session.cpp). The wasm
ReplSession ctor/reset still had TWO live addImplicitImport(compat)
sites portion 36 missed — the wasm engine was still implicitly
importing compat on every session. The fake `compat` namespaces in
core tests became `probe_ns`, so bugs_audit R6 now runs with NO
allowlist. `import compat.*` still parses for old scripts (import
statement machinery is intact); only the implicit layer is gone.

LESSON (again, stronger): the batch-regex that deleted
`restoreImplicitImports(...)` call lines didn't cover the `->` receiver
form, leaving `ctx.engine->` glued to the following line — caught by
the compile, but a review of every hunk (not just green build of the
files you edited FIRST) is mandatory for mechanical edits.

## Portion 38 — colorbar + text() export (probe table)

Probes (MATLAB R2025b, 2026-09-12): colorbar limits = CLim exactly;
VERTICAL tick ladder = the 10-interval rule (12 ticks on [1 12]) but a
HORIZONTAL strip uses the 5-interval ladder (6 ticks) — orientation-
dependent density, now matched in BOTH the export renderer and the IDE
(the IDE used target 5 everywhere). Colorbar FontSize 9. text defaults
DEFY memory: HorizontalAlignment LEFT (not center), VerticalAlignment
middle, FontSize 10 which does NOT follow axes FontSize. TEXT COLOURS
ARE MATLAB-EXACT ('r' = (255,0,0)) — do NOT reuse the soft series
palette (that is a deliberate numkit line aesthetic only).

Engine bugs fixed en route: (1) positional colorbar('southoutside')
was silently DROPPED — the parser only handled 'Location',<v> pairs
and pre-defaulted to 'east' (also affected the IDE); (2) text(..,
'Color',[r g b]) threw "Not a char array" (argStr=toString on a
numeric triplet — the recurring Value trap) — numeric 1x3 now encodes
as "rgb=r,g,b" extras. text() extras wire format grew halign=left|
center|right.

Export geometry: colorbar reserves 48px margin on its side; the strip
is 14px thick rendered as a Cmd::IMAGE rgbDirect gradient (reuses the
portion-35 PNG/SVG image plumbing); ticks + tickLabel() at FontSize 9;
text anchors: raster top-left of the glyph box with middle alignment
lifted by 3.5*scale, halign shifts by 6*scale/char. Known backend
mismatch (pre-existing): SVG <text> y is a baseline while the raster
treats it as the glyph-box top — tick/annotation labels sit lower in
SVG than PNG; filed with the export gaps.

## Portion 39 — contour/contourf/histogram export + the dark-theme trap

Export now renders contour (null-separated level segments SPLIT into
separate polylines — the old line path dropped the nulls and drew
spurious chords between unrelated segments), contourf (polygon rings,
null-separated, filled) and histogram (bars TOUCH: full bin width,
FaceColor 'auto'=(0,114,189), EdgeColor (217)x3, LW 0.5). histogram()
was MISSING as a function entirely (only in the handle-types table);
implemented with MATLAB-probe-verified auto binning: all-integer data
→ width-1 bins at x±0.5; else Scott 3.5·std·n^(−1/3) FLOORED to one
significant digit, edges floor(min/w)·w..ceil(max/w)·w (probes:
rand(1,100)→w 0.2/5 bins; randn(1,1000)→w 0.3/23 bins/−3.3..3.6;
integer samples → exact MATLAB Values vectors).

THEME/ALPHA TRAP (2026-09-12) — REFINED DIAGNOSIS: histogram default
FaceAlpha is 0.6 ALWAYS (probe: h.FaceAlpha==0.6; NOT a theme effect).
The "dark-tinted pinned colours" were 0.6-alpha bars compositing with
the figure background, which flipped white→(18,18,18) when the OS
theme turned dark (groot defaults stay light; only the export canvas
is dark). The DETERMINISTIC RECIPE for exact-colour parity: pin
FaceColor/EdgeColor AND FaceAlpha=1 — renders identical under any
theme (77525 pure (0,114,189) pixels verified on dark). numkit now
implements MATLAB-parity alpha semantics itself: default bars
pre-blend 0.6·face+0.4·white over the (white) canvas, FaceAlpha NV
honoured. Standing policy: numkit ALWAYS renders the documented light
defaults (the books contract) — user themes are not chased. If MATLAB
pixel references suddenly "MISMATCH" in the future, CHECK THEME +
ALPHA FIRST: alpha-composited defaults shift with the background,
colormap/line/text colours do not (all other graphics parity specs
passed unchanged against the dark MATLAB).

## Portion 40 — 3-D surf/mesh/waterfall export

Probes (MATLAB R2025b): surf FaceColor 'flat' — each quad takes the
palette colour of its FIRST corner's Z (perfect for a flat raster, no
interpolation needed), edges (0.129)x3 gray; mesh FaceColor 'none' but
EdgeColor 'flat' — individual LINE SEGMENTS coloured by Z through the
colormap; waterfall rows by Z; clim auto = [min Z, max Z] (same rule
as imagesc); default view (-37.5, 30).

Projection (empirically pinned by the CORNER-BLOB TEST — do not
derive from memory): a single hot corner Z(1,1)=1 rendered at
view(0,90) must land LEFT-BOTTOM of the content in MATLAB. Correct
transform (screen y up): sx = -x·sin(az) + y·cos(az); sy =
(x·cos(az)+y·sin(az))·sin(el) + z·cos(el); depth toward camera =
z·sin(el) − cos(el)·(x·cos(az)+y·sin(az)). The sign of the xy term in
sy was the bug the corner test caught (a top view looked vertically
mirrored). The parity spec pins this orientation forever via
blob-vs-chromatic-bbox normalization (chromatic bbox excludes white
AND gray → theme-independent under the dark OS theme).

Engine: mesh datasets split from type "plot3" to type "mesh" (the
export colours their segments by Z like MATLAB; the IDE adapter maps
mesh → its plot3 path, keeping the steel-blue look — engine/export
MATLAB-parity vs IDE aesthetic is the same split as the line palette).

GOTCHA: the nested-z parser must track bracket DEPTH — counting every
']' as a row end parsed 3 rows from a 2x2 matrix and read past the
vector (symptom: quads with garbage/degenerate geometry; found via the
SVG display-list debugger). Deferred: shading interp, 3-D box tick
labels, mesh hidden-line removal, lighting, fill3 vertex colours.
