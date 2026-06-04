// adapters.axes.js — adaptAxes: dataset list + config -> axis layer (ranges,
// ticks, grid, log scaling) + the rangeFromArr range helper.
import { parseLineSpec, KIND_PALETTE } from './adapters.linespec';
import { datasetToLayer } from './adapters.layer';

function rangeFromArr(arr, { fallbackPad = 0.05, positiveOnly = false } = {}) {
  let lo = Infinity, hi = -Infinity;
  for (const v of arr) {
    // positiveOnly: a log axis can't represent ≤ 0, so its limits are
    // computed over the positive data only — MATLAB drops non-positive
    // points ("Negative data ignored") rather than stretching the range
    // into negative territory (which would disable the log mapping).
    if (Number.isFinite(v) && (!positiveOnly || v > 0)) {
      if (v < lo) lo = v;
      if (v > hi) hi = v;
    }
  }
  if (!Number.isFinite(lo) || !Number.isFinite(hi)) {
    // No qualifying value. For a log axis with no positive data, return
    // an empty range so this layer contributes NOTHING to the merge
    // (Infinity < xLo / -Infinity > xHi are both false) — the caller's
    // final `Number.isFinite ? … : fallback` then kicks in. The generic
    // (non-log) empty case keeps the historical [-1, 1] fallback.
    return positiveOnly ? [Infinity, -Infinity] : [-1, 1];
  }
  if (lo === hi) {
    const pad = Math.abs(lo) * fallbackPad || 0.5;
    return [lo - pad, hi + pad];
  }
  return [lo, hi];
}

/**
 * Adapt one axes (datasets + config) into a renderable cell. Used both by
 * single-axes figures and by every cell of a subplot grid.
 *
 * Composite output: `kind: 'composite'` carrying a `layers[]` array of
 * heterogeneous layer objects (heatmap, series, text). Polar plots take
 * a separate path because their coordinate system is fundamentally
 * different.
 */
