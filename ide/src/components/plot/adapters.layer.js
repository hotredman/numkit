// adapters.layer.js — datasetToLayer: one engine dataset -> one plot layer.
import { parseLineSpec, KIND_PALETTE } from './adapters.linespec';

/**
 * Convert one engine dataset into a CompositePlot layer object. Returns
 * null for unsupported types. Layers carry data in original-data coords;
 * the renderer maps them through current sx/sy at draw time.
 */
export function datasetToLayer(d, palette_idx, ctx) {
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
      smoothNormals: !!styleObj2.smoothNormals,
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
    //
    // MATLAB linespec rule: if the user provided a marker glyph but NO
    // explicit line style (i.e. `plot(x, y, 'r+')` not `'r-+'`), draw
    // markers ONLY — no connecting line. Forcing lineStyle='-' on every
    // series broke this: stray scatter-style calls came out as a zigzag
    // polyline through arbitrary point order. Use 'none' to suppress the
    // path render; lineStyle='-' default still applies when no marker.
    lineStyle: styleObj.lineStyle || (styleObj.marker ? 'none' : '-'),
    marker: styleObj.marker || (t === 'scatter' ? 'o' : null),
    // MATLAB markers are open by default; scatter(...,'filled') / MarkerFaceColor
    // fill them. The engine emits filled=1 in the style string when set.
    filled: !!styleObj.filled,
    // comet animation hint — when true, CompositePlot animates the
    // polyline progressively via RAF on first mount.
    cometAnim: !!styleObj.cometAnim,
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
    // Phase 2c — engine-downsampled huge series. x/y above is a small M4
    // preview; the full data lives in the engine. CompositePlot refetches a
    // decimated viewport tile on zoom via
    // engine.getSeriesTile(figId, axIdx, dsIdx, x0, x1, width, algo).
    seriesDownsampled: !!d.seriesDownsampled,
    seriesN: d.n || 0,
    seriesXRange: Array.isArray(d.xRange) ? d.xRange : null,
    figId: ctx?.figId, axIdx: ctx?.axIdx, dsIdx: ctx?.dsIdx,
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
