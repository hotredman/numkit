/**
 * Adapters that bridge the engine's data shape to the mockup components.
 *
 * The mockup expects:
 *   variable = { name, type, kind, size, bytes, data:[[...]], preview, min?, max?, mean? }
 *   figure   = { id, title, xLabel, yLabel, xRange, yRange,
 *                series:[{ name, x:Number[], y:Number[], color, width?, opacity? }] }
 *
 * The engine produces:
 *   getVars() → { name: { type, size, bytes?, preview } | plain JS value }
 *   figure    = { id, datasets:[{x,y,type,label?,style?}], config:{title,xlabel,ylabel,xlim?,ylim?,grid,legend?} }
 */

const KIND_PALETTE = ['#7fd99a', '#5fb3d4', '#e9b870', '#9b8cf2', '#e26a6a', '#d4a5e6', '#f2a37e', '#6fcfbf'];

// MATLAB-style line spec parsing: 'r--o' → { color: '#f07070', dashed: true, marker: 'o' }
const STYLE_COLOR = { r: '#f07070', g: '#6ee7a0', b: '#60d0f0', k: '#d4d4f0', m: '#e070c0', c: '#60d0f0', y: '#e8d060', w: '#ffffff' };
function parseLineSpec(s) {
  if (!s || typeof s !== 'string') return {};
  let color = null;
  for (const ch of s) if (STYLE_COLOR[ch]) { color = STYLE_COLOR[ch]; break; }
  return { color };
}

/* ─────────────── workspace variables ─────────────── */

/**
 * Coerce a preview value (number | array | string) into a 2-D array suitable
 * for the Variable Editor table.
 */
function previewToData(preview, type) {
  if (preview == null) return [['<unavailable>']];
  if (typeof preview === 'number') return [[preview]];
  if (typeof preview === 'string') return [[preview]];
  if (typeof preview === 'boolean') return [[preview ? 'true' : 'false']];
  if (Array.isArray(preview)) {
    if (preview.length === 0) return [[]];
    if (Array.isArray(preview[0])) return preview.map((r) => r.slice());
    return [preview.slice()];
  }
  return [[String(preview)]];
}

function classify(size, type) {
  if (type === 'char' || type === 'string') return 'string';
  if (!size || size === '1x1' || size === '1×1') return 'scalar';
  const m = String(size).match(/(\d+)\s*[x×]\s*(\d+)/);
  if (!m) return 'matrix';
  const r = +m[1], c = +m[2];
  if (r === 1 || c === 1) return 'vector';
  return 'matrix';
}

function previewString(preview, kind, sizeStr, type) {
  if (preview == null) return `[${sizeStr} ${type || 'double'}]`;
  if (typeof preview === 'number') {
    return Number.isInteger(preview) ? String(preview) : preview.toFixed(6);
  }
  if (typeof preview === 'string') return `"${preview}"`;
  if (typeof preview === 'boolean') return preview ? 'true' : 'false';
  if (Array.isArray(preview)) {
    const flat = preview.flat();
    if (flat.length === 0) return '[]';
    const fmt = (x) => typeof x === 'number'
      ? (Number.isInteger(x) ? String(x) : x.toFixed(3))
      : String(x);
    if (flat.length <= 5) return flat.map(fmt).join(', ');
    return flat.slice(0, 5).map(fmt).join(', ') + ', …';
  }
  return String(preview);
}

function statsFromData(data) {
  let min = Infinity, max = -Infinity, sum = 0, n = 0;
  for (const row of data) for (const v of row) {
    if (typeof v === 'number') {
      if (v < min) min = v;
      if (v > max) max = v;
      sum += v; n++;
    }
  }
  if (n === 0) return { min: null, max: null, mean: null };
  return { min, max, mean: sum / n };
}

/**
 * Convert engine.getVars() output → array of variable objects shaped for the
 * mockup WorkspacePanel / VariableEditor.
 */
