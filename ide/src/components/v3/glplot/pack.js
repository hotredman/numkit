// glplot/pack.js — pack a line / scatter series into a GPU-ready interleaved
// Float32 buffer.
//
// Non-finite points (NaN / Inf — MATLAB gap semantics) split the series into
// separate segments: a WebGL line strip can't contain a gap, so each run of
// finite points is drawn as its own strip. Returns the packed finite
// vertices plus the per-segment vertex ranges.
//
//   packXY(x, y) → { data: Float32Array, count, segments: [{ offset, count }] }
//     data:     [x0,y0, x1,y1, …] finite points only, in order
//     count:    number of finite vertices
//     segments: vertex ranges (offset = first vertex, count = vertices) — one
//               per gap-free run; line draws use these as strip boundaries,
//               scatter ignores them (every vertex is a point).

export function packXY(x, y) {
  const n = Math.min(x ? x.length : 0, y ? y.length : 0);
  const data = new Float32Array(n * 2);
  const segments = [];
  let v = 0;            // finite-vertex write cursor
  let segStart = -1;    // first vertex of the current run
  for (let i = 0; i < n; i++) {
    const xi = x[i], yi = y[i];
    if (Number.isFinite(xi) && Number.isFinite(yi)) {
      data[v * 2] = xi;
      data[v * 2 + 1] = yi;
      if (segStart < 0) segStart = v;
      v++;
    } else if (segStart >= 0) {
      segments.push({ offset: segStart, count: v - segStart });
      segStart = -1;
    }
  }
  if (segStart >= 0) segments.push({ offset: segStart, count: v - segStart });
  return {
    data: v === n ? data : data.subarray(0, v * 2),
    count: v,
    segments,
  };
}
