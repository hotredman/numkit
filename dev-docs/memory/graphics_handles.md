# graphics handles — architecture (portion 29, 2026-09-07)

## Problem

MATLAB graphics is handle-object based: `h = plot(...)` returns an object
with `class(h)`, `h.LineWidth`, `set/get`, identity `==`, and `delete`.
nk's plot family returned nothing; the first accessor attempt
(portion 28) crashed EVERY engine construction with
`duplicate function registration: delete` because two TUs registered the
core name `delete` (graphics' handle-delete vs general_reg's file-delete).

## Solution — four cooperating layers

1. **Payload (`src/graphics/include/nk/graphics/graphics_handle.hpp`)**
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
nk deliberately keeps evaluating the bare `gcf.Parent` form
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
- nk's 254-level imagesc quantization can round a value to the
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
palette (that is a deliberate nk line aesthetic only).

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
theme (77525 pure (0,114,189) pixels verified on dark). nk now
implements MATLAB-parity alpha semantics itself: default bars
pre-blend 0.6·face+0.4·white over the (white) canvas, FaceAlpha NV
honoured. Standing policy: nk ALWAYS renders the documented light
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

## Portion 41 — findobj + quiver export + annotation

findobj (probes R2025b): no handle = ROOT scope across figures
(registry order); figure/axes handles scope to descendants; a CHART
handle scopes to ITSELF (MATLAB returns it with no children modelled);
'Color','r' filters compare NORMALIZED triplets ('r'==[1 0 0]) — the
chart get(h,'Color') now reads back the EXPLICIT linespec MATLAB-exact
(plot(x,y,'r') → [1 0 0]) instead of the documented default; the old
"introspection reports defaults until set" contract only covers the
palette-vs-MATLAB divergence, never explicit call intent. MATLAB HIDES
title/label texts from findobj (probe: findobj('Type','text')==0 next
to a title) — slot-bound Text records are excluded, user text() stays
findable.

annotation: normalized FIGURE coords; arrow/textarrow take (x, y)
2-vectors but ellipse/rectangle/textbox take ONE [x y w h] position
vector — the parser branches on numel==4; Name-Value keys are scanned
by CONTENT, not fixed i+=2 parity (the position vector shifts the
parity and silently drops every NV pair — the ellipse FaceColor
"missing fill" bug). Deferred: annotation handles, IDE rendering.

quiver: shaft + chevron head (~26°, max(4px, 12% of length)); tips
extend the auto data range. boxplot already rendered via portion-39
polygon/line/scatter coverage — verified, no work needed.

## Play portion A — manifest extractor + dependency graph

