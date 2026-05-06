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
 *
 * Viewport (controlled from the parent so fit / range inputs can mutate it):
 *   { r: [rMin, rMax] }
 */
import { useEffect, useMemo, useRef } from 'react';

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

/**
 * Pick a "nice" step for splitting a range into ~target divisions, using the
 * 1 / 2 / 2.5 / 5 / 10 multiplier set. The 2.5 step is what produces the
 * 0.25 / 0.5 / 0.75 / 1 series readers expect on a polar grid.
 */
function niceStep(range, target = 4) {
  if (range <= 0) return 1;
  const rough = range / target;
  const pow = Math.pow(10, Math.floor(Math.log10(rough)));
  const norm = rough / pow;
  if (norm < 1.5)  return 1   * pow;
  if (norm < 2.25) return 2   * pow;
  if (norm < 3.75) return 2.5 * pow;
  if (norm < 7)    return 5   * pow;
  return 10 * pow;
}

/** Round `v` upwards to the nearest "nice" rMax — 0.25 / 0.5 / 1 / 2.5 / 5 / 10 / … */
export function nicePolarMax(v) {
  if (!Number.isFinite(v) || v <= 0) return 1;
  const step = niceStep(v, 4);
  return Math.ceil(v / step) * step;
}

/** Default viewport for a figure — polar uses {r:[…]}, cartesian {x,y}. */
export function defaultPolarViewport(figure) {
  if (Array.isArray(figure.rlim) && figure.rlim.length === 2) {
    return { r: figure.rlim.slice() };
  }
  let m = 0;
  figure.series?.forEach((s) => s.rho?.forEach((v) => {
    if (Number.isFinite(v) && Math.abs(v) > m) m = Math.abs(v);
  }));
  return { r: [0, nicePolarMax(m || 1)] };
}

