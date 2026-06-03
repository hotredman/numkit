// decimate.js — viewport line-series downsampling for fast Plot rendering.
//
// A plot is only ~W pixels wide, so a series of N≫W points wastes work:
// thousands of samples collapse into one pixel column. We downsample the
// VISIBLE x-range to O(W) points before building the SVG path — the render
// cost drops from O(N) to O(W) while staying visually faithful.
//
// Algorithms (universal Plot, not audio-specific):
//   'm4'   — per pixel-column keep {first, min, max, last}. Pixel-faithful:
//            smooth data → a thin line; spikes and the true oscillation
//            extent are preserved (what MATLAB does for big lines). Default.
//   'lttb' — Largest-Triangle-Three-Buckets: smoother for trends, ~1 pt per
//            column, but can hide narrow spikes. Opt-in.
//   'none' — no decimation (raw slice of the visible range).
//
// x is assumed ascending (plot(y) → 1..N; plot(x,y) with sorted x); buckets
// are by x-position so they line up with pixel columns. A non-finite y
// (NaN/Inf) is a gap; it is preserved when it lands on a bucket endpoint.

// First index i with x[i] >= target (x ascending). Returns x.length if none.
function lowerBound(x, target) {
  let lo = 0, hi = x.length;
  while (lo < hi) {
    const mid = (lo + hi) >> 1;
    if (x[mid] < target) lo = mid + 1; else hi = mid;
  }
  return lo;
}

// Index range [i0, i1) covering the visible x-window, padded by one sample
// each side so the line enters/leaves the viewport instead of stopping at
// the edge. Clamped to [0, n].
export function visibleRange(x, x0, x1) {
  const n = x.length;
  if (n === 0) return [0, 0];
  let i0 = lowerBound(x, x0);
  let i1 = lowerBound(x, x1);
  if (i0 > 0) i0 -= 1;            // one point left of the window
  if (i1 < n) i1 += 1;           // one point right of the window
  if (i1 > n) i1 = n;
  if (i0 < 0) i0 = 0;
  if (i1 < i0) i1 = i0;
  return [i0, i1];
}

// M4: {first, min, max, last} per pixel column over the visible range.
// Returns { x:[], y:[] } with at most 4*width points, in x order.
export function decimateM4(x, y, x0, x1, width) {
  const [i0, i1] = visibleRange(x, x0, x1);
  const outX = [], outY = [];
  if (i1 <= i0) return { x: outX, y: outY };
  const cols = Math.max(1, width | 0);
  const span = (x1 - x0) || 1;
  const bucketOf = (xv) => {
    let b = Math.floor(((xv - x0) / span) * cols);
    if (b < 0) b = 0; else if (b >= cols) b = cols - 1;
    return b;
  };

  let curBucket = -1;
  let bFirst = i0, bLast = i0, bMinI = i0, bMaxI = i0, bMinY = Infinity, bMaxY = -Infinity;
  const flush = () => {
    if (curBucket < 0) return;
    // first / min / max / last in index (x) order, de-duplicated.
    const idxs = [bFirst, bMinI, bMaxI, bLast].sort((a, b) => a - b);
    let prev = -1;
    for (const ii of idxs) {
      if (ii !== prev) { outX.push(x[ii]); outY.push(y[ii]); prev = ii; }
    }
  };

  for (let i = i0; i < i1; i++) {
    const xv = x[i], yv = y[i];
    const b = bucketOf(xv);
    if (b !== curBucket) {
      flush();
      curBucket = b;
      bFirst = i; bLast = i; bMinI = i; bMaxI = i;
      bMinY = Number.isFinite(yv) ? yv : Infinity;
      bMaxY = Number.isFinite(yv) ? yv : -Infinity;
    } else {
      bLast = i;
      if (Number.isFinite(yv)) {
        if (yv < bMinY) { bMinY = yv; bMinI = i; }
        if (yv > bMaxY) { bMaxY = yv; bMaxI = i; }
      }
    }
  }
  flush();
  return { x: outX, y: outY };
}

// LTTB: ~`threshold` points preserving visual shape (smooth trends).
export function decimateLTTB(x, y, x0, x1, threshold) {
  const [i0, i1] = visibleRange(x, x0, x1);
  const n = i1 - i0;
  const outX = [], outY = [];
  if (n <= 0) return { x: outX, y: outY };
  if (threshold >= n || threshold < 3) {
    for (let i = i0; i < i1; i++) { outX.push(x[i]); outY.push(y[i]); }
    return { x: outX, y: outY };
  }
  const bucketSize = (n - 2) / (threshold - 2);
  let a = i0;                              // first point is always kept
  outX.push(x[i0]); outY.push(y[i0]);
  for (let i = 0; i < threshold - 2; i++) {
    // Average of the NEXT bucket (the third "triangle" vertex).
    let avgStart = i0 + Math.floor((i + 1) * bucketSize) + 1;
    let avgEnd = i0 + Math.floor((i + 2) * bucketSize) + 1;
    if (avgEnd > i1) avgEnd = i1;
    let avgX = 0, avgY = 0;
    const avgN = Math.max(1, avgEnd - avgStart);
    for (let j = avgStart; j < avgEnd; j++) { avgX += x[j]; avgY += y[j]; }
    avgX /= avgN; avgY /= avgN;
    // Pick the point in THIS bucket with the largest triangle area.
    const rangeStart = i0 + Math.floor(i * bucketSize) + 1;
    const rangeEnd = i0 + Math.floor((i + 1) * bucketSize) + 1;
    const ax = x[a], ay = y[a];
    let maxArea = -1, maxIdx = rangeStart;
    for (let j = rangeStart; j < rangeEnd && j < i1; j++) {
      const area = Math.abs((ax - avgX) * (y[j] - ay) - (ax - x[j]) * (avgY - ay));
      if (area > maxArea) { maxArea = area; maxIdx = j; }
    }
    outX.push(x[maxIdx]); outY.push(y[maxIdx]);
    a = maxIdx;
  }
  outX.push(x[i1 - 1]); outY.push(y[i1 - 1]);
  return { x: outX, y: outY };
}

// Dispatcher. Returns the raw visible slice when there's nothing to gain
// (visible count already ≤ 2·width) or algo === 'none'. `decimated` tells
// the caller whether downsampling actually happened. width is the plot's
// pixel width.
export function decimateSeries(x, y, x0, x1, width, algo = 'm4') {
  const w = Math.max(1, width | 0);
  const [i0, i1] = visibleRange(x, x0, x1);
  const n = i1 - i0;
  if (algo === 'none' || n <= w * 2) {
    const ox = [], oy = [];
    for (let i = i0; i < i1; i++) { ox.push(x[i]); oy.push(y[i]); }
    return { x: ox, y: oy, decimated: false, n };
  }
  const out = algo === 'lttb'
    ? decimateLTTB(x, y, x0, x1, w)
    : decimateM4(x, y, x0, x1, w);
  return { x: out.x, y: out.y, decimated: true, n };
}
