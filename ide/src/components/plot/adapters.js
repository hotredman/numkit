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
 *
 * The bulky dataset->layer and axes adapters live in sibling modules.
 */
import { adaptAxes } from './adapters.axes';


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

export function classify(size, type) {
  // Container types are classified by type, not shape — a 1×1 struct
  // is still a struct, not a scalar. The Variable Editor gates its
  // nested tree-view on these.
  if (type === 'struct') return 'struct';
  if (type === 'cell')   return 'cell';
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
    // Full stat set from the engine (min/max/mean/median/mode/var/std) when
    // present; otherwise a preview-derived partial (min/max/mean only) so
    // the ValueTable still populates those columns on older WASM builds.
    const engineStats = (isStruct && raw.stats && typeof raw.stats === 'object')
      ? raw.stats : null;
    const fallbackStats = Number.isFinite(stats.min)
      ? { min: stats.min, max: stats.max, mean: stats.mean } : null;

    out.push({
      name,
      type, kind,
      size: sizeNorm,
      bytes,
      data,
      preview: previewString(preview, kind, sizeNorm, type),
      min: stats.min, max: stats.max, mean: stats.mean,
      stats: engineStats || fallbackStats,
    });
  }
  return out;
}

/* ─────────────── figures ─────────────── */


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
      // Figure-level super-title (sgtitle), separate from per-cell
      // title. Rendered as a header strip above the cell grid in
      // SubplotGrid. Empty / missing = no strip drawn.
      superTitle: fig.superTitle || '',
      grid: [rows, cols],
      cells,
      // linkaxes mode (figure-level state). 'x'/'y'/'xy' = SubplotGrid
      // mirrors viewport changes across cells on those axes. Empty =
      // each cell pans/zooms independently (the default).
      linkMode: fig.linkMode || '',
      _raw: fig,
    };
  }

  // Single-axes figure — adapt directly.
  const { datasets, cfg } = flatten(fig);
  const adapted = adaptAxes(fig.id, fig.id, datasets, cfg);
  if (!adapted) return null;
  // Auto-substitute a default heading when the script didn't call title().
  // Track this in titleAuto so display ▾ can keep its "title" toggle
  // disabled — the user only cares about toggling user-set titles.
  if (!adapted.title) {
    adapted.title = `Figure ${fig.id}`;
    adapted.titleAuto = true;
  }
  // sgtitle on a single-axes figure: still expose the figure-level
  // superTitle so the modal header strip can render it (separate from
  // the axes' own title text).
  adapted.superTitle = fig.superTitle || '';
  adapted._raw = fig;
  return adapted;
}

export function adaptFigures(figs) {
  if (!Array.isArray(figs)) return [];
  return figs.map(adaptFigure).filter(Boolean);
}
