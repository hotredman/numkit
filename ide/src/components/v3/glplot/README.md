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
| `pack.js`       | yes — series → interleaved Float32 buffer + gap segments | unit |
| `transforms.js` | yes — polar (θ,ρ)→(x,y) etc. (later) | unit |
| `glcontext.js`  | thin — compile shader / program / buffer helpers (later) | integration |
| `GLPlotRenderer.js` | imperative WebGL2 — context, VBOs, draw loop (later) | integration / live |
| `shaders/*`     | GLSL strings (line / scatter programs) (later) | live |

The pure modules carry the testable logic; the GL layer is a thin,
isolated imperative wrapper over them (verified live, not in jsdom — jsdom
has no WebGL). No hacks: data crosses from the engine as binary Float32
(the `getFigureDisplayTile` typed-memory-view pattern), straight into VBOs.

## Coordinate-agnostic
The renderer takes plain (x, y). Polar feeds (θ,ρ)→(x,y) first; log applies
in the projection. So one line / scatter program serves cartesian, polar
and log without special cases.