export function adaptAxes(figId, cellId, datasets, cfg, axIdx = 0) {
  // Polar — cfg.polar=true, datasets carry (theta, rho) as (x, y).
  // Lives outside the composite path because polar coords aren't (x, y).
  if (cfg.polar) {
    const series = datasets.map((d, i) => {
      const styleObj = typeof d.style === 'string' ? parseLineSpec(d.style) : (d.style || {});
      // Polar mode → PolarPlot's renderer switch:
      //   'line'    — polarplot     (polyline)
      //   'scatter' — polarscatter  (markers)
      //   'bar'     — polarhistogram(radial wedges)
      //   'rose'    — rose          (wedge-from-origin variant of bar)
      //   'bubble'  — polarbubblechart (markers with per-point size)
      //   'compass' — compass       (arrow from origin to each point)
      let mode = 'line';
      const t = (d.type || '').toLowerCase();
      if      (t === 'scatter') mode = 'scatter';
      else if (t === 'bar')     mode = 'bar';
      else if (t === 'rose')    mode = 'rose';
      else if (t === 'bubble')  mode = 'bubble';
      else if (t === 'compass') mode = 'compass';
      return {
        name: d.label || `series ${i + 1}`,
        mode,
        theta: Array.isArray(d.x) ? d.x.map(Number) : [],
        rho:   Array.isArray(d.y) ? d.y.map(Number) : [],
        // Per-point size (polarbubblechart). Wire field is `size`,
        // distinct from the dataset-level `markerSize` style attr.
        sizes: Array.isArray(d.size) ? d.size.map(Number) : null,
        // Per-point colour: either an array of [r,g,b] triplets
        // (1×3 for a shared color, N×3 for per-point) OR a flat
        // array of colormap-index scalars. Renderer handles both.
        pointColors: Array.isArray(d.pointColor) ? d.pointColor : null,
        color: styleObj.color || d.color || KIND_PALETTE[i % KIND_PALETTE.length],
        // LineSpec dash pattern + marker for compass (and future
        // line-style honoring modes). 'solid' default keeps
        // existing visuals.
        dash:  styleObj.dash || null,
        width: d.lineWidth || styleObj.lineWidth || 1.6,
      };
    });
    return {
      kind: 'polar',
      id: cellId,
      title: cfg.title || '',
      thetaDir: cfg.thetaDir || 'counterclockwise',
      thetaZeroLocation: cfg.thetaZeroLocation || 'right',
      rlim: cfg.rlim,
      // thetalim limits the angular sweep. Numeric form: [thetaMin,
      // thetaMax] in DEGREES (MATLAB convention). null = full
      // 360° sweep (default).
      thetalim: Array.isArray(cfg.thetalim) && cfg.thetalim.length === 2
        ? cfg.thetalim.slice() : null,
      // Custom theta/r ticks + labels (MATLAB thetaticks / rticks /
      // thetaticklabels / rticklabels). Null = renderer auto-grid.
      thetaticks:      Array.isArray(cfg.thetaticks)      ? cfg.thetaticks      : null,
      rticks:          Array.isArray(cfg.rticks)          ? cfg.rticks          : null,
      thetaticklabels: Array.isArray(cfg.thetaticklabels) ? cfg.thetaticklabels : null,
      rticklabels:     Array.isArray(cfg.rticklabels)     ? cfg.rticklabels     : null,
      grid: cfg.grid !== undefined ? cfg.grid : 'on',  // polar default = on (MATLAB)
      gridMinor: cfg.gridMinor || 'off',
      series,
    };
  }

  // Composite path — heterogeneous layers in original z-order.
  const layers = [];
  let paletteIdx = 0;
  for (let i = 0; i < datasets.length; i++) {
    const ly = datasetToLayer(datasets[i], paletteIdx,
                              { cfg, figId, axIdx, dsIdx: i });
    if (!ly) continue;
    if (ly.kind === 'series') paletteIdx++;
    layers.push(ly);
  }
  if (layers.length === 0) return null;

  const heatmapLy = layers.find((l) => l.kind === 'heatmap');
  const rgbLy     = layers.find((l) => l.kind === 'image-rgb');

  // Compute xRange / yRange. With a heatmap layer, use the imagesc x/y
  // vector extents (they fully bound the matrix). Otherwise scan series.
  let xRange, yRange;
  if (rgbLy) {
    // image-rgb spans 0.5..nC+0.5 / 0.5..nR+0.5 by default (pixel
    // centres at integer coords with ±0.5 padding). User xlim/ylim
    // override.
    const nR = rgbLy.nR, nC = rgbLy.nC;
    xRange = (Array.isArray(cfg.xlim) && cfg.xlim.length === 2)
      ? cfg.xlim.slice() : [0.5, nC + 0.5];
    yRange = (Array.isArray(cfg.ylim) && cfg.ylim.length === 2)
      ? cfg.ylim.slice() : [0.5, nR + 0.5];
  } else if (heatmapLy) {
    // Pull x/y vectors back off the original imagesc / pcolor dataset.
    const dsIdx = heatmapLy._dsIdx;
    const d = datasets[dsIdx];
    const nR = heatmapLy.originalRows, nC = heatmapLy.originalCols;
    const xr = d.x || [0, Math.max(0, nC - 1)];
    const yr = d.y || [0, Math.max(0, nR - 1)];
    const x0 = xr[0], x1 = xr[xr.length - 1];
    const y0 = yr[0], y1 = yr[yr.length - 1];
    if (heatmapLy.vertexAligned) {
      // pcolor — (x, y) are cell vertices, span the panel exactly.
      xRange = [x0, x1];
      yRange = [y0, y1];
    } else {
      // imagesc — (x, y) are cell centres, pad ±cell/2 on each edge.
      const cW = nC > 1 ? (x1 - x0) / (nC - 1) : 1;
      const cH = nR > 1 ? (y1 - y0) / (nR - 1) : 1;
      xRange = [x0 - cW / 2, x1 + cW / 2];
      yRange = [y0 - cH / 2, y1 + cH / 2];
    }
    // User-set xlim / ylim override the auto-padded extent so the
    // modal's range inputs and the rendered viewport match the
    // script's intent exactly (no half-cell drift).
    if (Array.isArray(cfg.xlim) && cfg.xlim.length === 2) xRange = cfg.xlim.slice();
    if (Array.isArray(cfg.ylim) && cfg.ylim.length === 2) yRange = cfg.ylim.slice();
  } else {
    let xLo = Infinity, xHi = -Infinity;
    let yLoL = Infinity, yHiL = -Infinity;  // left-side Y bounds
    let yLoR = Infinity, yHiR = -Infinity;  // right-side Y bounds
    var yRange2 = null;
    for (const ly of layers) {
      if (ly.kind !== 'series') continue;
      // Skip reference lines from auto-range — xline(5) shouldn't
      // contribute Y bounds (its Y is a sentinel) and yline(3)
      // shouldn't contribute X.
      if (ly.mode === 'xline' || ly.mode === 'yline') continue;
      // Quiver: arrow tips extend past (x, y) by (u*scale, v*scale).
      let xs = ly.x, ys = ly.y;
      if (ly.mode === 'quiver' && Array.isArray(ly.u) && Array.isArray(ly.v)) {
        const s = Number.isFinite(ly.scale) ? ly.scale : 1;
        xs = ly.x.concat(ly.x.map((v, i) => v + (Number(ly.u[i]) || 0) * s));
        ys = ly.y.concat(ly.y.map((v, i) => v + (Number(ly.v[i]) || 0) * s));
      }
      // Log axes fit their limits to positive data only (MATLAB drops
      // non-positive points). Right-side series follow yscale2.
      const yLogSide = (ly.yside === 'right' ? cfg.yscale2 : cfg.yscale) === 'log';
      const [a, b] = rangeFromArr(xs, { positiveOnly: cfg.xscale === 'log' });
      const [c, d] = rangeFromArr(ys, { positiveOnly: yLogSide });
      if (a < xLo) xLo = a;
      if (b > xHi) xHi = b;
      if (ly.yside === 'right') {
        if (c < yLoR) yLoR = c;
        if (d > yHiR) yHiR = d;
      } else {
        if (c < yLoL) yLoL = c;
        if (d > yHiL) yHiL = d;
      }
    }
    xRange = (Array.isArray(cfg.xlim) && cfg.xlim.length === 2)
      ? cfg.xlim.slice()
      : [Number.isFinite(xLo) ? xLo : -1, Number.isFinite(xHi) ? xHi : 1];
    yRange = (Array.isArray(cfg.ylim) && cfg.ylim.length === 2)
      ? cfg.ylim.slice()
      : [Number.isFinite(yLoL) ? yLoL : -1, Number.isFinite(yHiL) ? yHiL : 1];
    // Right-side range (only meaningful when yyEnabled). When no right
    // dataset is present we still synthesise a sane fallback so the
    // renderer can at least draw the right axis without NaNs.
    if (cfg.yyEnabled) {
      yRange2 = (Array.isArray(cfg.ylim2) && cfg.ylim2.length === 2)
        ? cfg.ylim2.slice()
        : [Number.isFinite(yLoR) ? yLoR : -1, Number.isFinite(yHiR) ? yHiR : 1];
    }
    // `axis tight` means "no whitespace padding around data". Skip
    // the default 4%/6% pad. Auto-scale still happens.
    const tight = (cfg.axisMode === 'tight');
    // Pad the auto-range. Linear axes get a flat margin; LOG axes must
    // pad multiplicatively (in log space) — a flat linear margin on log
    // data (e.g. [1, 1000] → [-39, 1040]) pushes the lower bound ≤ 0,
    // which silently disables the log mapping downstream (the renderer's
    // xLogActive guard needs lo > 0) so the axis renders LINEAR despite
    // xscale === 'log'. Log-space padding keeps the bound strictly
    // positive, matching MATLAB's loglog / semilog auto-limits.
    const padRange = (range, frac, isLog) => {
      if (isLog && range[0] > 0 && range[1] > 0) {
        const lo = Math.log10(range[0]);
        const hi = Math.log10(range[1]);
        const p = (hi - lo) * frac || 0.05;
        range[0] = Math.pow(10, lo - p);
        range[1] = Math.pow(10, hi + p);
      } else {
        const p = (range[1] - range[0]) * frac || (isLog ? 0 : 0.5);
        range[0] -= p; range[1] += p;
      }
    };
    if (!cfg.xlim && !tight) padRange(xRange, 0.04, cfg.xscale === 'log');
    if (!cfg.ylim && !tight) padRange(yRange, 0.06, cfg.yscale === 'log');
    if (cfg.yyEnabled && !cfg.ylim2 && !tight) padRange(yRange2, 0.06, cfg.yscale2 === 'log');
  }

  // 3-D detection: any layer with raw z data → route to the WebGL
  // renderer. plot3 / scatter3 / stem3 / surf / mesh all emit on the
  // wire as plot3 or scatter3 datasets, so this single check covers
  // the whole MVP surface.
  const has3D = layers.some((ly) => ly && ly.kind === 'series' && Array.isArray(ly.z));
  if (has3D) {
    // Honour user-set lims; auto-fit when omitted (renderer computes
    // bbox over layer data). Null sentinel = auto.
    const xlim3 = Array.isArray(cfg.xlim) && cfg.xlim.length === 2 ? cfg.xlim : null;
    const ylim3 = Array.isArray(cfg.ylim) && cfg.ylim.length === 2 ? cfg.ylim : null;
    const zlim3 = Array.isArray(cfg.zlim) && cfg.zlim.length === 2 ? cfg.zlim : null;
    return {
      kind: 'composite3d',
      id: cellId,
      title:  cfg.title  || '',
      xLabel: cfg.xlabel || '',
      yLabel: cfg.ylabel || '',
      zLabel: cfg.zlabel || '',
      xlim: xlim3,
      ylim: ylim3,
      zlim: zlim3,
      // axisMode forwarded so the renderer can honour 'equal' / 'vis3d'
      // (equal data units per world unit on every axis).
      axisMode: cfg.axisMode || '',
      // grid / box toggles. Tri-state on the wire: cfg.grid is
      // present only when the script called grid(...) explicitly. If
      // absent, MATLAB defaults a 3-D figure to `grid on` (a
      // wireframe without a frame is unreadable).
      grid: cfg.grid !== undefined ? cfg.grid : 'on',
      gridMinor: cfg.gridMinor || 'off',
      // view: [az, el] in degrees from the C++ view(az, el) call;
      // null = renderer's default (-37.5°, 30°).
      view: Array.isArray(cfg.view) && cfg.view.length === 2 ? cfg.view : null,
      // 3-D lighting / material from the C++ side (empty → renderer
      // default).
      lighting: cfg.lighting || '',
      material: cfg.material || '',
      camlight: cfg.camlight || '',
      // Interaction toggles. '' = default (all enabled).
      rotate3d: cfg.rotate3d || '',
      pan3d: cfg.pan3d || '',
      zoom3d: cfg.zoom3d || '',
      layers,
    };
  }

  return {
    kind: 'composite',
    id: cellId,
    title:  cfg.title  || '',
    subtitle: cfg.subtitle || '',
    xLabel: cfg.xlabel || '',
    yLabel: cfg.ylabel || '',
    xRange, yRange,
    // 2-D default is `off` (MATLAB parity). cfg.grid is present only
    // when grid(...) was explicitly called.
    grid: cfg.grid || 'off',
    gridMinor: cfg.gridMinor || 'off',
    xscale: cfg.xscale || 'linear',
    yscale: cfg.yscale || 'linear',
    // axisMode: 'equal' | 'square' | 'tight' | 'auto' | '' (default).
    // Renderer reshapes sx/sy or panel size based on this value.
    axisMode: cfg.axisMode || '',
    // axisVisible: false → hide ticks/labels/box (imshow / `axis off`).
    // Field is absent in JSON unless the script set it false; we treat
    // missing as visible.
    axisVisible: cfg.axisVisible !== false,
    // boxOn: MATLAB box on/off — show full frame vs. just bottom+left.
    // Default true; off only when script said `box off` explicitly.
    boxOn: cfg.box !== 'off',
    // Custom tick positions / labels (xticks / yticks / xticklabels /
    // yticklabels). Empty arrays → renderer falls back to niceTicks
    // auto-generation. Labels are honoured only when their length
    // matches the corresponding ticks count.
    xTicks: Array.isArray(cfg.xticks) ? cfg.xticks.slice() : null,
    yTicks: Array.isArray(cfg.yticks) ? cfg.yticks.slice() : null,
    xTickLabels: Array.isArray(cfg.xticklabels) ? cfg.xticklabels.slice() : null,
    yTickLabels: Array.isArray(cfg.yticklabels) ? cfg.yticklabels.slice() : null,
    xTickFormat: typeof cfg.xtickformat === 'string' ? cfg.xtickformat : '',
    yTickFormat: typeof cfg.ytickformat === 'string' ? cfg.ytickformat : '',
    customColormap: Array.isArray(cfg.customColormap) ? cfg.customColormap.slice() : null,
    legendBoxOn: cfg.legendBox !== 'off',
    xTickAngle: Number.isFinite(cfg.xtickangle) ? Number(cfg.xtickangle) : 0,
    yTickAngle: Number.isFinite(cfg.ytickangle) ? Number(cfg.ytickangle) : 0,
    // xDir / yDir: 'normal' (default) or 'reverse'. Renderer flips
    // the corresponding sx/sy mapping when 'reverse'. axis('ij')
    // shorthand is resolved on the C++ side: it sets yDir='reverse'
    // and axisMode='ij', so the renderer only needs to inspect yDir.
    xDir: cfg.xDir || 'normal',
    yDir: cfg.yDir || 'normal',
    // Legend labels (positional override of layer.name) + placement.
    // legend is an array of strings — empty / undefined disables the
    // legend block. legendLocation defaults to 'best' when labels are
    // present but no explicit location was set.
    legend: Array.isArray(cfg.legend) ? cfg.legend.slice() : [],
    legendLocation: cfg.legendLocation || '',
    // Colorbar placement — only honoured when there's a heatmap layer.
    // Empty = bar hidden (the default until `colorbar()` is called).
    colorbarLocation: cfg.colorbarLocation || '',
    // yyaxis: dual Y-axis state. yyEnabled gates the secondary axis
    // rendering; right-side layers (layer.yside === 'right') are routed
    // through yRange2 + yLabel2 + yscale2.
    yyEnabled: !!cfg.yyEnabled,
    yLabel2: cfg.ylabel2 || '',
    yRange2: (typeof yRange2 !== 'undefined' && yRange2) ? yRange2 : null,
    yscale2: cfg.yscale2 || 'linear',
    layers,
  };
}
