# glplot — WebGL data renderer for the Plot viewer

A **hybrid** renderer for the hardest cases (millions of points: line /
scatter / polar / …). WebGL2 draws the heavy data layers; SVG keeps
everything light and declarative (axes, grid, ticks, legend, crosshair,
toolbar, context menus) — and small data layers stay on SVG too.

Pan / zoom on the WebGL layer is a uniform (projection-matrix) update →
**O(1) per frame** regardless of point count, algorithm, or zoom.

## Layering (clean separation of pure logic vs the GL layer)

| File | Pure? | Tested |
|---|---|---|
| `projection.js` | yes — data→clip-space math (linear/log/reversed) + inverse | unit |
| `pack.js`       | yes — series → interleaved Float32 VBO + gap segments | unit |
| `route.js`      | yes — GL gate (flag/interactive/webgl2) + which layers route | unit |
| `transforms.js` | yes — polar (θ,ρ)→(x,y) etc. (later) | unit |
| `glcontext.js`  | thin — compile shader / program / buffer helpers | integration |
| `GLPlotRenderer.js` | imperative WebGL2 — context, VBOs, draw loop | integration / live |
| `GLChart.jsx`   | React wrapper — canvas overlay, init/setSeries/draw effects | render-smoke / live |

The pure modules carry the testable logic; the GL layer is a thin,
isolated imperative wrapper over them (verified live, not in jsdom — jsdom
has no WebGL).

## Two data paths into the VBO
- **< 1M points** — the full series already lives in JS (`layer.x/.y`);
  `selectGLSeries` → `packXY` → VBO.
- **> 1M points (GPU LOD)** — the engine downsamples for the inline preview and
  keeps the raw signal engine-side. `selectGLBigSeries` flags those layers; the
  chart draws an engine-decimated viewport tile (~4·W points via
  `engine.getSeriesTile`) → `packXY` → VBO. The tile covers a few× the
  viewport, so panning within it is a pure projection-uniform update (O(1)); it
  refetches (debounced) only when the viewport leaves coverage or the zoom
  changes ≥2×. Bounded points at any zoom → no overdraw at full zoom-out, a
  tiny VBO at any N.

GL is **interactive-window only** — non-interactive preview cards keep the
SVG decimation path (WebGL contexts are scarce, ~16/page; a dense raw line
also reads as a solid fill at thumbnail size).

## Coordinate-agnostic
The renderer takes plain (x, y). Polar feeds (θ,ρ)→(x,y) first; log applies
in the projection. So one line / scatter program serves cartesian, polar
and log without special cases.