export function adaptVariables(engineVars) {
  if (!engineVars) return [];
  const out = [];
  for (const [name, raw] of Object.entries(engineVars)) {
    const isStruct = raw && typeof raw === 'object' && !Array.isArray(raw) && 'type' in raw;
    const type = isStruct ? (raw.type || 'double')
      : Array.isArray(raw) ? 'double'
      : typeof raw === 'string' ? 'char'
      : typeof raw === 'boolean' ? 'logical'
      : 'double';
    const size = isStruct ? (raw.size || '1×1')
      : Array.isArray(raw) ? (Array.isArray(raw[0]) ? `${raw.length}×${raw[0].length}` : `1×${raw.length}`)
      : typeof raw === 'string' ? `1×${raw.length}`
      : '1×1';
    const sizeNorm = String(size).replace('x', '×');
    const kind = classify(size, type);
    const preview = isStruct ? raw.preview : raw;
    const bytes = isStruct ? (raw.bytes || 8) : 8;
    const data = previewToData(preview, type);
    const stats = statsFromData(data);

    out.push({
      name,
      type, kind,
      size: sizeNorm,
      bytes,
      data,
      preview: previewString(preview, kind, sizeNorm, type),
      min: stats.min, max: stats.max, mean: stats.mean,
    });
  }
  return out;
}

/* ─────────────── figures ─────────────── */

function rangeFromArr(arr, fallbackPad = 0.05) {
  let lo = Infinity, hi = -Infinity;
  for (const v of arr) {
    if (Number.isFinite(v)) {
      if (v < lo) lo = v;
      if (v > hi) hi = v;
    }
  }
  if (!Number.isFinite(lo) || !Number.isFinite(hi)) return [-1, 1];
  if (lo === hi) {
    const pad = Math.abs(lo) * fallbackPad || 0.5;
    return [lo - pad, hi + pad];
  }
  return [lo, hi];
}

/**
 * Pull the first axes' datasets+config out of the engine figure. The engine
 * emits `{ id, axes:[{ datasets, config, subplotIndex }] }` for new figures;
 * legacy `{ id, datasets, config }` is supported as a fallback.
 */
function flatten(fig) {
  if (Array.isArray(fig.axes) && fig.axes.length > 0) {
    return { datasets: fig.axes[0].datasets || [], cfg: fig.axes[0].config || {} };
  }
  return { datasets: Array.isArray(fig.datasets) ? fig.datasets : [],
           cfg: fig.config || {} };
}

/**
 * Convert one engine dataset into a CompositePlot layer object. Returns
 * null for unsupported types. Layers carry data in original-data coords;
 * the renderer maps them through current sx/sy at draw time.
 */