export default function PolarPlot({
  figure, width, height,
  viewport, setViewport,
  major = true,
  minor = true,
  fontScale = 1,
  interactive = true,
}) {
  const svgRef  = useRef(null);
  const dragRef = useRef(null);
  const dirSign = figure.thetaDir === 'clockwise' ? -1 : 1;
  const zero    = thetaZeroOffset(figure.thetaZeroLocation);

  // Resolve viewport: prefer controlled prop, else fall back to figure.rlim,
  // else auto from data extent.
  const fallback = useMemo(() => defaultPolarViewport(figure), [figure]);
  const vp = (viewport && Array.isArray(viewport.r) && viewport.r.length === 2)
    ? viewport
    : fallback;
  const [rMin, rMax] = vp.r;

  const padTop = (figure.title ? 28 : 12) * fontScale;
  const padBot = 12 * fontScale;
  const padX   = 12 * fontScale;
  const cx = width / 2;
  const cy = padTop + (height - padTop - padBot) / 2;
  const radius = Math.max(20, Math.min(width / 2 - padX - 30, (height - padTop - padBot) / 2 - 28));

  const span = (rMax - rMin) || 1;
  const rScale = (rho) => ((rho - rMin) / span) * radius;

  // Major rings on every nice step (skipping the centre at rMin) plus minor
  // rings at step/5 spacing — matches InteractivePlot's tick split.
  const { rTicksMajor, rTicksMinor } = useMemo(() => {
    const step = niceStep(span, 4);
    const majorArr = [];
    const start = Math.ceil(rMin / step) * step;
    for (let v = start; v <= rMax + step * 1e-6; v += step) {
      if (Math.abs(v - rMin) > step * 1e-6) majorArr.push(+v.toFixed(12));
    }
    const minorStep = step / 5;
    const minorArr = [];
    for (let v = Math.ceil(rMin / minorStep) * minorStep; v <= rMax + minorStep * 1e-6; v += minorStep) {
      if (Math.abs(((v - start) / step) - Math.round((v - start) / step)) > 1e-6
        && Math.abs(v - rMin) > minorStep * 1e-6) {
        minorArr.push(+v.toFixed(12));
      }
    }
    return { rTicksMajor: majorArr, rTicksMinor: minorArr };
  }, [rMin, rMax, span]);

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

  // ── interaction ───────────────────────────────────────────────────────
  function onMouseDown(e) {
    if (!interactive || !setViewport || e.button !== 0) return;
    dragRef.current = { sy: e.clientY, r0: vp.r.slice() };
    e.currentTarget.style.cursor = 'grabbing';
  }
  function onMouseMove(e) {
    if (!dragRef.current || !setViewport) return;
    const d = dragRef.current;
    // Drag-up shrinks rMax (zoom in), drag-down grows it (zoom out).
    // Sensitivity: full-modal-height drag ≈ 2× change.
    const factor = Math.exp((e.clientY - d.sy) / Math.max(150, height));
    const lo = d.r0[0];
    const hi = d.r0[1];
    setViewport({ r: [lo, lo + (hi - lo) * factor] });
  }
  function onMouseUp(e) {
    dragRef.current = null;
    if (e.currentTarget) e.currentTarget.style.cursor = 'grab';
  }
  function onMouseLeave(e) { onMouseUp(e); }
  function onDblClick() {
    if (!interactive || !setViewport) return;
    setViewport(defaultPolarViewport(figure));
  }

  // Wheel listener attached imperatively because React's onWheel is passive.
  useEffect(() => {
    if (!interactive || !setViewport) return;
    const el = svgRef.current;
    if (!el) return;
    function onWheel(e) {
      e.preventDefault();
      const factor = Math.exp(e.deltaY * 0.0015);
      const lo = vp.r[0];
      const hi = vp.r[1];
      setViewport({ r: [lo, lo + (hi - lo) * factor] });
    }
    el.addEventListener('wheel', onWheel, { passive: false });
    return () => el.removeEventListener('wheel', onWheel);
  });

  return (
    <svg
      ref={svgRef}
      width="100%" height="100%"
      viewBox={`0 0 ${width} ${height}`}
      preserveAspectRatio="xMidYMid meet"
      style={{
        display: 'block',
        cursor: interactive && setViewport ? 'grab' : 'default',
        userSelect: 'none',
        fontFamily: 'JetBrains Mono, monospace',
        pointerEvents: interactive ? 'auto' : 'none',
      }}
      onMouseDown={onMouseDown}
      onMouseMove={onMouseMove}
      onMouseUp={onMouseUp}
      onMouseLeave={onMouseLeave}
      onDoubleClick={onDblClick}
    >
      <rect x={0} y={0} width={width} height={height} fill="var(--bg-1)" />

      {/* Title */}
      {figure.title && (
        <text x={cx} y={padTop - 10} fill="var(--plot-text-strong)"
          fontSize={12 * fontScale} textAnchor="middle">{figure.title}</text>
      )}

      <g transform={`translate(${cx}, ${cy})`}>
        {/* Minor rings: faint, no labels */}
        {minor && rTicksMinor.map((rho, i) => {
          const r = rScale(rho);
          if (r <= 0 || r > radius + 0.5) return null;
          return (
            <circle key={`rm${i}`} cx={0} cy={0} r={r} fill="none"
              stroke="var(--plot-grid-min)" />
          );
        })}
        {/* Minor spokes — every 15°, between the 30° majors */}
        {minor && Array.from({ length: 12 }, (_, k) => k * 30 + 15).map((deg) => {
          const a = zero + dirSign * (deg * Math.PI / 180);
          const x = Math.cos(a) * radius;
          const y = -Math.sin(a) * radius;
          return (
            <line key={`smn${deg}`} x1={0} y1={0} x2={x} y2={y}
              stroke="var(--plot-grid-min)" />
          );
        })}

        {/* Major rings + radial tick labels */}
        {major && rTicksMajor.map((rho, i) => {
          const r = rScale(rho);
          if (r <= 0 || r > radius + 0.5) return null;
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

        {/* Major angular spokes every 30° + degree labels */}
        {Array.from({ length: 12 }, (_, k) => k * 30).map((deg) => {
          const a = zero + dirSign * (deg * Math.PI / 180);
          const x = Math.cos(a) * radius;
          const y = -Math.sin(a) * radius;
          const xt = Math.cos(a) * (radius + 14);
          const yt = -Math.sin(a) * (radius + 14);
          return (
            <g key={`sp${deg}`}>
              {major && (
                <line x1={0} y1={0} x2={x} y2={y}
                  stroke="var(--plot-grid)" strokeDasharray="2 4" />
              )}
              <text x={xt} y={yt + 3} fill="var(--plot-text)" fontSize={9 * fontScale}
                textAnchor="middle">{deg}°</text>
            </g>
          );
        })}

        {/* Series — polylines, clipped against rMax */}
        <clipPath id={`pclip-${figure.id}-${Math.round(width)}`}>
          <circle cx={0} cy={0} r={radius} />
        </clipPath>
        <g clipPath={`url(#pclip-${figure.id}-${Math.round(width)})`}>
          {figure.series?.map((s, idx) => {
            if (!s.theta?.length || !s.rho?.length) return null;
            const color = s.color || PALETTE[idx % PALETTE.length];
            let d = '';
            let started = false;
            for (let i = 0; i < s.theta.length; i++) {
              const rho = s.rho[i];
              if (rho == null || !Number.isFinite(rho)) { started = false; continue; }
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
      </g>
    </svg>
  );
}
