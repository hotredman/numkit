// glplot/projection.js — pure data → clip-space projection.
//
// The WebGL canvas covers exactly the plot rectangle, so a data point maps
// straight to clip space [-1, 1]². Axes are independent (no rotation):
//
//   clipX = ax · tx(x) + bx        clipY = ay · ty(y) + by
//
// where tx/ty is identity (linear) or natural log. xMin maps to clip x = -1,
// xMax to +1 (reversed axes flip the sign). WebGL clip-y points up, so yMin
// sits at clip y = -1 (the canvas covers the plot rect; flipping the canvas
// to screen orientation is the GL layer's job, not this math's).
//
// Pure + framework-free → fully unit-tested; the vertex shader consumes
// {ax, bx, ay, by, xLog, yLog} as uniforms.

// Build the projection for a viewport. Reversed/log flags optional.
export function makeProjection({
  xMin, xMax, yMin, yMax,
  xLog = false, yLog = false,
  xRev = false, yRev = false,
}) {
  const x0 = xLog ? Math.log(xMin) : xMin;
  const x1 = xLog ? Math.log(xMax) : xMax;
  const y0 = yLog ? Math.log(yMin) : yMin;
  const y1 = yLog ? Math.log(yMax) : yMax;

  // Guard a degenerate range so we never divide by zero (returns a finite,
  // if uninteresting, mapping rather than NaN/Inf).
  const sx = x1 !== x0 ? 2 / (x1 - x0) : 1;
  const sy = y1 !== y0 ? 2 / (y1 - y0) : 1;
  let ax = sx, bx = -1 - sx * x0;
  let ay = sy, by = -1 - sy * y0;
  if (xRev) { ax = -ax; bx = -bx; }   // xMin → +1, xMax → -1
  if (yRev) { ay = -ay; by = -by; }
  return { ax, bx, ay, by, xLog: !!xLog, yLog: !!yLog };
}

// data (x, y) → [clipX, clipY]. For tests + CPU-side hit-testing.
export function projectPoint(p, x, y) {
  const tx = p.xLog ? Math.log(x) : x;
  const ty = p.yLog ? Math.log(y) : y;
  return [p.ax * tx + p.bx, p.ay * ty + p.by];
}

// Inverse: clip x → data x (hover / hit-test). Inverse for y likewise.
export function unprojectX(p, clipX) {
  const tx = (clipX - p.bx) / p.ax;
  return p.xLog ? Math.exp(tx) : tx;
}
export function unprojectY(p, clipY) {
  const ty = (clipY - p.by) / p.ay;
  return p.yLog ? Math.exp(ty) : ty;
}
