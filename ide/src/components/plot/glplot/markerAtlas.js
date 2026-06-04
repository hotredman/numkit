// glplot/markerAtlas.js — a horizontal atlas of marker shapes (white-on-
// transparent alpha masks) drawn on a Canvas2D, matching the SVG MarkerGlyph
// set. The GL point shader samples the cell for a series' marker and tints it
// with the series colour, so GL scatter / polar / line markers render every
// MATLAB marker shape — not just the disc — and match the SVG fallback.
//
// One small texture covers all shapes; the shape→cell map is a pure,
// unit-tested function. Atlas building needs a real Canvas2D (verified live;
// jsdom has no 2-D context) and returns null when unavailable.

// Column order == cell index. Unknown / round markers fall back to the disc.
export const MARKERS = ['o', 's', 'd', '^', 'v', '<', '>', 'p', 'h', '+', 'x'];

// The shape is drawn at this fraction of the cell radius. The GL renderer maps
// gl_PointCoord (the full cell) → the sprite, so the *visible* marker radius is
// ATLAS_R_FRAC × (pointSize/2). The renderer sets pointSize = size·dpr /
// ATLAS_R_FRAC so a GL marker is the same on-screen size as the SVG MarkerGlyph
// (radius `size`) — markers look identical either side of the SVG↔GL threshold.
export const ATLAS_R_FRAC = 0.38;

// marker char → atlas column. '.' and null/undefined → disc (0).
export function markerCell(marker) {
  if (marker === '.' || marker == null) return 0;
  const i = MARKERS.indexOf(marker);
  return i < 0 ? 0 : i;
}

function poly(ctx, cx, cy, pts) {
  ctx.beginPath();
  pts.forEach(([dx, dy], k) => (k ? ctx.lineTo(cx + dx, cy + dy) : ctx.moveTo(cx + dx, cy + dy)));
  ctx.closePath();
  ctx.fill();
}

function ngon(ctx, cx, cy, r, n, rot) {
  ctx.beginPath();
  for (let k = 0; k < n; k++) {
    const a = rot + (k * 2 * Math.PI) / n;
    const x = cx + r * Math.cos(a);
    const y = cy + r * Math.sin(a);
    if (k) ctx.lineTo(x, y); else ctx.moveTo(x, y);
  }
  ctx.closePath();
  ctx.fill();
}

// Trace a closed shape's path (no fill/stroke yet) so the caller picks.
function shapePath(ctx, m, cx, cy, r) {
  switch (m) {
    case 'o': ctx.beginPath(); ctx.arc(cx, cy, r, 0, 2 * Math.PI); return true;
    case 's': ctx.beginPath(); ctx.rect(cx - r, cy - r, 2 * r, 2 * r); return true;
    case 'd': poly(ctx, cx, cy, [[0, -r], [r, 0], [0, r], [-r, 0]]); return true;
    case '^': poly(ctx, cx, cy, [[0, -r], [r, r], [-r, r]]); return true;
    case 'v': poly(ctx, cx, cy, [[0, r], [r, -r], [-r, -r]]); return true;
    case '<': poly(ctx, cx, cy, [[-r, 0], [r, -r], [r, r]]); return true;
    case '>': poly(ctx, cx, cy, [[r, 0], [-r, -r], [-r, r]]); return true;
    case 'p': ngon(ctx, cx, cy, r, 5, -Math.PI / 2); return true;
    case 'h': ngon(ctx, cx, cy, r, 6, 0); return true;
    default: ctx.beginPath(); ctx.arc(cx, cy, r, 0, 2 * Math.PI); return true;
  }
}

// Draw one marker centred at (cx, cy), half-size r. Mirrors MarkerGlyph: closed
// shapes fill (filled row) or stroke an outline (open row, the MATLAB default);
// '+'/'x' are always strokes. Screen y is down, so '^' (up) has its apex at -y.
function drawMarker(ctx, m, cx, cy, r, filled) {
  if (m === '+') {
    ctx.beginPath();
    ctx.moveTo(cx - r, cy); ctx.lineTo(cx + r, cy);
    ctx.moveTo(cx, cy - r); ctx.lineTo(cx, cy + r);
    ctx.stroke();
    return;
  }
  if (m === 'x') {
    ctx.beginPath();
    ctx.moveTo(cx - r, cy - r); ctx.lineTo(cx + r, cy + r);
    ctx.moveTo(cx - r, cy + r); ctx.lineTo(cx + r, cy - r);
    ctx.stroke();
    return;
  }
  shapePath(ctx, m, cx, cy, r);
  if (filled) ctx.fill(); else ctx.stroke();
}

// Build the atlas canvas: 2 rows (row 0 = open/outline, row 1 = filled) ×
// MARKERS columns. → { canvas, cols, rows, cell } | null (no 2-D context).
export function buildMarkerAtlas(cell = 64) {
  if (typeof document === 'undefined') return null;
  const cols = MARKERS.length;
  const rows = 2;
  const canvas = document.createElement('canvas');
  canvas.width = cell * cols;
  canvas.height = cell * rows;
  const ctx = canvas.getContext('2d');
  if (!ctx) return null;
  ctx.fillStyle = '#fff';
  ctx.strokeStyle = '#fff';
  ctx.lineWidth = Math.max(2, cell * 0.16);   // outline ≈ SVG strokeWidth at render size
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';
  const r = cell * ATLAS_R_FRAC;   // visible radius; renderer scales pointSize to match SVG
  for (let row = 0; row < rows; row++) {       // 0 = open, 1 = filled
    const cy = row * cell + cell / 2;
    for (let i = 0; i < cols; i++) {
      drawMarker(ctx, MARKERS[i], i * cell + cell / 2, cy, r, row === 1);
    }
  }
  return { canvas, cols, rows, cell };
}
