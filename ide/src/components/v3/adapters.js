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
 * Adapt one axes (datasets + config) into a renderable cell. Used both by
 * single-axes figures and by every cell of a subplot grid.
 *
 * `figId` and `cellId` together build a unique React key + clip-path id —
 * subplots reuse the same fig.id across cells, so we suffix the cell.
 */
function adaptAxes(figId, cellId, datasets, cfg, axIdx = 0) {
  // Heatmap / imagesc — single z-matrix dataset.
  // After Stage A the engine ships uint8 indices (0..254 + 255 NaN sentinel)
  // plus quantization range (cminOrig/cmaxOrig). The IDE renders via a
  // 256-entry LUT — no float→colour math in the hot path.
  const imgDsIdx = datasets.findIndex((d) => (d.type || '').toLowerCase() === 'imagesc');
  const imgDs = imgDsIdx >= 0 ? datasets[imgDsIdx] : null;
  if (imgDs && imgDs.z) {
    const z = imgDs.z;
    const nR = z.length;
    const nC = z[0]?.length || 0;
    const xr = imgDs.x || [0, Math.max(0, nC - 1)];
    const yr = imgDs.y || [0, Math.max(0, nR - 1)];
    const x0 = xr[0], x1 = xr[xr.length - 1];
    const y0 = yr[0], y1 = yr[yr.length - 1];
    const cW = nC > 1 ? (x1 - x0) / (nC - 1) : 1;
    const cH = nR > 1 ? (y1 - y0) / (nR - 1) : 1;
    // Quantization range from the engine. cmin/cmax are aliases used by
    // existing code paths (status bar, exports, hover) — they reflect the
    // original-data scale so user-visible numbers are correct.
    const cminOrig = (typeof imgDs.cminOrig === 'number') ? imgDs.cminOrig : 0;
    const cmaxOrig = (typeof imgDs.cmaxOrig === 'number') ? imgDs.cmaxOrig : 1;
    return {
      kind: 'heatmap',
      id: cellId,
      title:  cfg.title  || '',
      xLabel: cfg.xlabel || '',
      yLabel: cfg.ylabel || '',
      xRange: [x0 - cW / 2, x1 + cW / 2],
      yRange: [y0 - cH / 2, y1 + cH / 2],
      z,                              // uint8 indices, rows-major 2-D array
      cmin: cminOrig, cmax: cmaxOrig, // aliases for existing UI
      cminOrig, cmaxOrig,
      colorScaleBaked: imgDs.colorScaleBaked || null,   // 'log' | null
      xscale: cfg.xscale || 'linear',                    // 'log' | 'linear'
      yscale: cfg.yscale || 'linear',                    // 'log' | 'linear'
      colormap: cfg.colormap || 'parula',
      downsampled:   imgDs.downsampled === true,
      originalRows:  imgDs.originalRows  || nR,
      originalCols:  imgDs.originalCols  || nC,
      _figId: figId,
      _axIdx: axIdx,
      _dsIdx: imgDsIdx,
    };
  }

  // Polar — cfg.polar=true, datasets carry (theta, rho) as (x, y).
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

  // Line-ish kinds.
  const supported = new Set(['line', 'scatter', 'stem', 'stairs',
    'bar', 'hist', 'semilogx', 'semilogy', 'loglog']);
  const lineish = datasets.filter((d) => supported.has((d.type || 'line').toLowerCase()));
  if (lineish.length === 0) return null;

  const series = lineish.map((d, i) => {
    const xArr = Array.isArray(d.x) ? d.x.map(Number) : [];
    const yArr = Array.isArray(d.y) ? d.y.map(Number) : [];
    const styleObj = typeof d.style === 'string' ? parseLineSpec(d.style)
                   : (d.style || {});
    const color = styleObj.color || d.color || KIND_PALETTE[i % KIND_PALETTE.length];
    const width = d.lineWidth || styleObj.lineWidth || styleObj.width || 1.5;
    const t = (d.type || 'line').toLowerCase();
    let mode = 'line';
    if (t === 'scatter')   mode = 'scatter';
    else if (t === 'stem') mode = 'stem';
    else if (t === 'bar' || t === 'hist') mode = 'bar';
    else if (t === 'stairs') mode = 'stairs';
    return {
      name: d.label || `series ${i + 1}`,
      x: xArr, y: yArr,
      color, width, mode,
      opacity: d.style?.opacity ?? 1,
    };
  });

  let xLo = Infinity, xHi = -Infinity, yLo = Infinity, yHi = -Infinity;
  series.forEach((s) => {
    const [a, b] = rangeFromArr(s.x);
    const [c, d] = rangeFromArr(s.y);
    if (a < xLo) xLo = a;
    if (b > xHi) xHi = b;
    if (c < yLo) yLo = c;
    if (d > yHi) yHi = d;
  });

  const xRange = (Array.isArray(cfg.xlim) && cfg.xlim.length === 2)
    ? cfg.xlim.slice()
    : [Number.isFinite(xLo) ? xLo : -1, Number.isFinite(xHi) ? xHi : 1];
  const yRange = (Array.isArray(cfg.ylim) && cfg.ylim.length === 2)
    ? cfg.ylim.slice()
    : [Number.isFinite(yLo) ? yLo : -1, Number.isFinite(yHi) ? yHi : 1];

  if (!cfg.xlim) {
    const pad = (xRange[1] - xRange[0]) * 0.04 || 0.5;
    xRange[0] -= pad; xRange[1] += pad;
  }
  if (!cfg.ylim) {
    const pad = (yRange[1] - yRange[0]) * 0.06 || 0.5;
    yRange[0] -= pad; yRange[1] += pad;
  }

  return {
    kind: 'line',
    id: cellId,
    title:  cfg.title || '',
    xLabel: cfg.xlabel || '',
    yLabel: cfg.ylabel || '',
    xRange, yRange,
    grid: cfg.grid || '',
    series,
  };
}

/**
 * Convert one engine figure → mockup figure shape. Returns an object with a
 * `kind` field that the caller uses to pick a renderer:
 *   { kind: 'line',    series, ... }              → InteractivePlot
 *   { kind: 'heatmap', z, cmin, cmax, ... }       → Heatmap
 *   { kind: 'polar',   series, thetaDir, ... }    → PolarPlot
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
