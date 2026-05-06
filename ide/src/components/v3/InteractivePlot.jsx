import { useEffect, useRef, useState } from 'react';
import ContextMenu from './ContextMenu';
import { computeFitViewport, exportSvgNode, exportPngNode } from './plotUtils';

/**
 * Pan/zoom interactive plot used in figure preview cards and the FigureWindow modal.
 *
 * `figure` shape:
 *   { id, title, xLabel, yLabel, xRange:[lo,hi], yRange:[lo,hi],
 *     series: [{ name, x:Number[], y:Number[], color, width?, opacity? }] }
 *
 * `viewport` is controlled by the parent so axis-range inputs / fit menus can
 * mutate it from outside.
 */
export default function InteractivePlot({
  figure,
  width,
  height,
  viewport,
  setViewport,
  major = true,
  minor = true,
  fontScale = 1,
  interactive = true,
}) {
  const svgRef = useRef(null);
  const [hover, setHover] = useState(null);
  const [ctxMenu, setCtxMenu] = useState(null);
  const dragRef = useRef(null);

  const padL = 60 * fontScale;
  const padR = 18;
  const padT = 36 * fontScale;
  const padB = 44 * fontScale;
  const W = Math.max(50, width - padL - padR);
  const H = Math.max(50, height - padT - padB);

  const [xMin, xMax] = viewport.x;
  const [yMin, yMax] = viewport.y;
  const sx = (v) => padL + ((v - xMin) / (xMax - xMin)) * W;
  const sy = (v) => padT + H - ((v - yMin) / (yMax - yMin)) * H;
  const isx = (px) => xMin + ((px - padL) / W) * (xMax - xMin);
  const isy = (py) => yMax - ((py - padT) / H) * (yMax - yMin);

  function niceTicks(min, max, target = 6) {
    const range = max - min;
    if (range <= 0) return { major: [min], minor: [] };
    const rough = range / target;
    const pow = Math.pow(10, Math.floor(Math.log10(rough)));
    const norm = rough / pow;
    let step;
    if (norm < 1.5) step = 1 * pow;
    else if (norm < 3) step = 2 * pow;
    else if (norm < 7) step = 5 * pow;
    else step = 10 * pow;
    const start = Math.ceil(min / step) * step;
    const majorArr = [];
    for (let v = start; v <= max + step * 1e-6; v += step) majorArr.push(+v.toFixed(12));
    const minorStep = step / 5;
    const minorArr = [];
    for (let v = Math.ceil(min / minorStep) * minorStep; v <= max + minorStep * 1e-6; v += minorStep) {
      if (Math.abs(((v - start) / step) - Math.round((v - start) / step)) > 1e-6) minorArr.push(+v.toFixed(12));
    }
    return { major: majorArr, minor: minorArr };
  }

  const xTicks = niceTicks(xMin, xMax, 8);
  const yTicks = niceTicks(yMin, yMax, 6);

  function fmtTick(v) {
    const a = Math.abs(v);
    if (a !== 0 && (a < 1e-3 || a >= 1e5)) return v.toExponential(1);
    if (a >= 100) return v.toFixed(0);
    if (a >= 10)  return v.toFixed(1);
    if (a >= 1)   return v.toFixed(2);
    return v.toFixed(3);
  }

  function onMouseDown(e) {
    if (!interactive || e.button !== 0) return;
    const rect = svgRef.current.getBoundingClientRect();
    dragRef.current = {
      sx: e.clientX, sy: e.clientY,
      x0: viewport.x.slice(), y0: viewport.y.slice(),
      W, H, rect,
    };
    e.currentTarget.style.cursor = 'grabbing';
  }
  function onMouseMove(e) {
    if (!svgRef.current || !interactive) return;
    const rect = svgRef.current.getBoundingClientRect();
    const px = (e.clientX - rect.left) * (width / rect.width);
    const py = (e.clientY - rect.top)  * (height / rect.height);
    if (px >= padL && px <= padL + W && py >= padT && py <= padT + H) {
      setHover({ px, py, x: isx(px), y: isy(py) });
    } else {
      setHover(null);
    }
    if (!dragRef.current) return;
    const d = dragRef.current;
    const sxRatio = (d.x0[1] - d.x0[0]) / (d.W * (rect.width / width));
    const syRatio = (d.y0[1] - d.y0[0]) / (d.H * (rect.height / height));
    const dx = (e.clientX - d.sx) * sxRatio;
    const dy = (e.clientY - d.sy) * syRatio;
    setViewport({
      x: [d.x0[0] - dx, d.x0[1] - dx],
      y: [d.y0[0] + dy, d.y0[1] + dy],
    });
  }
  function onMouseUp(e) {
    dragRef.current = null;
    if (e.currentTarget) e.currentTarget.style.cursor = 'grab';
  }
  function onMouseLeave(e) {
    setHover(null);
    onMouseUp(e);
  }
  function onDblClick() {
    if (!interactive) return;
    setViewport({ x: figure.xRange.slice(), y: figure.yRange.slice() });
  }
  function onContextMenu(e) {
    if (!interactive) return;
    e.preventDefault();
    setCtxMenu({ x: e.clientX, y: e.clientY });
  }
  function applyFit(mode, axisMode) {
    const figDefault = { x: figure.xRange.slice(), y: figure.yRange.slice() };
    setViewport(computeFitViewport(figure.series, mode, axisMode, viewport, figDefault));
  }
  const multiSeries = Array.isArray(figure.series) && figure.series.length > 1;
  const ctxItems = [
    { label: 'Reset to default',
      onClick: () => setViewport({ x: figure.xRange.slice(), y: figure.yRange.slice() }) },
    { label: 'Save as SVG',
      onClick: () => exportSvgNode(svgRef.current, `figure_${figure.id}.svg`) },
    { label: 'Save as PNG @2×',
      onClick: () => exportPngNode(svgRef.current, width, height, 2, `figure_${figure.id}.png`) },
    { separator: true },
    { label: 'Fit both axes', onClick: () => applyFit('all', 'both') },
    { label: 'Fit X only',    onClick: () => applyFit('all', 'x') },
    { label: 'Fit Y only',    onClick: () => applyFit('all', 'y') },
    ...(multiSeries ? [
      { separator: true },
      { head: 'Fit single curve' },
      ...figure.series.map((s) => ({
        row: true, color: s.color, name: s.name,
        buttons: [
          { label: 'xy', onClick: () => applyFit(s.name, 'both') },
          { label: 'x',  onClick: () => applyFit(s.name, 'x') },
          { label: 'y',  onClick: () => applyFit(s.name, 'y') },
        ],
      })),
    ] : []),
  ];

  // wheel listener attached imperatively because React's onWheel is passive.
  useEffect(() => {
    if (!interactive) return;
    const el = svgRef.current;
    if (!el) return;
    function onWheel(e) {
      e.preventDefault();
      const rect = el.getBoundingClientRect();
      const px = (e.clientX - rect.left) * (width / rect.width);
      const py = (e.clientY - rect.top)  * (height / rect.height);
      const cx = isx(px), cy = isy(py);
      const factor = Math.exp(e.deltaY * 0.0015);
      setViewport({
        x: [cx - (cx - xMin) * factor, cx + (xMax - cx) * factor],
        y: [cy - (cy - yMin) * factor, cy + (yMax - cy) * factor],
      });
    }
    el.addEventListener('wheel', onWheel, { passive: false });
    return () => el.removeEventListener('wheel', onWheel);
  });

  const clipId = `clip-${figure.id}-${Math.round(width)}`;

  let hoverSnap = null;
  if (hover && figure.series.length === 1) {
    const s = figure.series[0];
    let bestI = 0, bestD = Infinity;
    for (let i = 0; i < s.x.length; i++) {
      const d = Math.abs(s.x[i] - hover.x);
      if (d < bestD) { bestD = d; bestI = i; }
    }
    hoverSnap = { x: s.x[bestI], y: s.y[bestI], color: s.color, name: s.name };
  }

  return (
    <>
    {ctxMenu && (
      <ContextMenu x={ctxMenu.x} y={ctxMenu.y} items={ctxItems}
        onClose={() => setCtxMenu(null)} />
    )}
    <svg
      ref={svgRef}
      width="100%" height="100%"
      viewBox={`0 0 ${width} ${height}`}
      preserveAspectRatio="xMidYMid meet"
      style={{
        display: 'block',
        cursor: interactive ? 'grab' : 'default',
        userSelect: 'none',
        fontFamily: 'JetBrains Mono, monospace',
        pointerEvents: interactive ? 'auto' : 'none',
      }}
      onMouseDown={onMouseDown}
      onMouseMove={onMouseMove}
      onMouseUp={onMouseUp}
      onMouseLeave={onMouseLeave}
      onDoubleClick={onDblClick}
      onContextMenu={onContextMenu}
    >
      <defs>
        <clipPath id={clipId}>
          <rect x={padL} y={padT} width={W} height={H} />
        </clipPath>
      </defs>

      {/* Full viewBox background so any "letterbox" margins from
          preserveAspectRatio="meet" don't show through to the body's
          panel-coloured background underneath. */}
      <rect x={0} y={0} width={width} height={height} fill="var(--bg-1)" />
      <rect x={padL} y={padT} width={W} height={H} fill="var(--plot-bg)" />

      {minor && xTicks.minor.map((v, i) => (
        <line key={`mx${i}`} x1={sx(v)} x2={sx(v)} y1={padT} y2={padT + H} stroke="var(--plot-grid-min)" />
      ))}
      {minor && yTicks.minor.map((v, i) => (
        <line key={`my${i}`} x1={padL} x2={padL + W} y1={sy(v)} y2={sy(v)} stroke="var(--plot-grid-min)" />
      ))}
      {major && xTicks.major.map((v, i) => (
        <line key={`gx${i}`} x1={sx(v)} x2={sx(v)} y1={padT} y2={padT + H} stroke="var(--plot-grid)" />
      ))}
      {major && yTicks.major.map((v, i) => (
        <line key={`gy${i}`} x1={padL} x2={padL + W} y1={sy(v)} y2={sy(v)} stroke="var(--plot-grid)" />
      ))}

      <g clipPath={`url(#${clipId})`}>
        {figure.series.map((s, sIdx) => {
          const mode = s.mode || 'line';
          const w = s.width || 1.5;
          const op = s.opacity ?? 1;

          // Scatter: just markers, no line connection
          if (mode === 'scatter') {
            return (
              <g key={s.name} opacity={op}>
                {s.x.map((xv, i) => {
                  const px = sx(xv), py = sy(s.y[i]);
                  if (!Number.isFinite(px) || !Number.isFinite(py)) return null;
                  return <circle key={i} cx={px} cy={py} r={2.5} fill={s.color} />;
                })}
              </g>
            );
          }

          // Stem: vertical lines from y=0 baseline to data points + marker
          if (mode === 'stem') {
            const baseY = sy(Math.max(0, yMin));
            return (
              <g key={s.name} opacity={op}>
                {s.x.map((xv, i) => {
                  const px = sx(xv), py = sy(s.y[i]);
                  if (!Number.isFinite(px) || !Number.isFinite(py)) return null;
                  return (
                    <g key={i}>
                      <line x1={px} x2={px} y1={baseY} y2={py} stroke={s.color} strokeWidth={w * 0.7} />
                      <circle cx={px} cy={py} r={2.2} fill={s.color} />
                    </g>
                  );
                })}
              </g>
            );
          }

          // Bar / hist: filled rectangles centered on x with auto width
          if (mode === 'bar') {
            // bar width = 70% of inter-x spacing (or 70% of W/N if uniform)
            const xs = s.x.filter(Number.isFinite);
            let bw = 8;
            if (xs.length > 1) {
              const spacing = Math.abs(sx(xs[1]) - sx(xs[0]));
              bw = Math.max(2, spacing * 0.7);
            }
            const baseY = sy(Math.max(0, yMin));
            // Per-series horizontal offset so multiple bar series don't overlap
            const off = (sIdx - (figure.series.length - 1) / 2) * bw * 1.05;
            return (
              <g key={s.name} opacity={op}>
                {s.x.map((xv, i) => {
                  const px = sx(xv) + off, py = sy(s.y[i]);
                  if (!Number.isFinite(px) || !Number.isFinite(py)) return null;
                  const top = Math.min(py, baseY);
                  const h = Math.abs(py - baseY);
                  return <rect key={i} x={px - bw / 2} y={top} width={bw} height={h}
                    fill={s.color} stroke="none" />;
                })}
              </g>
            );
          }

          // Stairs: zero-order hold piecewise line
          if (mode === 'stairs') {
            let d = '';
            for (let i = 0; i < s.x.length; i++) {
              const px = sx(s.x[i]), py = sy(s.y[i]);
              if (!Number.isFinite(px) || !Number.isFinite(py)) continue;
              if (i === 0) d += `M${px.toFixed(2)},${py.toFixed(2)} `;
              else {
                const pPx = sx(s.x[i - 1]);
                d += `L${px.toFixed(2)},${sy(s.y[i - 1]).toFixed(2)} L${px.toFixed(2)},${py.toFixed(2)} `;
                void pPx;
              }
            }
            return <path key={s.name} d={d} stroke={s.color} fill="none"
              strokeWidth={w} opacity={op}
              strokeLinejoin="miter" strokeLinecap="butt" />;
          }

          // Default: line
          let started = false;
          let d = '';
          for (let i = 0; i < s.x.length; i++) {
            const px = sx(s.x[i]), py = sy(s.y[i]);
            if (Number.isFinite(px) && Number.isFinite(py)) {
              d += (started ? 'L' : 'M') + px.toFixed(2) + ',' + py.toFixed(2) + ' ';
              started = true;
            }
          }
          return (
            <path key={s.name} d={d} stroke={s.color} fill="none"
              strokeWidth={w} opacity={op}
              strokeLinejoin="round" strokeLinecap="round" />
          );
        })}
      </g>

      <rect x={padL} y={padT} width={W} height={H} fill="none" stroke="var(--plot-frame)" />

      {xTicks.major.map((v, i) => {
        const x = sx(v);
        if (x < padL - 1 || x > padL + W + 1) return null;
        return (
          <g key={`xl${i}`}>
            <line x1={x} x2={x} y1={padT + H} y2={padT + H + 4} stroke="var(--plot-tick)" />
            <text x={x} y={padT + H + 14 * fontScale + 2} fill="var(--plot-text)" fontSize={10 * fontScale} textAnchor="middle">{fmtTick(v)}</text>
          </g>
        );
      })}
      {yTicks.major.map((v, i) => {
        const y = sy(v);
        if (y < padT - 1 || y > padT + H + 1) return null;
        return (
          <g key={`yl${i}`}>
            <line x1={padL - 4} x2={padL} y1={y} y2={y} stroke="var(--plot-tick)" />
            <text x={padL - 7} y={y + 3} fill="var(--plot-text)" fontSize={10 * fontScale} textAnchor="end">{fmtTick(v)}</text>
          </g>
        );
      })}

      {figure.xLabel && (
        <text x={padL + W / 2} y={height - 8} fill="var(--plot-text)" fontSize={11 * fontScale} textAnchor="middle">{figure.xLabel}</text>
      )}
      {figure.yLabel && (
        <text x={14} y={padT + H / 2} fill="var(--plot-text)" fontSize={11 * fontScale} textAnchor="middle"
          transform={`rotate(-90 14 ${padT + H / 2})`}>{figure.yLabel}</text>
      )}
      {figure.title && (
        <text x={padL + W / 2} y={padT - 12 * fontScale} fill="var(--plot-text-strong)" fontSize={12 * fontScale} textAnchor="middle">{figure.title}</text>
      )}

      {hover && (
        <g pointerEvents="none">
          <line x1={hover.px} x2={hover.px} y1={padT} y2={padT + H} stroke="var(--plot-cross)" strokeDasharray="2 3"/>
          <line x1={padL} x2={padL + W} y1={hover.py} y2={hover.py} stroke="var(--plot-cross)" strokeDasharray="2 3"/>
          {hoverSnap && (
            <>
              <circle cx={sx(hoverSnap.x)} cy={sy(hoverSnap.y)} r="3" fill={hoverSnap.color} stroke="white" strokeWidth="1"/>
              <g transform={`translate(${Math.min(hover.px + 8, padL + W - 110)}, ${Math.max(hover.py - 28, padT + 4)})`}>
                <rect width="104" height="26" fill="var(--plot-tip-bg)" stroke="var(--plot-cross)" rx="3"/>
                <text x="6" y="11" fill="var(--plot-tip-text)" fontSize="10">x = {fmtTick(hoverSnap.x)}</text>
                <text x="6" y="22" fill="var(--plot-tip-text)" fontSize="10">y = {fmtTick(hoverSnap.y)}</text>
              </g>
            </>
          )}
        </g>
      )}
    </svg>
    </>
  );
}