function datasetToLayer(d, palette_idx, ctx) {
  const t = (d.type || '').toLowerCase();
  const styleObj = typeof d.style === 'string' ? parseLineSpec(d.style)
                 : (d.style || {});
  const baseColor = styleObj.color || d.color || KIND_PALETTE[palette_idx % KIND_PALETTE.length];

  if (t === 'imagesc' || t === 'pcolor') {
    if (!d.z) return null;
    const z = d.z;
    const nR = z.length;
    const nC = z[0]?.length || 0;
    const cminOrig = (typeof d.cminOrig === 'number') ? d.cminOrig : 0;
    const cmaxOrig = (typeof d.cmaxOrig === 'number') ? d.cmaxOrig : 1;
    return {
      kind: 'heatmap',
      // pcolor places (x, y) at cell vertices (imagesc at cell centres).
      // Range computation in adaptAxes pads imagesc by ±cellW/2 on each
      // edge; for pcolor the x/y vector spans the panel exactly.
      vertexAligned: t === 'pcolor',
      z,                                                  // uint8 indices, row-major 2-D
      cmin: cminOrig, cmax: cmaxOrig,                     // aliases used by status / exports
      cminOrig, cmaxOrig,
      colorScaleBaked: d.colorScaleBaked || null,         // 'log' | null
      colormap: ctx.cfg.colormap || 'parula',
      downsampled: d.downsampled === true,
      originalRows: d.originalRows || nR,
      originalCols: d.originalCols || nC,
      _figId: ctx.figId,
      _axIdx: ctx.axIdx,
      _dsIdx: ctx.dsIdx,
    };
  }

  if (t === 'text') {
    // Engine packs name-value extras into ds.style as "key=val;key=val".
    const extras = {};
    if (typeof d.style === 'string') {
      for (const kv of d.style.split(';')) {
        const [k, v] = kv.split('=');
        if (k && v) extras[k.trim()] = v.trim();
      }
    }
    return {
      kind: 'text',
      x: Number(d.x?.[0] ?? 0),
      y: Number(d.y?.[0] ?? 0),
      text:  d.label || '',
      color: extras.color || 'white',
      fontSize: extras.fontSize ? Number(extras.fontSize) : 11,
    };
  }

  const supported = ['line', 'plot', 'scatter', 'stem', 'stairs',
                     'bar', 'hist', 'semilogx', 'semilogy', 'loglog',
                     'errorbar', 'barh', 'area', 'quiver',
                     'plot3', 'scatter3'];
  if (!supported.includes(t)) return null;
  const x = Array.isArray(d.x) ? d.x.map(Number) : [];
  const y = Array.isArray(d.y) ? d.y.map(Number) : [];
  let mode = 'line';
  if (t === 'scatter') mode = 'scatter';
  else if (t === 'stem') mode = 'stem';
  else if (t === 'bar' || t === 'hist') mode = 'bar';
  else if (t === 'barh') mode = 'barh';
  else if (t === 'stairs') mode = 'stairs';
  else if (t === 'errorbar') mode = 'errorbar';
  else if (t === 'area') mode = 'area';
  else if (t === 'quiver') mode = 'quiver';
  else if (t === 'plot3') mode = 'line';
  else if (t === 'scatter3') mode = 'scatter';

  // 3-D projection (plot3 / scatter3): cabinet projection collapses
  // (x, y, z) → 2-D screen coords by adding z*scale*cos(α) to x and
  // z*scale*sin(α) to y. α = 30°, scale = 0.5 — standard cabinet
  // values that give a readable depth cue without distorting the
  // x-y plane. Fully 3-D camera (orbit / dolly) is B3 territory.
  let xOut = x, yOut = y;
  if (t === 'plot3' || t === 'scatter3') {
    // For plot3/scatter3, d.z is a 1-D vector (different shape from
    // imagesc's 2-D matrix; disambiguated by the type field).
    const z = Array.isArray(d.z) && !Array.isArray(d.z[0])
      ? d.z.map(Number) : [];
    const zScale = 0.5;
    const cosA = Math.cos(Math.PI / 6);  // 30°
    const sinA = Math.sin(Math.PI / 6);
    xOut = x.map((xi, i) => xi + (Number(z[i]) || 0) * zScale * cosA);
    yOut = y.map((yi, i) => yi + (Number(z[i]) || 0) * zScale * sinA);
  } else if (mode === 'barh') {
    // barh stores `xJson = vertical positions` and `yJson = bar lengths`
    // on the C++ side (mirroring bar's input order). For axis-range
    // scanning + rendering it's cleaner to expose those swapped: the
    // X axis of the chart shows lengths, the Y axis shows positions.
    xOut = y;
    yOut = x;
  }

  const layer = {
    kind: 'series',
    mode,
    name: d.label || `series ${palette_idx + 1}`,
    x: xOut, y: yOut,
    color: baseColor,
    width: d.lineWidth || styleObj.lineWidth || styleObj.width || 1.5,
    size:  d.markerSize || styleObj.markerSize || 3,
    opacity: d.style?.opacity ?? 1,
  };

  // Errorbar bounds — symmetric (e) or asymmetric (eNeg/ePos). The
  // renderer derives bar limits as y-eNeg .. y+ePos with eJson
  // doubled-up when symmetric.
  if (t === 'errorbar') {
    const e    = Array.isArray(d.e)    ? d.e.map(Number)    : null;
    const eNeg = Array.isArray(d.eNeg) ? d.eNeg.map(Number) : null;
    const ePos = Array.isArray(d.ePos) ? d.ePos.map(Number) : null;
    layer.eNeg = eNeg || e || [];
    layer.ePos = ePos || e || [];
  }

  // Area baseline — engine packs `base=N` into ds.style (semicolon-
  // separated alongside any other extras). Default is 0.
  if (t === 'area') {
    layer.baseline = 0;
    if (typeof d.style === 'string') {
      for (const kv of d.style.split(';')) {
        const m = kv.trim().match(/^base=(-?[\d.eE+-]+)$/);
        if (m) layer.baseline = Number(m[1]);
      }
    }
  }

  // Quiver components — u/v parallel-indexed with x/y. The renderer
  // draws each arrow from (x[i], y[i]) to (x[i]+u[i]*scale,
  // y[i]+v[i]*scale). Optional scale comes through style="scale=N".
  if (t === 'quiver') {
    layer.u = Array.isArray(d.u) ? d.u.map(Number) : [];
    layer.v = Array.isArray(d.v) ? d.v.map(Number) : [];
    layer.scale = 1;
    if (typeof d.style === 'string') {
      for (const kv of d.style.split(';')) {
        const m = kv.trim().match(/^scale=(-?[\d.eE+-]+)$/);
        if (m) layer.scale = Number(m[1]);
      }
    }
  }

  return layer;
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
function adaptAxes(figId, cellId, datasets, cfg, axIdx = 0) {
  // Polar — cfg.polar=true, datasets carry (theta, rho) as (x, y).
  // Lives outside the composite path because polar coords aren't (x, y).
  if (cfg.polar) {
    const series = datasets.map((d, i) => {
      const styleObj = typeof d.style === 'string' ? parseLineSpec(d.style) : (d.style || {});
      return {
        name: d.label || `series ${i + 1}`,
        theta: Array.isArray(d.x) ? d.x.map(Number) : [],
        rho:   Array.isArray(d.y) ? d.y.map(Number) : [],
        color: styleObj.color || d.color || KIND_PALETTE[i % KIND_PALETTE.length],
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
      grid: cfg.grid || '',
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

  // Compute xRange / yRange. With a heatmap layer, use the imagesc x/y
  // vector extents (they fully bound the matrix). Otherwise scan series.
  let xRange, yRange;
  if (heatmapLy) {
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
  } else {
    let xLo = Infinity, xHi = -Infinity, yLo = Infinity, yHi = -Infinity;
    for (const ly of layers) {
      if (ly.kind !== 'series') continue;
      // Quiver: arrow tips extend past (x, y) by (u*scale, v*scale).
      // Include both endpoints so the autoscaled viewport actually
      // contains the tips and the arrowheads aren't clipped.
      let xs = ly.x, ys = ly.y;
      if (ly.mode === 'quiver' && Array.isArray(ly.u) && Array.isArray(ly.v)) {
        const s = Number.isFinite(ly.scale) ? ly.scale : 1;
        xs = ly.x.concat(ly.x.map((v, i) => v + (Number(ly.u[i]) || 0) * s));
        ys = ly.y.concat(ly.y.map((v, i) => v + (Number(ly.v[i]) || 0) * s));
      }
      const [a, b] = rangeFromArr(xs);
      const [c, d] = rangeFromArr(ys);
      if (a < xLo) xLo = a;
      if (b > xHi) xHi = b;
      if (c < yLo) yLo = c;
      if (d > yHi) yHi = d;
    }
    xRange = (Array.isArray(cfg.xlim) && cfg.xlim.length === 2)
      ? cfg.xlim.slice()
      : [Number.isFinite(xLo) ? xLo : -1, Number.isFinite(xHi) ? xHi : 1];
    yRange = (Array.isArray(cfg.ylim) && cfg.ylim.length === 2)
      ? cfg.ylim.slice()
      : [Number.isFinite(yLo) ? yLo : -1, Number.isFinite(yHi) ? yHi : 1];
    // `axis tight` means "no whitespace padding around data". Skip
    // the default 4%/6% pad. Auto-scale still happens.
    const tight = (cfg.axisMode === 'tight');
    if (!cfg.xlim && !tight) {
      const pad = (xRange[1] - xRange[0]) * 0.04 || 0.5;
      xRange[0] -= pad; xRange[1] += pad;
    }
    if (!cfg.ylim && !tight) {
      const pad = (yRange[1] - yRange[0]) * 0.06 || 0.5;
      yRange[0] -= pad; yRange[1] += pad;
    }
  }

  return {
    kind: 'composite',
    id: cellId,
    title:  cfg.title  || '',
    xLabel: cfg.xlabel || '',
    yLabel: cfg.ylabel || '',
    xRange, yRange,
    grid: cfg.grid || '',
    xscale: cfg.xscale || 'linear',
    yscale: cfg.yscale || 'linear',
    // axisMode: 'equal' | 'square' | 'tight' | 'auto' | '' (default).
    // Renderer reshapes sx/sy or panel size based on this value.
    axisMode: cfg.axisMode || '',
    // xDir / yDir: 'normal' (default) or 'reverse'. Renderer flips
    // the corresponding sx/sy mapping when 'reverse'. axis('ij')
    // shorthand is resolved on the C++ side: it sets yDir='reverse'
    // and axisMode='ij', so the renderer only needs to inspect yDir.
    xDir: cfg.xDir || 'normal',
    yDir: cfg.yDir || 'normal',
    layers,
  };
}

/**
 * Convert one engine figure → IDE figure shape. Returns an object with a
 * `kind` field that the caller uses to pick a renderer:
 *   { kind: 'composite', layers, ... }            → CompositePlot
 *   { kind: 'polar',     series, thetaDir, ... }  → PolarPlot
 *   { kind: 'subplot',   cells, grid, ... }       → SubplotGrid
 *   null                                          → not renderable yet
 */
export function adaptFigure(fig) {
  if (!fig) return null;

  // Subplot grid — multiple axes laid out as a [rows, cols] tile. Each axes
  // is recursively adapted into one cell of the grid.
  if (Array.isArray(fig.subplotGrid) && fig.subplotGrid.length === 2
      && Array.isArray(fig.axes) && fig.axes.length > 0) {
    const [rows, cols] = fig.subplotGrid;
    const cells = [];
    fig.axes.forEach((ax, i) => {
      const cell = adaptAxes(fig.id, `${fig.id}-${i}`, ax.datasets || [], ax.config || {}, i);
      if (cell) {
        cell.subplotIndex = ax.subplotIndex || (i + 1);
        cells.push(cell);
      }
    });
    if (cells.length === 0) return null;
    return {
      kind: 'subplot',
      id: fig.id,
      title: `Figure ${fig.id}`,
      grid: [rows, cols],
      cells,
      _raw: fig,
    };
  }

  // Single-axes figure — adapt directly.
  const { datasets, cfg } = flatten(fig);
  const adapted = adaptAxes(fig.id, fig.id, datasets, cfg);
  if (!adapted) return null;
  if (!adapted.title) adapted.title = `Figure ${fig.id}`;
  adapted._raw = fig;
  return adapted;
}

export function adaptFigures(figs) {
  if (!Array.isArray(figs)) return [];
  return figs.map(adaptFigure).filter(Boolean);
}