Format pinned from a REAL gallery script (user sample): the manifest
block is `%{ ... %}`; CANONICAL marker `@play` (user decision — the
spec's original guess was right), with `%{#ui` from the early prototype
kept as a LEGACY ALIAS until the gallery migrates. The engine extracts
the raw JSON + the control `var` names + defaults — nothing else; the
web layer parses the full JSON for UI. Marker must be a standalone
word (`@playx` is not a marker); `%{` only opens a block at line start
(lexer rule mirrored); the FIRST manifest wins.

Dependency semantics decisions (baked into dirtySetFor):
- A PURE DEFINITION of a parameter (writes var, never reads it) is
  SKIPPED by dirtySetFor — setParam REPLACES it; re-running would
  clobber the slider back to the script default. Readers + merge-
  writers still re-run.
- An `if` WITH its whole body is ONE top-level statement (statement
  indexing by AST children, not source lines — test-anchored).
- The real gallery sample's shape verified: change theta0 → only
  AF + polarplot re-run (theta vector cached); change win → the
  wildcard if + tail re-run.
- Some CALL forms carry the callee as an IDENTIFIER CHILD with empty
  strValue — the wildcard check (eval/feval/assignin/clear/load/run)
  must look at both (the EvalCallIsWildcard test caught it).

Portion B next: ReactiveSession over Engine (setParam + evalDirty via
runOneChunk) with the equivalence property evalDirty == full re-run.

## Play portion B — reactive session (B1 done, B2 green core, B3 partial)

B1 (index-aware facts, ALL IN + shipped): literal elem/range/stepped-
covering extraction, ALL for linear/end/var-index/logical, size/
numel/length/ndims args read ALL (merge-writes GROW the variable);
kill-vs-accumulate edge semantics (scalar redefinition kills — nearest
writer; merge-writes accumulate — whole-var readers depend on EVERY
earlier writer, region readers on the nearest intersecting writer);
merge-write LHS is NOT an rvalue read (lhsMergeBases) so cell
definitions stay clean on element edits.

PARSER FACT (test-pinned): paren access `C(...)` parses as CALL with
the head as an IDENTIFIER child — `C(7)` is AST-identical to `foo(3)`;
regions are recorded ONLY for unambiguous markers (CELLIDX braces,
END_VAL arg, COLON arg). Brace access is what the table editor uses.

B2: ReactiveSession — one semantics (evalStatement seam = rewrite +
runOneChunk, the engine's own per-statement path), native param writes
via a scratch carrier + real assignment statements, cycle guard
(forward-only edges → SCCs structurally empty), generation counter.
WILDCARD-PREFIX RULE (the big B2 lesson): a wildcard (for/while/if/…)
may ACCUMULATE on earlier state (`AF = AF+…` over a prior def) — its
re-run must pull the whole earlier prefix in, SKIPPING pure defs of
every externally-set param (else param sliders get clobbered by script
defaults). Without the rule equivalence fails on ANY loop-bearing
script (double accumulation).

B3 OPEN BUG (bugs/opened/core/reactive-multiparam-af-stale.md): after
a wildcard-tail tick, params + ph are CORRECT but the for-chunk's AF
write-back across consecutive evals diverges (mag sees the right new
AF, a later raw read sees a third value). Both equivalence tests are
DISABLED_ guards of that one bug; element-write equivalence, budget,
cycles, parse errors all green. Likely in the VM chunk-local register
sync for a re-executed loop statement — next session starts there
(vm_->lastVarMap / syncVMToWorkspace on re-run).

## Play portion B follow-up — the AF bug was a FALSE ALARM (closed)

Numeric re-analysis killed bugs/closed/core/reactive-multiparam-af-
stale.md: the equivalence tests' REFERENCE pre-pended params before
the script, whose own def lines then overwrote them — the "reference"
computed with defaults while the session was CORRECT (verified to 6
digits: session ph/mag match the closed-form values for the set
params; "full" matched the script defaults exactly). The AF-vs-mag
"inconsistency" was real-part (elemAsDouble on complex) vs magnitude
(abs) — different quantities, both right. LESSON (twice now): when an
equivalence test fails, verify the REFERENCE first — compute the
expected value in closed form / python before hunting engine ghosts.
Both tests re-enabled with param-def-line REPLACEMENT references; 6/6
green; the wildcard-prefix rule (double accumulation, real) stays.

## Play portion C — wasm reactive bindings

Ten `play*` functions on the nk_ide.js glue: playLoadScript (manifest
JSON out, {"__error":...} on load failure), playSetParamDouble /
String (selects) / Element (table editor), playEvalDirty, playHasDirty
/ LastError / Generation, playArrayViewInfo + playArrayView (zero-copy
typed_memory_view into the linear memory; offset in BYTES, complex
spans [real|imag]), playReset.

CHANNEL DESIGN (the C lesson): figure updates ride NO separate path —
playEvalDirty RETURNS the drained output buffer, which contains the
__FIGURE_DATA__ markers the dirty plot statements emitted through the
engine's normal output func. The web layer parses exactly the wire the
IDE consumes (outputBuf_ was already the shared sink; a drainOutput()
accessor was the only seam needed). This keeps one channel, one
semantics, zero duplication.

Smoke: ide/scripts/play-bindings.smoke.mjs — 11 checks (manifest,
plain script, dirty lifecycle, closed-form value, generation, view
shape + data, figure wire on tick, string select branch, element
write) — ALL GREEN against the freshly built glue. Node harness notes:
repl_execute RETURNS captured stdout (disp output is rounded — compare
with 1e-3, not 1e-9); a second Module instance is the clean way to
capture print output for wire assertions.

## Play portion D1 — play/ app skeleton (PURE SVELTE 5, no React)

User decision (2026-09-12): NO React islands — straight to Svelte. The
Svelte plot renderer is the SUCCESSOR of the IDE's React one: identical
engine wire JSON (the parity-verified contract), shared framework-free
helpers (@ide-plot alias → plotTicks.js / colormaps.js — they import
clean, zero React deps), chart types added portion by portion. The
React renderer stays in the IDE frozen until coverage, then flips.

play/ stack: Svelte 5 + @sveltejs/vite-plugin-svelte 4.x (NOTE: the
plugin's 5.x line peers on vite 6 — pinned 4.x to stay on the repo's
vite 5), pure vite.config with the @ide-plot alias. App.svelte = the
3-column layout (sidebar controls from the @play manifest /
editor / figures); PlotFigure.svelte renders the wire (D1 coverage:
2-D cartesian line series + POLAR — spokes/rings/steered beam), pulls
niceTicks from the shared helper. Gallery: build-time scan
(scripts/build-gallery.mjs) mirrors the C++ extractor's rules
(@play canonical, #ui alias, first wins) → gallery.generated.js.

The flagship gallery example — the user's REAL beamforming script
(phased-array-beamforming.m) — runs end-to-end: manifest → theta0
slider tick → beam peak MOVES to -30.0° exactly, chebwin select keeps
normalization = 1 (reactive-loop.smoke.mjs, 7/7 green).

En-route bug filed: exist('butter'/'hamming') returns 0 for registered
toolbox functions while direct calls work (core builtins fine —
sin→5); same opcode/external family as the portion-33 exist fix.
Guard DISABLED_ExistToolboxFunctionNoFilter.

## Play D1 follow-ups — lib/charts + the TypeScript migration

lib/plot renamed lib/charts with a sharper decomposition (user call):
STAGES (CartesianStage/PolarStage — the coordinate systems) vs CHARTS
(LineChart — stage-agnostic, takes any projection fn; the D1 series
duplication died). Pure model split stage.ts (viewport/projection/
polar) + series.ts (filter/style/paths).

TYPESCRIPT (user decision, same session): play/ is TS end-to-end,
svelte-check ZERO errors, rule R8 in the handbook. The crown:
wire.ts types the engine wire JSON (LineDataset/ImageDataset/
WireAxesConfig per portion-29–41 fields) and the @play manifest.
TS CAUGHT A REAL LATENT BUG immediately: D1 called the IDE's
niceTicks — which returns {major, minor} — as an array; polar
r-rings would have crashed at first render (the loop smoke never
renders, so it was green). Boundary rule: IDE JS imports get ONE
typed shim (ticks()) instead of call-site casts. Gotchas hit:
svelte-plugin 5.x peers vite 6 (pinned 4.x); svelte-check wants
TS 5.x not 7 (pinned ~5.7); @types/node for node: imports;
Snippet-typed children instead of slots in Svelte 5; a named
snippet {#snippet figures()} SHADOWS same-named state inside its
block (renamed state to wireFigures).

## Play D2 handoff state (2026-09-13, to flash session)

DONE (uncommitted or just committed by predecessor): scripts/
sync-wasm.mjs copies nk_ide.js/.wasm into play/public/ (wired into
dev/build scripts) — the browser boot path now resolves. NEXT (the
flash brief, in order): (1) HeatmapChart.svelte in lib/charts —
imagesc rendering; typed shim over the IDE's buildHeatmapLUT
(colormaps.js, pure JS) like ticks() did for niceTicks; clim from
dataset cminOrig/cmaxOrig or axes config climJson; the wire carries
z as nested row arrays + colormap name in config. (2) Two gallery
examples: damped-oscillation (plot, cartesian line) + 2D-gaussian
imagesc with a colormap select. (3) Build + loop smoke extension for
imagesc + commit. RULES the flash session MUST follow: handbook
svelte_architecture.md R1–R8 (no div/style on pages, one component
per file, typed wire contract, TS zero svelte-check errors); smoke
loop pattern in play/scripts/reactive-loop.smoke.mjs; commit style:
English subject+body + Co-Authored-By trailer; push ONLY origin.
HANDOFF CHECKPOINT: everything through D1+sync-wasm verified —
svelte-check 0/0, smokes 7/7+11/11.

## Play D2 — working prototype (imagesc renderer + 3-example gallery)

HeatmapChart.svelte + heatmap.ts: imagesc datasets render per-cell
rects through the ENGINE-SUPPLIED palette. Two wire palette paths,
both engine-truth: customColormap (N×3 [0,1] JSON — colormap(M) AND
named dumps serialize this) and colormap:"name" (resolved via the
IDE's MATLAB-dumped tables — namedPalette shim over getLUT/getColormap
@ide-plot). yDir reverse baked into cell order (row 0 top). Cell edges
from the wire's x/y edge vectors (± spacing/2). svelte-check 0/0.

GALLERY (3 examples, all driven end-to-end in reactive-loop.smoke
13/13): phased-array beamforming (steers to -30.0 exactly), damped
oscillation (zeta tick recompute matches closed form to 4 digits),
2D-gaussian heatmap (centre=1, sigma tick, colormap select).

ENGINE SEMANTICS pinned by the smoke hunt: colormap() alone does not
flip fig.modified — a palette-only change emits on the NEXT dirty
statement (the Svelte renderer reads cfg.customColormap/colormap off
any emission, so UI-wise the user sees it after the next tick —
documented UX, not a bug). The select-variable form (cm='hot';
colormap(cm)) does not make the variable's READERS dirty unless a
statement reads cm — a gallery tail `colormap(cm)` re-runs on tick
because cm IS its input. The smoke drives the switch via
repl_execute('colormap(hot);') — same engine path.

Wire emit detail: colormap:"custom" + customColormap for numeric/named
dumps both; the export renderer's bake and the IDE tables are the same
MATLAB dump (portion 35).

## Play gallery-67 batch (sections A–G, 7 section files, 67 numbered scripts)

Engine capability map (probed, not guessed): windows hamming/hann/
chebwin/taylorwin(NEGATIVE sll)/kaiser/blackmanharris work;
taylorwin sll must be NEGATIVE dB; butter/cheby1/cheby2/fir1/freqz/
filtfilt/filter/chirp/spectrogram/pwelch/hilbert(pow2 only — bug
filed)/fftshift/ode45/ode23/roots/polyval/besselj/interp1 all work;
tf objects: step/bode work but tf MULTIPLICATION and minreal are
broken (struct — PID/root-locus examples use explicit polynomial
math instead); MATLAB has no ternary (removed from section C);
colormap(name) works quoted, parula NOT callable as function (but
the renderer resolves named palettes from the IDE dump). exist() is
unreliable for toolbox functions (bug filed) — inventory by CALLING.

Gallery layout: section files section-a.m … section-g.m, each holding
its numbered scripts (1–67) — the @play-split per-script comes later;
for now each section is one gallery entry verified error-free through
the engine. Python heredoc mangles \n in template literals and
regex-with-escapes — use file-based patches or sed-safe constructs.
MATLAB scripts must not use ternaries or JS-style syntax.
