# IDE backlog — post-B0 roadmap

B0 (unified `CompositePlot` walking `figure.layers[]`) is in main as of
2026-05-06. The list below tracks what's needed to reach real
MATLAB-parity coverage. Each phase builds on the previous one — pick
a wave to ship, don't cherry-pick across waves.

## B1 — overlay relatives (1–2 sessions)

Reuses the existing `layers[]` infrastructure. Each item adds a new
layer kind or a new mode under `kind: 'series'`.

- [ ] `errorbar(x, y, e)` / `errorbar(x, y, neg, pos)` — series-mode
      `'errorbar'`; renders cap-bars on top of a line/marker series.
- [ ] `area(x, y)` — series-mode `'area'`; fill below the curve to a
      configurable baseline (default 0). Works under log axes too.
- [ ] `barh(y, w)` — horizontal-bar mode; mirrors existing `bar` but
      sx/sy roles swapped.
- [ ] `plot3(x, y, z)` / `scatter3` — 2-D projection only at first
      (cabinet projection or just XZ / XY toggle). Full 3-D camera is
      B3 territory.
- [ ] `quiver(x, y, u, v)` — vector field arrows on a grid; scale to
      cell width by default with `'AutoScale'` option.
- [ ] `pcolor(x, y, Z)` — almost `imagesc` but cell *vertices* live at
      the (x, y) coords, with optional `shading interp`. Renders as a
      heatmap-layer variant or a separate `kind: 'pcolor'`.

## B2 — real plot infrastructure (2–4 sessions)

Cross-cutting features that change how every panel composes itself.

- [ ] `axis equal` / `axis square` / `axis tight` — aspect-ratio
      locking on the SVG's sx/sy. Needs viewport projection rework
      (currently sx and sy are independent).
- [ ] `yyaxis left` / `yyaxis right` — dual y-axis with independent
      scales. Renderer needs a parallel sy_right with its own ticks +
      label, and series gets a `'yyaxis'` discriminator.
- [ ] `linkaxes([ax1 ax2], 'xy')` — synchronous pan/zoom across
      subplot cells. SubplotGrid currently keeps per-cell viewports;
      linkaxes broadcasts viewport changes through the linked group.
- [ ] `colorbar('south')` / `colorbar('Location', ...)` — render the
      colorbar in any of {north, south, east, west, eastoutside, …}
      instead of the hardcoded right-of-axes slot.
- [ ] `legend('Location', 'northwest')` + `legend('boxoff')` /
      `'boxon'` — currently the legend is fixed at top-right of the
      figure window. Needs anchor-aware positioning and a frame
      toggle.
- [ ] `set(gca, 'XDir', 'reverse')` / `'YDir', 'reverse'` — flip axis
      directions without flipping data.

## B3 — advanced (only if it earns its keep)

Each item is a several-session investment. Skip unless we have a real
script that needs it.

- [ ] `contour(X, Y, Z)` / `contourf` — marching-squares iso-lines.
      Output as a series of `kind: 'series'` polylines (one per
      contour level) with cmap-derived colours.
- [ ] `surf(X, Y, Z)` / `mesh` — 3-D rendering. Needs a real camera
      (orbit + dolly), normal-based shading, hidden-surface removal.
      This is the line where SVG stops being enough — would need a
      Three.js or WebGL canvas.
- [ ] `histogram2(x, y)` — 2-D histogram, count grid rendered as a
      heatmap layer with the histogram-counts as `Z`.
- [ ] `streamslice` / `streamline` — vector field integration.
      Builds on `quiver`.

## Notes

- Polar plots stay on their own `kind: 'polar'` renderer. Nothing in
  B1/B2/B3 is polar-specific (would be a separate B4 wave if needed).
- Subplot grid (`kind: 'subplot'`) is composition only — when its
  cells gain B1/B2 features, they inherit automatically.
- Bug fixes happen on `fix/ide-bugs` branches per cycle. This backlog
  is for net-new features only.
