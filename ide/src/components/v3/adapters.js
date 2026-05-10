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

// MATLAB-style line spec parsing: 'r--o' → { color, lineStyle, marker }
//   color   — single char from STYLE_COLOR
//   lineStyle — '-' | '--' | ':' | '-.' (longest-match wins)
//   marker  — 'o' | '+' | '*' | '.' | 'x' | 's' | 'd' | '^' | 'v' | '<' | '>' | 'p' | 'h'
// Order in the spec is free (MATLAB tolerates "or", "ro-", "-or", etc.).
const STYLE_COLOR = { r: '#f07070', g: '#6ee7a0', b: '#60d0f0', k: '#d4d4f0', m: '#e070c0', c: '#60d0f0', y: '#e8d060', w: '#ffffff' };
const STYLE_MARKERS = new Set(['o', '+', '*', '.', 'x', 's', 'd', '^', 'v', '<', '>', 'p', 'h']);
function parseLineSpec(s) {
  if (!s || typeof s !== 'string') return {};
  // Two style dialects share this slot:
  //   • Classic MATLAB linespec ("r--o", "b:") — single-char color.
  //   • Engine extras ("color=#rrggbb;lineWidth=2") — explicit kv list.
  // Parse as kv first (it's unambiguous because of the '=' sign), then
  // fall back to a left-to-right longest-match tokeniser.
  const out = {};
  if (s.includes('=')) {
    for (const kv of s.split(';')) {
      const [k, v] = kv.split('=');
      if (!k || v == null) continue;
      const key = k.trim(), val = v.trim();
      if (key === 'color') out.color = val;
      else if (key === 'lineWidth' || key === 'linewidth') out.lineWidth = Number(val);
      else if (key === 'fontSize' || key === 'fontsize') out.fontSize = Number(val);
      else if (key === 'fillOpacity' || key === 'fillopacity') out.fillOpacity = Number(val);
    }
    return out;
  }
  // Left-to-right scan. Try line-style longest-match first (so '--'
  // beats '-' and '-.' beats '-'/'.'); then color; then marker.
  let i = 0;
  while (i < s.length) {
    const c2 = s.substr(i, 2);
    if (!out.lineStyle && (c2 === '--' || c2 === '-.')) {
      out.lineStyle = c2; i += 2; continue;
    }
    const c = s[i];
    if (!out.lineStyle && (c === '-' || c === ':')) {
      out.lineStyle = c; i += 1; continue;
    }
    if (!out.color && STYLE_COLOR[c]) {
      out.color = STYLE_COLOR[c]; i += 1; continue;
    }
    if (!out.marker && STYLE_MARKERS.has(c)) {
      // '.' is ambiguous (line-style '-.' vs marker '.'). The
      // longest-match for '-.' above already eats the line-style
      // form, so a bare '.' here is unambiguously a marker.
      out.marker = c; i += 1; continue;
    }
    // Unknown char — skip silently (MATLAB also ignores stray chars).
    i += 1;
  }
  return out;
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

  if (t === 'image-rgb') {
    if (!d.rgb) return null;
    return {
      kind: 'image-rgb',
      // d.rgb is [[[r,g,b],...],...] row-major. originalRows/Cols from
      // engine. The renderer builds an off-screen canvas + data-URL
      // and embeds it in an SVG <image>.
      rgb: d.rgb,
      nR: d.originalRows || (Array.isArray(d.rgb) ? d.rgb.length : 0),
      nC: d.originalCols || (Array.isArray(d.rgb?.[0]) ? d.rgb[0].length : 0),
      downsampled: d.downsampled === true,
      _figId: ctx.figId,
      _axIdx: ctx.axIdx,
      _dsIdx: ctx.dsIdx,
    };
  }
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
                     'plot3', 'scatter3', 'polygon',
                     'surf', 'bar3', 'waterfall', 'fill3',
                     'quiver3', 'contour3',
                     'xline', 'yline'];
  if (!supported.includes(t)) return null;

  // null is the wire-format "break" marker (JSON forbids NaN). Map it
  // back to NaN here so the line renderer's Number.isFinite() check
  // treats it as a polyline break instead of coercing to 0 (which
  // would join unrelated segments through the origin). Declared
  // EARLY because the surf branch below uses it — putting it after
  // surf's `if` block triggers a TDZ "Cannot access 'a' before
  // initialization" crash when surf data lands.
  const numOrBreak = (v) => (v === null ? NaN : Number(v));

  // surf / bar3 / waterfall — all share the (Xs, Ys, Z[Nr][Nc]) wire
  // shape. Mode picks the renderer's geometry path: triangle mesh
  // for surf, cuboids for bar3, ribbons for waterfall.
  if (t === 'surf' || t === 'bar3' || t === 'waterfall') {
    const Xs = Array.isArray(d.x) ? d.x.map(numOrBreak) : [];
    const Ys = Array.isArray(d.y) ? d.y.map(numOrBreak) : [];
    const Zmat = (Array.isArray(d.z) && Array.isArray(d.z[0]))
                 ? d.z.map((row) => row.map(numOrBreak))
                 : null;
    if (!Zmat || Zmat.length === 0 || !Xs.length || !Ys.length) return null;
    // Flat z[] for bbox + has3D detection. Length == Xs * Ys; xRaw /
    // yRaw expanded to match so computeBBox + buildVertices walk the
    // full grid. Renderer's `mode === 'surface'` branch consumes
    // surfaceGrid and ignores xRaw/yRaw/z.
    const flatX = [];
    const flatY = [];
    const flatZ = [];
    for (let r = 0; r < Ys.length; r++) {
      for (let c = 0; c < Xs.length; c++) {
        flatX.push(Xs[c]);
        flatY.push(Ys[r]);
        flatZ.push(Zmat[r] ? Zmat[r][c] : NaN);
      }
    }
    const modeMap = { surf: 'surface', bar3: 'bar3', waterfall: 'waterfall' };
    const colorMap = { surf: '#4a90b8', bar3: '#5fa7d9', waterfall: '#4a90b8' };
    return {
      kind: 'series',
      mode: modeMap[t],
      name: d.label || `${t} ${palette_idx + 1}`,
      x: flatX,
      y: flatY,
      xRaw: flatX,
      yRaw: flatY,
      z: flatZ,
      surfaceGrid: { Xs, Ys, Z: Zmat },
      color: colorMap[t],
      width: 1,
      size: 3,
      opacity: 1,
      yside: 'left',
      fillOpacity: 1,
    };
  }

  // quiver3 — 3-D vector field. C++ packs w[] inside the style as
  // "wJson=[...]"; parse it back here. Each (x, y, z) is the seed,
  // each (u, v, w) is the displacement.
  if (t === 'quiver3') {
    const xRaw = Array.isArray(d.x) ? d.x.map(numOrBreak) : [];
    const yRaw = Array.isArray(d.y) ? d.y.map(numOrBreak) : [];
    const zRaw = Array.isArray(d.z) && !Array.isArray(d.z[0])
                 ? d.z.map(numOrBreak) : [];
    const u    = Array.isArray(d.u) ? d.u.map(numOrBreak) : [];
    const v    = Array.isArray(d.v) ? d.v.map(numOrBreak) : [];
    let w = [];
    let scale = 1;
    if (typeof d.style === 'string') {
      // Match wJson=[...] with greedy bracket close at the next ;
      const wm = d.style.match(/wJson=(\[[^\]]*\])/);
      if (wm) {
        try { w = JSON.parse(wm[1]).map(numOrBreak); }
        catch (e) { w = []; }
      }
      const sm = d.style.match(/scale=([0-9.eE+-]+)/);
      if (sm) scale = Number(sm[1]);
    }
    if (!xRaw.length || !u.length) return null;
    return {
      kind: 'series',
      mode: 'quiver3',
      name: d.label || `quiver3 ${palette_idx + 1}`,
      x: xRaw, y: yRaw,
      xRaw, yRaw,
      z: zRaw,
      u, v, w,
      scale,
      color: '#9467bd',
      width: 1.5,
      size: 3,
      opacity: 1,
      yside: 'left',
      fillOpacity: 1,
    };
  }

  // contour3 — contour lines drawn at the surface height. Same wire
  // shape as surf (Xs, Ys, Z[Nr][Nc]) plus optional levels in style.
  if (t === 'contour3') {
    const Xs = Array.isArray(d.x) ? d.x.map(numOrBreak) : [];
    const Ys = Array.isArray(d.y) ? d.y.map(numOrBreak) : [];
    const Zmat = (Array.isArray(d.z) && Array.isArray(d.z[0]))
                 ? d.z.map((row) => row.map(numOrBreak))
                 : null;
    if (!Zmat || !Xs.length || !Ys.length) return null;
    let n = 10;
    let levels = null;
    if (typeof d.style === 'string') {
      const lm = d.style.match(/levels=(\[[^\]]*\])/);
      if (lm) {
        try { levels = JSON.parse(lm[1]).map(numOrBreak); }
        catch (e) { levels = null; }
      }
      const nm = d.style.match(/n=(\d+)/);
      if (nm) n = Number(nm[1]);
    }
    // Flatten for has3D detection.
    const flatZ = [];
    const flatX = [];
    const flatY = [];
    for (let r = 0; r < Ys.length; r++) {
      for (let c = 0; c < Xs.length; c++) {
        flatX.push(Xs[c]);
        flatY.push(Ys[r]);
        flatZ.push(Zmat[r] ? Zmat[r][c] : NaN);
      }
    }
    return {
      kind: 'series',
      mode: 'contour3',
      name: d.label || `contour3 ${palette_idx + 1}`,
      x: flatX, y: flatY,
      xRaw: flatX, yRaw: flatY,
      z: flatZ,
      surfaceGrid: { Xs, Ys, Z: Zmat },
      levels, n,
      color: '#1f77b4',
      width: 1.5,
      size: 3,
      opacity: 1,
      yside: 'left',
      fillOpacity: 1,
    };
  }

  // fill3 — multi-polygon 3-D filled shapes. Wire format: x/y/z
  // arrays with null separators between polygon groups (parallel to
  // the 2-D polygon path, plus z). Optional `vertexColors` carries
  // a flat [r,g,b,r,g,b,...] uint8 list parallel to the finite
  // (x, y, z) samples — null separators do NOT consume colour
  // entries.
  if (t === 'fill3') {
    const xRaw = Array.isArray(d.x) ? d.x.map(numOrBreak) : [];
    const yRaw = Array.isArray(d.y) ? d.y.map(numOrBreak) : [];
    const zRaw = Array.isArray(d.z) && !Array.isArray(d.z[0])
                 ? d.z.map(numOrBreak) : [];
    if (!xRaw.length || !yRaw.length || !zRaw.length) return null;
    const styleObj2 = typeof d.style === 'string' ? parseLineSpec(d.style) : (d.style || {});
    return {
      kind: 'series',
      mode: 'polygon3d',
      name: d.label || `fill3 ${palette_idx + 1}`,
      x: xRaw, y: yRaw,
      xRaw, yRaw,
      z: zRaw,
      vertexColors: Array.isArray(d.vertexColors) ? d.vertexColors : null,
      color: styleObj2.color || '#9467bd',
      width: 1,
      size: 3,
      opacity: 1,
      yside: 'left',
      fillOpacity: styleObj2.fillOpacity != null ? styleObj2.fillOpacity : 0.7,
    };
  }
  const x = Array.isArray(d.x) ? d.x.map(numOrBreak) : [];
  const y = Array.isArray(d.y) ? d.y.map(numOrBreak) : [];
  let mode = 'line';
  if (t === 'scatter') mode = 'scatter';
  else if (t === 'stem') mode = 'stem';
  else if (t === 'bar' || t === 'hist') mode = 'bar';
  else if (t === 'barh') mode = 'barh';
  else if (t === 'stairs') mode = 'stairs';
  else if (t === 'errorbar') mode = 'errorbar';
  else if (t === 'area') mode = 'area';
  else if (t === 'quiver') mode = 'quiver';
  else if (t === 'polygon') mode = 'polygon';
  else if (t === 'plot3') mode = 'line';
  else if (t === 'scatter3') mode = 'scatter';
  else if (t === 'xline') mode = 'xline';
  else if (t === 'yline') mode = 'yline';

  // 3-D projection (plot3 / scatter3): cabinet projection collapses
  // (x, y, z) → 2-D screen coords by adding z*scale*cos(α) to x and
  // z*scale*sin(α) to y. α = 30°, scale = 0.5 — standard cabinet
  // values that give a readable depth cue without distorting the
  // x-y plane. Fully 3-D camera (orbit / dolly) is B3 territory.
  // Capture raw (pre-cabinet) x/y so the WebGL renderer can use the
  // un-projected coordinates. The 2-D composite path uses xOut / yOut
  // (cabinet-applied below for plot3/scatter3).
  const xRaw = x, yRaw = y;
  let xOut = x, yOut = y;
  if (t === 'plot3' || t === 'scatter3') {
    // For plot3/scatter3, d.z is a 1-D vector (different shape from
    // imagesc's 2-D matrix; disambiguated by the type field).
    const z = Array.isArray(d.z) && !Array.isArray(d.z[0])
      ? d.z.map(numOrBreak) : [];
    const zScale = 0.5;
    const cosA = Math.cos(Math.PI / 6);  // 30°
    const sinA = Math.sin(Math.PI / 6);
    // Cabinet shift. NaN in z (segment break) → NaN in screen coords;
    // missing z (shorter array than x/y) → 0 so plot3 with implicit
    // z=0 still falls in the x-y plane.
    const zAt = (i) => (i < z.length ? z[i] : 0);
    xOut = x.map((xi, i) => xi + zAt(i) * zScale * cosA);
    yOut = y.map((yi, i) => yi + zAt(i) * zScale * sinA);
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
    // Linespec extras parsed from styleObj. lineStyle drives strokeDasharray
    // in CompositePlot; marker triggers an overlay layer of point glyphs
    // along the line. mode === 'scatter' ignores lineStyle (no path drawn).
    lineStyle: styleObj.lineStyle || '-',
    marker: styleObj.marker || (t === 'scatter' ? 'o' : null),
    width: d.lineWidth || styleObj.lineWidth || styleObj.width || 1.5,
    size:  d.markerSize || styleObj.markerSize || 3,
    opacity: d.style?.opacity ?? 1,
    // yyaxis routing. 'left' (default) or 'right' — empty/missing on
    // single-axis figures, never serialised then. Renderer picks sy or
    // sy2 based on this.
    yside: d.yside || 'left',
    // Polygon fill opacity (mode='polygon'). Driven by the styleObj
    // 'fillOpacity' key parsed from the engine's style string.
    fillOpacity: styleObj.fillOpacity != null ? styleObj.fillOpacity : 0.7,
    // Raw 3-D z (pre-cabinet) so the WebGL renderer can use the real
    // depth when this figure is routed through composite3d. The 2-D
    // composite path ignores `z` and uses the cabinet-projected
    // xOut / yOut above.
    z: (t === 'plot3' || t === 'scatter3')
       ? (Array.isArray(d.z) && !Array.isArray(d.z[0]) ? d.z.map(numOrBreak) : null)
       : null,
    // Raw (un-cabinet) x/y for the 3-D renderer. Same array as xOut/
    // yOut above for non-3-D types — duplicated so the WebGL path
    // doesn't have to special-case which fields are pre-projected.
    xRaw, yRaw,
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
      // Polar mode: 'line' (default), 'scatter' (markers only),
      // 'bar' (radial wedges from origin to rho). Driven by ds.type
      // which the C++ side stamps as 'scatter' for polarscatter and
      // 'bar' for polarhistogram.
      let mode = 'line';
      const t = (d.type || '').toLowerCase();
      if (t === 'scatter') mode = 'scatter';
      else if (t === 'bar') mode = 'bar';
      return {
        name: d.label || `series ${i + 1}`,
        mode,
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
      // thetalim limits the angular sweep. Numeric form: [thetaMin,
      // thetaMax] in DEGREES (MATLAB convention). null = full
      // 360° sweep (default).
      thetalim: Array.isArray(cfg.thetalim) && cfg.thetalim.length === 2
        ? cfg.thetalim.slice() : null,
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
      const [a, b] = rangeFromArr(xs);
      const [c, d] = rangeFromArr(ys);
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
    if (!cfg.xlim && !tight) {
      const pad = (xRange[1] - xRange[0]) * 0.04 || 0.5;
      xRange[0] -= pad; xRange[1] += pad;
    }
    if (!cfg.ylim && !tight) {
      const pad = (yRange[1] - yRange[0]) * 0.06 || 0.5;
      yRange[0] -= pad; yRange[1] += pad;
    }
    if (cfg.yyEnabled && !cfg.ylim2 && !tight) {
      const pad = (yRange2[1] - yRange2[0]) * 0.06 || 0.5;
      yRange2[0] -= pad; yRange2[1] += pad;
    }
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
    // Custom tick positions / labels (xticks / yticks / xticklabels /
    // yticklabels). Empty arrays → renderer falls back to niceTicks
    // auto-generation. Labels are honoured only when their length
    // matches the corresponding ticks count.
    xTicks: Array.isArray(cfg.xticks) ? cfg.xticks.slice() : null,
    yTicks: Array.isArray(cfg.yticks) ? cfg.yticks.slice() : null,
    xTickLabels: Array.isArray(cfg.xticklabels) ? cfg.xticklabels.slice() : null,
    yTickLabels: Array.isArray(cfg.yticklabels) ? cfg.yticklabels.slice() : null,
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
  if (!adapted.title) adapted.title = `Figure ${fig.id}`;
  adapted._raw = fig;
  return adapted;
}

export function adaptFigures(figs) {
  if (!Array.isArray(figs)) return [];
  return figs.map(adaptFigure).filter(Boolean);
}
