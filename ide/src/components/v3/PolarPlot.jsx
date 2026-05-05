/**
 * Polar plot — `polarplot(theta, rho)`. Concentric-circle radial grid, radial
 * spokes every 30°, optional `thetaDir` (clockwise/counterclockwise) and
 * `thetaZeroLocation` (top/bottom/left/right) from the engine config.
 *
 * Figure shape (built by adapters.adaptFigure when cfg.polar=true):
 *   {
 *     id, title,
 *     thetaDir,          // 'counterclockwise' (default) | 'clockwise'
 *     thetaZeroLocation, // 'right' (default) | 'top' | 'left' | 'bottom'
 *     rlim,              // [rmin, rmax] — auto-computed when undefined
 *     series: [{ name, theta:Number[], rho:Number[], color, width? }],
 *   }
 */
import { useMemo } from 'react';

const PALETTE = ['#7fd99a', '#5fb3d4', '#e9b870', '#9b8cf2', '#e26a6a',
                 '#d4a5e6', '#f2a37e', '#6fcfbf'];

function thetaZeroOffset(loc) {
  switch (loc) {
    case 'top':    return Math.PI / 2;
    case 'left':   return Math.PI;
    case 'bottom': return -Math.PI / 2;
    default:       return 0;
  }
}

function niceMax(v) {
  if (v <= 0) return 1;
  const pow = Math.pow(10, Math.floor(Math.log10(v)));
  const norm = v / pow;
  let step;
  if (norm < 1.5) step = 1.5 * pow;
  else if (norm < 3) step = 3 * pow;
  else if (norm < 7) step = 7 * pow;
  else step = 10 * pow;
  return Math.ceil(v / (step / 4)) * (step / 4);
}

export default function PolarPlot({ figure, width, height, fontScale = 1 }) {
  const dirSign = figure.thetaDir === 'clockwise' ? -1 : 1;
  const zero    = thetaZeroOffset(figure.thetaZeroLocation);

  const rMax = useMemo(() => {
    if (Array.isArray(figure.rlim) && figure.rlim.length === 2) return figure.rlim[1];
    let m = 0;
    figure.series?.forEach((s) => s.rho?.forEach((v) => {
      if (v != null && Math.abs(v) > m) m = Math.abs(v);
    }));
    return niceMax(m || 1);
  }, [figure]);
  const rMin = Array.isArray(figure.rlim) && figure.rlim.length === 2 ? figure.rlim[0] : 0;

  const padTop = (figure.title ? 28 : 12) * fontScale;
  const padBot = 12 * fontScale;
  const padX   = 12 * fontScale;
  const cx = width / 2;
  const cy = padTop + (height - padTop - padBot) / 2;
  const radius = Math.max(20, Math.min(width / 2 - padX - 30, (height - padTop - padBot) / 2 - 28));

  const rScale = (rho) => ((rho - rMin) / (rMax - rMin || 1)) * radius;

  const rTicks = (() => {
    // 4 evenly spaced rings from rMin (inner) to rMax (outer). Skip the centre ring.
    const arr = [];
    for (let i = 1; i <= 4; i++) arr.push(rMin + ((rMax - rMin) * i) / 4);
    return arr;
  })();

  function fmtR(v) {
    const a = Math.abs(v);
    if (a !== 0 && (a < 1e-3 || a >= 1e5)) return v.toExponential(1);
    if (a >= 100) return v.toFixed(0);
    if (a >= 10)  return v.toFixed(1);
    if (a >= 1)   return v.toFixed(2);
    return v.toFixed(3);
  }

  function ptFor(theta, rho) {
    const r = rScale(rho);
    const a = zero + dirSign * theta;
    return [Math.cos(a) * r, -Math.sin(a) * r];
  }

  return (
    <svg
      width="100%" height="100%"
      viewBox={`0 0 ${width} ${height}`}
      preserveAspectRatio="xMidYMid meet"
      style={{ display: 'block', userSelect: 'none', fontFamily: 'JetBrains Mono, monospace' }}
    >
      <rect x={0} y={0} width={width} height={height} fill="var(--bg-1)" />

      {/* Title */}
      {figure.title && (
        <text x={cx} y={padTop - 10} fill="var(--plot-text-strong)"
          fontSize={12 * fontScale} textAnchor="middle">{figure.title}</text>
      )}

      <g transform={`translate(${cx}, ${cy})`}>
        {/* Radial grid: concentric circles + tick labels */}
        {rTicks.map((rho, i) => {
          const r = rScale(rho);
          return (
            <g key={`rt${i}`}>
              <circle cx={0} cy={0} r={r} fill="none" stroke="var(--plot-grid)" strokeDasharray="2 4" />
              <text x={3} y={-r - 2} fill="var(--plot-text)" fontSize={9 * fontScale}>
                {fmtR(rho)}
              </text>
            </g>
          );
        })}
        {/* Outer frame circle */}
        <circle cx={0} cy={0} r={radius} fill="none" stroke="var(--plot-frame)" />

        {/* Angular spokes every 30° */}
        {Array.from({ length: 12 }, (_, k) => k * 30).map((deg) => {
          const a = zero + dirSign * (deg * Math.PI / 180);
          const x = Math.cos(a) * radius;
          const y = -Math.sin(a) * radius;
          const xt = Math.cos(a) * (radius + 14);
          const yt = -Math.sin(a) * (radius + 14);
          return (
            <g key={`sp${deg}`}>
              <line x1={0} y1={0} x2={x} y2={y} stroke="var(--plot-grid)" strokeDasharray="2 4" />
              <text x={xt} y={yt + 3} fill="var(--plot-text)" fontSize={9 * fontScale}
                textAnchor="middle">{deg}°</text>
            </g>
          );
        })}

        {/* Series — polylines */}
        {figure.series?.map((s, idx) => {
          if (!s.theta?.length || !s.rho?.length) return null;
          const color = s.color || PALETTE[idx % PALETTE.length];
          let d = '';
          let started = false;
          for (let i = 0; i < s.theta.length; i++) {
            const rho = s.rho[i];
            if (rho == null || !Number.isFinite(rho)) continue;
            const [x, y] = ptFor(s.theta[i], rho);
            d += (started ? 'L' : 'M') + x.toFixed(2) + ',' + y.toFixed(2) + ' ';
            started = true;
          }
          // Close the path if theta sweeps a full revolution
          const range = Math.abs(s.theta[s.theta.length - 1] - s.theta[0]);
          if (range >= Math.PI * 1.95) d += 'Z';
          return (
            <path key={s.name} d={d} stroke={color} fill="none"
              strokeWidth={s.width || 1.6}
              strokeLinejoin="round" strokeLinecap="round" />
          );
        })}
      </g>
    </svg>
  );
}
