# IDE backlog — post-B0 roadmap

B0 (unified `CompositePlot` walking `figure.layers[]`) is in main as of
2026-05-06. The list below tracks what's needed to reach real
MATLAB-parity coverage. Each phase builds on the previous one — pick
a wave to ship, don't cherry-pick across waves.

## B1 — overlay relatives (DONE)

All six landed on `fix/ide-bugs` between 2026-05-09 and 2026-05-10.
Each function ships C++ builtin (`libs/graphics/src/library.cpp`) +
adapter wiring (`adapters.js`) + renderer mode
(`CompositePlot.jsx`) + Playwright e2e spec under
`ide/desktop/tests/e2e/b1-*.spec.js`.

- [x] `errorbar(x, y, e)` / `errorbar(x, y, neg, pos)` —
      series-mode `'errorbar'`; centre dot + vertical bar + caps.
      Symmetric and asymmetric forms both supported.
- [x] `area(x, y[, base])` — series-mode `'area'`; filled polygon
      from curve down to baseline (default 0). NaN points break the
      polygon into independent sub-paths.
- [x] `barh(y)` / `barh(x, y)` — horizontal-bar mode; mirrors `bar`
      with x/y roles swapped at adapter level so the X axis shows
      lengths and the Y axis shows positions.
- [x] `plot3(x, y, z)` / `scatter3(x, y, z)` — 2-D cabinet
      projection (30°, scale 0.5). Real 3-D camera (orbit / dolly)
      is B3 territory.
- [x] `quiver(x, y, u, v[, scale])` — vector field arrows. Each
      arrow = shaft + 2 head fins. Range scan extends to arrow
      tips so heads don't clip on autoscale.
- [x] `pcolor(C)` / `pcolor(X, Y, C)` — heatmap with cell
      *vertices* at (x, y) instead of cell centres. Shares the
      imagesc emit body via captured-lambda + `std::bind`; differs
      only in `type` field and the adapter's range-padding branch.

Total e2e suite: 40 passed.

While doing pcolor we found and fixed a stale `reg("surface",
"pcolor", noop)` that double-registered `compat.pcolor`. The
engine's `registerFunction` rejects duplicates and dropped the
renderer into fallback mode silently. The same audit pass should
run on every `noop` placeholder we promote to a real impl.

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
