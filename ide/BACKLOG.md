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

## B2 — real plot infrastructure (DONE)

All six landed across 2026-05-09 / 10. Cross-cutting features that
change how every panel composes itself.

- [x] `axis equal` / `axis square` / `axis tight` — aspect-ratio
      locking on the SVG's sx/sy via viewport extension on whichever
      axis has more screen space per data unit.
- [x] `yyaxis left` / `yyaxis right` — dual y-axis. Each dataset
      stamps its `yside` via `FigureManager::pushDataset`; renderer
      runs a parallel `sy2` mapping for right-side layers; modal
      shows the second Y axis on the right edge with its own ticks.
- [x] `linkaxes([], 'xy')` — synchronous pan/zoom across subplot
      cells. Mode stored on FigureState; SubplotGrid mirrors viewport
      changes across linked cells (skips polar cells whose viewport
      shape differs).
- [x] `colorbar('Location', ...)` — east / west / north / south
      placement (with `*outside` aliases). Two gradient orientations
      in defs; `'off'` marker disables the bar.
- [x] `legend('Location', ...)` — 9-position layout (north / south /
      east / west / 4 corners / best / none); per-series swatch
      shape matches mode (line / circle / rect for area+bar).
- [x] `set(gca, 'XDir'/'YDir', 'reverse')` plus the `xdir` / `ydir`
      direct setters and `axis('ij')` / `axis('xy')` shorthand. sx
      and sy fork on direction; flip composes with linear and log
      scales.

## B3 — advanced (DONE)

All four landed 2026-05-09 / 10 — `surf` and friends ride on a real
WebGL canvas.

- [x] `contour(X, Y, Z)` / `contourf` — marching squares per cell;
      each level emits its own type=line dataset with HSL→RGB color
      ramp. NaN-safe.
- [x] `surf(X, Y, Z)` / `mesh` — 3-D rendering through three.js
      (perspective + orbit + dolly + face shading + lighting). See
      §WebGL below — surf is the function that motivated the
      WebGL roll-out.
- [x] `histogram2(x, y[, n|[nx ny]])` — 2-D bin grid routed through
      the imagesc heatmap pipeline (per-pixel quantization + LUT).
- [x] `streamslice` / `streamline` — RK4 integration over the
      vector field; auto 5×5 seed grid for streamslice, explicit
      seeds for streamline.

## Tier 1 — wrapper builtins (DONE)

Quick wrappers over existing layer kinds; landed 2026-05-09 / 10 in
four batches (A: stem3 / compass / feather / spy ; B: stat charts ;
C: polar variants ; D: function-handle plots).

- [x] `clf` (was already there but listed for completeness)
- [x] `stem3` — 3-D stems through cabinet projection (now WebGL).
- [x] `compass(U, V)` / `compass(Z complex)` — radial arrows from
      origin via quiver wrapper.
- [x] `feather(U, V)` — arrows on the x-axis.
- [x] `spy(M)` — scatter at non-zero entries, axis ij.
- [x] `cdfplot(x)` — empirical CDF via stairs.
- [x] `qqplot(x)` — Q-Q vs the standard normal; Abramowitz-Stegun
      probit + IQR-based reference line.
- [x] `pareto(Y)` — sorted bars + cumulative-percent line.
- [x] `histfit(x[, n])` — histogram + Gaussian fit overlay.
- [x] `gscatter(x, y, g)` — scatter coloured by group label.
- [x] `polarscatter(theta, rho)` — markers on polar axes.
- [x] `polarhistogram(theta[, nbins])` — angular wedge bins.
- [x] `fplot(@(x) f(x), [a b])` — function-handle plot via
      `Engine::callFunctionHandle` (works on TW + VM).
- [x] `fcontour(@(x,y) f, [xa xb ya yb])` — proxies through
      compat.contour with a 30×30 sampled grid.
- [x] `fsurf(@(x,y) f, …)` / `fmesh(@(x,y) f, …)` — surf / mesh from
      a function handle.

## Tier 2 — polygon layer kind + filled shapes (DONE)

Net-new polygon layer kind in CompositePlot (031ca21e) + 8 builtins
that build on it (2603176e). Closed 2026-05-09.

- [x] `patch(X, Y[, C])` / `fill(X, Y[, C])` — universal filled
      polygon. Multi-polygon via column matrix with null separators.
      Color accepts single-char, "#hex", or [r g b] triplet.
- [x] `fill3(X, Y, Z[, C])` — 3-D filled polygons. Now routed
      through the WebGL renderer (Etap 3) with real depth.
- [x] `pie(X[, explode])` / `pie3(X[, explode])` — wedge polygons
      with auto-percentage labels.
- [x] `boxplot(X)` / `boxchart(X)` — Tukey box + whiskers + outliers.
- [x] `violinplot(X)` — KDE shape (Silverman bandwidth) + slim box +
      median dot.
