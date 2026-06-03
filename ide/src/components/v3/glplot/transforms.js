// glplot/transforms.js — pure coordinate pre-transforms feeding the renderer.
//
// The GL renderer is coordinate-agnostic: it draws plain (x, y). Polar series
// are converted (θ, ρ) → (x, y) here first, so a single line / scatter
// program serves cartesian and polar alike (the polar grid stays SVG).

// (θ, ρ) → cartesian (x, y).
//   degrees   — θ is in degrees (default radians)
//   thetaZero — angular offset in radians (matches figure polar config)
//   direction — 'ccw' (default) or 'cw'
export function polarToCartesian(theta, rho, opts = {}) {
  const { degrees = false, thetaZero = 0, direction = 'ccw' } = opts;
  const n = Math.min(theta ? theta.length : 0, rho ? rho.length : 0);
  const x = new Float32Array(n);
  const y = new Float32Array(n);
  const toRad = degrees ? Math.PI / 180 : 1;
  const dir = direction === 'cw' ? -1 : 1;
  for (let i = 0; i < n; i++) {
    const a = thetaZero + dir * theta[i] * toRad;
    const r = rho[i];
    x[i] = r * Math.cos(a);
    y[i] = r * Math.sin(a);
  }
  return { x, y };
}
