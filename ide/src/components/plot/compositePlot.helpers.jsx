// compositePlot.helpers.jsx — pure helpers for CompositePlot: the GL
// routing threshold, MATLAB line-spec dash map, the SVG marker-glyph
// dispatcher, and series-name resolution. No hooks / no component state.

// Route a line/stairs layer to the WebGL overlay once it has more than this
// many points (and the full data lives in JS — downsampled previews stay on
// the existing path until the binary-transport layer feeds the GPU).
export const GL_MIN_POINTS = 50000;

// MATLAB linespec → SVG strokeDasharray. '-' (or absent) means solid;
// returning undefined keeps the default solid stroke. Pixel patterns
// roughly match MATLAB defaults (LineStyle '--' ≈ 4 px on / 4 px off).
export const DASH_FOR = { '-': undefined, '--': '6,4', ':': '1,3', '-.': '6,3,1,3' };

// SVG marker glyph dispatcher used by both line-overlay and scatter
// modes. r is the half-size in pixels (matches MATLAB MarkerSize ≈ r).
export function MarkerGlyph({ marker, cx, cy, r, color, idx, filled = false }) {
  // MATLAB markers are OPEN (outline in the series colour) by default; `filled`
  // fills them. '+'/'x'/'*' are always strokes; '.' is a small filled dot.
  const fillC = filled ? color : 'none';
  const sw = filled ? 0.6 : 1.4;
  switch (marker) {
    case 'o':
      return <circle key={idx} cx={cx} cy={cy} r={r} fill={fillC} stroke={color} strokeWidth={sw} />;
    case 's':
      return <rect key={idx} x={cx - r} y={cy - r} width={r * 2} height={r * 2}
        fill={fillC} stroke={color} strokeWidth={sw} />;
    case 'd':
      return <path key={idx} d={`M${cx},${cy - r} L${cx + r},${cy} L${cx},${cy + r} L${cx - r},${cy} Z`}
        fill={fillC} stroke={color} strokeWidth={sw} />;
    case '^':
      return <path key={idx} d={`M${cx},${cy - r} L${cx + r},${cy + r} L${cx - r},${cy + r} Z`}
        fill={fillC} stroke={color} strokeWidth={sw} />;
    case 'v':
      return <path key={idx} d={`M${cx},${cy + r} L${cx + r},${cy - r} L${cx - r},${cy - r} Z`}
        fill={fillC} stroke={color} strokeWidth={sw} />;
    case '<':
      return <path key={idx} d={`M${cx - r},${cy} L${cx + r},${cy - r} L${cx + r},${cy + r} Z`}
        fill={fillC} stroke={color} strokeWidth={sw} />;
    case '>':
      return <path key={idx} d={`M${cx + r},${cy} L${cx - r},${cy - r} L${cx - r},${cy + r} Z`}
        fill={fillC} stroke={color} strokeWidth={sw} />;
    case 'p': {  // pentagon (5-pointed) — approximate
      const pts = [];
      for (let k = 0; k < 5; k++) {
        const a = -Math.PI / 2 + k * (2 * Math.PI / 5);
        pts.push(`${cx + r * Math.cos(a)},${cy + r * Math.sin(a)}`);
      }
      return <polygon key={idx} points={pts.join(' ')}
        fill={fillC} stroke={color} strokeWidth={sw} />;
    }
    case 'h': {  // hexagon
      const pts = [];
      for (let k = 0; k < 6; k++) {
        const a = k * (Math.PI / 3);
        pts.push(`${cx + r * Math.cos(a)},${cy + r * Math.sin(a)}`);
      }
      return <polygon key={idx} points={pts.join(' ')}
        fill={fillC} stroke={color} strokeWidth={sw} />;
    }
    case '+':
      return (
        <g key={idx}>
          <line x1={cx - r} y1={cy} x2={cx + r} y2={cy} stroke={color} strokeWidth={1.5} />
          <line x1={cx} y1={cy - r} x2={cx} y2={cy + r} stroke={color} strokeWidth={1.5} />
        </g>
      );
    case 'x':
      return (
        <g key={idx}>
          <line x1={cx - r} y1={cy - r} x2={cx + r} y2={cy + r} stroke={color} strokeWidth={1.5} />
          <line x1={cx - r} y1={cy + r} x2={cx + r} y2={cy - r} stroke={color} strokeWidth={1.5} />
        </g>
      );
    case '*':
      return (
        <g key={idx}>
          <line x1={cx - r} y1={cy} x2={cx + r} y2={cy} stroke={color} strokeWidth={1.2} />
          <line x1={cx} y1={cy - r} x2={cx} y2={cy + r} stroke={color} strokeWidth={1.2} />
          <line x1={cx - r * 0.7} y1={cy - r * 0.7} x2={cx + r * 0.7} y2={cy + r * 0.7} stroke={color} strokeWidth={1.2} />
          <line x1={cx - r * 0.7} y1={cy + r * 0.7} x2={cx + r * 0.7} y2={cy - r * 0.7} stroke={color} strokeWidth={1.2} />
        </g>
      );
    case '.':
      return <circle key={idx} cx={cx} cy={cy} r={Math.max(1, r * 0.4)}
        fill={color} stroke="none" />;
    default:
      return <circle key={idx} cx={cx} cy={cy} r={r} fill={fillC} stroke={color} strokeWidth={sw} />;
  }
}

/** Resolve the display name for one series layer. Priority matches
 *  MATLAB / the legend block:
 *    1. `figure.legend[idx]`  — the i-th label passed to legend(...)
 *    2. `layer.name`          — the script-set DisplayName on the layer
 *    3. `series ${idx+1}`     — guaranteed-non-empty fallback
 *  Whitespace-only legend strings are treated as empty (fall through).
 *  Used by both the in-figure legend block AND the ПКМ Fit Series ▶
 *  rows so they always agree on what to call each curve. */
export function resolveSeriesName(figure, layer, idx) {
  const fromLegend = ((figure.legend && figure.legend[idx]) || '').trim();
  return fromLegend || layer.name || `series ${idx + 1}`;
}