- [x] `bar3(Z)` — 3-D bars. Initially cabinet-projected polygons;
      promoted to WebGL cuboid mesh in Etap 3.
- [x] `waterfall(Z)` — row-by-row ribbons. WebGL strip mesh.

Deferred upgrades (filled bands for `contourf`, stacked `area`)
remain unfinished — both require non-trivial algorithmic work
(filled marching squares for contourf, baseline tracking for
stacked area).

## WebGL — three.js renderer for 3-D figures (DONE)

8-stage roll-out 2026-05-09 / 10. Library: **three.js** (rejected
regl as semi-maintained; bundle delta acceptable in an Electron
app). Single shared `Composite3DPlot.jsx` handles every 3-D figure
kind.

- [x] **Etap 0** — sourcemaps ON by default + `FigureErrorBoundary`
      isolating per-figure crashes (vs. the global one in
      `ErrorBoundary.jsx` that takes over the whole window).
- [x] **Etap 1** — 3-D axes infrastructure: cube edges, grid lines
      on three back faces, tick labels via CSS2DRenderer (HTML
      overlay that follows the camera), axis names, `view(2)` /
      `view(3)` preset shortcuts, `zlim` / `zlabel` builtins
      replacing noops, `axis equal` / `axis vis3d` for 3-D figures.
- [x] **Etap 2** — face-shaded surf. C++ wire format split: surf
      now emits a single dataset with the Z-matrix; mesh keeps the
      legacy two-polyline format. Renderer builds a triangle mesh
      with per-vertex colours sampled from a viridis-ish HSL ramp
      by Z, NaN-aware (cells with non-finite corners leave holes
      instead of stretching geometry).
- [x] **Etap 3** — bar3 / waterfall / fill3 promoted from cabinet-
      projected polygons to raw 3-D coords + WebGL primitives.
      Cabinet pre-projection removed from the C++ side.
- [x] **Etap 4** — lighting model. `lighting('flat'|'gouraud'|
      'phong'|'none')` switches the THREE material; `material('shiny'
      |'metal'|'dull')` tunes specular/shininess (phong only);
      `camlight('left'|'right'|'headlight')` attaches a directional
      light to the camera position; `surfl` is a wrapper that
      drops surf + camlight headlight + gouraud + shiny in one call.
- [x] **Etap 5** — 3-D wrappers. `quiver3` (line segments), `contour3`
      (marching squares per level riding the surface), `surfc` /
      `meshc` (surf/mesh + contour3 in the same axes).
- [x] **Etap 6** — interaction. Hover Raycaster shows (x, y, z)
      data tooltip; `rotate3d` / `pan3d` / `zoom3d` builtins toggle
      OrbitControls per-axis.
