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
  `gcf.Number` dotted-rvalue VM/TW divergence
  (bugs/opened/lang/dotted-rvalue-on-call-result.md).

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
