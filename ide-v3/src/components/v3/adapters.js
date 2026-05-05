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
 * Convert one engine figure → mockup figure shape. Returns null if the
 * figure has no plottable line/scatter datasets (e.g. pure heatmap).
 */
export function adaptFigure(fig) {
  if (!fig) return null;
  const cfg = fig.config || {};
  const datasets = Array.isArray(fig.datasets) ? fig.datasets : [];

  // Pull only line / scatter / stem-style datasets — those map cleanly to
  // the mockup's series shape. Everything else (heatmap, bar, etc.) is
  // skipped here; the caller can decide to fall back to legacy rendering.
  const lineish = datasets.filter((d) => {
    const t = (d.type || 'line').toLowerCase();
    return t === 'line' || t === 'scatter' || t === 'stem' || t === 'stairs'
        || t === 'semilogx' || t === 'semilogy' || t === 'loglog';
  });
  if (lineish.length === 0) return null;

  const series = lineish.map((d, i) => {
    const xArr = Array.isArray(d.x) ? d.x.map(Number) : [];
    const yArr = Array.isArray(d.y) ? d.y.map(Number) : [];
    const color = d.style?.color || d.color || KIND_PALETTE[i % KIND_PALETTE.length];
    const width = d.style?.lineWidth || d.style?.width || 1.5;
    return {
      name: d.label || `series ${i + 1}`,
      x: xArr,
      y: yArr,
      color,
      width,
      opacity: d.style?.opacity ?? 1,
    };
  });

  // Aggregate ranges across all series so a default fit shows everything
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

  // 4% padding on auto-ranges so curves don't touch axes
  if (!cfg.xlim) {
    const pad = (xRange[1] - xRange[0]) * 0.04 || 0.5;
    xRange[0] -= pad; xRange[1] += pad;
  }
  if (!cfg.ylim) {
    const pad = (yRange[1] - yRange[0]) * 0.06 || 0.5;
    yRange[0] -= pad; yRange[1] += pad;
  }

  return {
    id: fig.id,
    title:  cfg.title || `Figure ${fig.id}`,
    xLabel: cfg.xlabel || '',
    yLabel: cfg.ylabel || '',
    xRange, yRange,
    series,
    /* Pass through original config so the modal's status bar / fallback can
       inspect grid / legend settings if needed. */
    _raw: fig,
  };
}

export function adaptFigures(figs) {
  if (!Array.isArray(figs)) return [];
  return figs.map(adaptFigure).filter(Boolean);
}