- [x] **Etap 7** — PNG export through `canvas.toDataURL` (the SVG
      pipeline can't help WebGL geometry); theme-aware colours
      (cube edges + grid + clear-color read CSS-vars at mount,
      transparent canvas lets the wrapper div carry `--plot-bg` so
      theme switches work without imperative refresh).
- [x] **Etap 8** — edge-case sweep: degenerate inputs (single
      point, constant Z), NaN/Inf propagation, mixed 3-D types in
      one figure, 50×50 surf perf, repeat figure-type swaps.

## FigureWindow rework (DONE)

Closes the user-reported papercuts on the modal:

- [x] X / Y inputs moved from the toolbar into a dedicated
      footer row (`.fw-range-row`) next to the live readout.
- [x] Z input added — 3-D figures now show six inputs (x / y / z
      lo+hi). 2-D and polar keep their existing pairs.
- [x] X / Y / Z inputs are wired live. Composite3DPlot is
      `forwardRef`'d; FigureWindow's `viewport3d` prop overrides
      figure.xlim/ylim/zlim and the renderer's `onBBox` callback
      auto-fills the inputs with the data extent on first mount.
- [x] Fit menu has a 3-D branch: `all axes` / `X only` / `Y only` /
      `Z only` / `reset to data extent`.
- [x] Save / Export for 3-D: PNG via canvas.toDataURL, CSV / TSV /
      JSON via `getCsvData()` (per-layer x / y / z). SVG export
      explicitly disabled for 3-D with an explanatory tooltip.
- [x] 3-D preview cards no longer hard-code a dark background:
      transparent canvas + wrapper div carries `var(--plot-bg)`,
      so light-theme users actually see a light card.
- [x] `.fw-range-row` symmetric padding so the X / Y / Z inputs
      don't kiss the panel borders.

## Grid MATLAB parity (DONE)

`grid` / `grid on` / `grid off` / `grid minor` now match MATLAB:
major and minor are independent state, with `gridUserTouched`
tri-state on the wire so the JS adapter can default 2-D figures to
no grid and 3-D / polar figures to grid-on. Composite3DPlot now
draws minor grid lines (5× denser) on the back faces.

Coherence fixes shipped in the same wave:

- Preview card propagates major + minor (was reading the legacy
  `figure.grid === 'minor'` enum that never matched after the
  wire-format split).
- Heatmap `xlim`/`ylim` now honour script-set values (auto-padding
  was overriding them).
- Legend gating: toolbar button + SVG legend block hidden when the
  script never called `legend(...)`. The HTML overlay double-draw
  bug is fixed by removing the overlay entirely — CompositePlot's
  internal legend is the single source of truth.

End-to-end coverage: `grid-matlab-parity.spec.js` (11 cases) +
`figure-state-parity.spec.js` (20 cases including legend gating
and preview ↔ modal coherence).

## 2026-05-10 cycle: BUGS #38, #39 + imshow + linespec

### Done

- [x] **BUG #38 — linespec / N-V params dropped by renderer.** Fix in
      `parseLineSpec` (token-based parse of color + lineStyle +
      marker), adapter forwards `lineStyle` / `marker` onto layer,
      `CompositePlot.jsx` line render applies `strokeDasharray` and
      overlays MarkerGlyph elements. New SVG dispatcher covers the
      full MATLAB marker set (`o + s d ^ v < > p h x * .`). e2e
      `linespec-params.spec.js` (8 cases).

- [x] **BUG #39 — 3-D grid toggle reset camera + grid hid the data.**
      Split `Composite3DPlot.jsx` mega-effect into (a) figure-data
      rebuild and (b) grid-toggle-only. Added `lastViewRef` so
      `figure.view` re-applies only on actual change — user orbits
      survive every prop tick. Built grid lines on **all six** cube
      faces and let the render tick toggle visibility per-frame
      via dot(camera.position, faceNormal). e2e
      `3d-grid-camera.spec.js` (4 cases) + `data-numkit-3d` canvas
      hook for inspection.

- [x] **`imshow` — display image (grayscale + RGB).** Builtin in
      `libs/graphics/src/library.cpp` covering:
        - `imshow(I)` with class-default range (`uint8`→[0,255],
          `double/single/logical`→[0,1])
        - `imshow(I, [lo hi])` — explicit range
        - `imshow(I, [])` — auto-range (data extent, == imagesc)
        - `imshow(RGB)` for M×N×3 (`uint8` / `double` / `logical`)
      Sets `colormap='gray'` (grayscale only), `axisMode='image'`,
      `axisVisible=false`, `yDir='reverse'`. Reads via
      `Value::elemAsDouble` so all numeric classes Just Work.
      New `axisVisible` field on AxesState + `rgbBytes` /
      `rgbJson` on DatasetInfo + JSON wire shape. IDE: adapter
      `image-rgb` branch, `CompositePlot.jsx` SVG `<image>` from
      canvas data-URL, axisVisible suppresses ticks/frame.
      `axis off` / `axis on` builtin extension flips axisVisible.
      Tests: gtest `figure_test.cpp::ImshowTest` (7 cases), parity
      spec, smoke .m, e2e `imshow.spec.js` (8 cases). Deferred
      parts in `audit/findings/graphics/imshow.md`: filename input,
      `'DisplayRange'`/`'XData'`/`'Colormap'` N-V parsing, RGBA
      M×N×4, `imref2d` spatial referencing.

## Backlog (remaining)

These don't have shipping pressure. Each is a separate investment.

- [ ] `contourf` filled bands — current impl draws lines; real
      filled contours need filled-marching-squares with closed
      polygons per band.
- [ ] `area` stacked — multi-series stacked-area plots; needs
      baseline tracking across layers.
- [ ] `slice` / `isosurface` — volume rendering on 3-D scalar
      grids. Needs marching cubes + 3-D texture sampling.
- [ ] `coneplot` / `streamtube` — 3-D streamline variants with
      cone-headed arrows and tube geometry.
- [ ] `geoplot` / `geoplot3` — geographic axes with basemap tiles.
- [ ] `animatedline` — incremental updates with per-frame draw
      callbacks. Needs an animation infrastructure pass.
- [ ] View-preset toolbar in FigureWindow (top / side / front / iso
      / reset) — would use the imperative `setView(az, el)` handle
      already on Composite3DPlot; just needs UI.
- [ ] Higher-resolution 3-D PNG export — current
      `getCanvasDataURL(scale)` ignores scale. Offscreen render at
      target resolution.
- [ ] linkprop for 3-D camera across subplot cells.

## Notes

- Polar plots stay on their own `kind: 'polar'` renderer. Polar
  variants of B1-B3 builtins (polarscatter, polarhistogram) live
  there; they share the polar adapter + `PolarPlot.jsx` renderer.
- Subplot grid (`kind: 'subplot'`) is composition only — its
  cells inherit composite / composite3d / polar features.
- Bug fixes happen on `fix/ide-bugs` branches per cycle. This
  backlog is for net-new features only.
