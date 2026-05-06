/**
 * Heatmap (imagesc) renderer. Mirrors InteractivePlot's outer shape — same
 * SVG / viewBox / pan-zoom hooks — but renders a pixel image inside the
 * plot area instead of line paths.
 *
 * Figure shape it expects (built by adapters.adaptFigure when type='imagesc'):
 *   {
 *     id, title, xLabel, yLabel,
 *     xRange, yRange,           // matrix coordinate extents
 *     z: number[][],            // rows × cols
 *     cmin, cmax, colormap,     // value-range and colormap name
 *   }
 */
import { useEffect, useMemo, useRef, useState } from 'react';
import { renderHeatmapDataURL, getColormap } from './colormaps';
import ContextMenu from './ContextMenu';
import { exportSvgNode, exportPngNode, exportPngForPrint } from './plotUtils';

export default function Heatmap({
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
  const padR = 70 * fontScale;  // wider to fit the colorbar
  const padT = 36 * fontScale;
  const padB = 44 * fontScale;
  const W = Math.max(50, width - padL - padR);
  const H = Math.max(50, height - padT - padB);

  const [xMin, xMax] = viewport.x;
  const [yMin, yMax] = viewport.y;
  const sx  = (v) => padL + ((v - xMin) / (xMax - xMin)) * W;
  const sy  = (v) => padT + H - ((v - yMin) / (yMax - yMin)) * H;
  const isx = (px) => xMin + ((px - padL) / W) * (xMax - xMin);
  const isy = (py) => yMax - ((py - padT) / H) * (yMax - yMin);

  // Pre-render the heatmap to a data URL (cached on the figure/colormap).
  // Memoised on z reference + range so panning doesn't re-rasterise.
  const dataURL = useMemo(() => {
    if (!figure.z) return null;
    return renderHeatmapDataURL(figure.z, figure.cmin, figure.cmax, figure.colormap);
  }, [figure.z, figure.cmin, figure.cmax, figure.colormap]);

  function niceTicks(min, max, target = 6) {
    const range = max - min;
    if (range <= 0) return { major: [min], minor: [] };
    const rough = range / target;
    const pow = Math.pow(10, Math.floor(Math.log10(rough)));
    const norm = rough / pow;
    const step = norm < 1.5 ? pow : norm < 3 ? 2 * pow : norm < 7 ? 5 * pow : 10 * pow;
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

  /* ─── pan/zoom (same as InteractivePlot) ─── */
  function onMouseDown(e) {
    if (!interactive || e.button !== 0) return;
    const rect = svgRef.current.getBoundingClientRect();
    dragRef.current = { sx: e.clientX, sy: e.clientY, x0: viewport.x.slice(), y0: viewport.y.slice(), W, H, rect };
    e.currentTarget.style.cursor = 'grabbing';
  }
  function onMouseMove(e) {
    if (!svgRef.current || !interactive) return;
    const rect = svgRef.current.getBoundingClientRect();
    const px = (e.clientX - rect.left) * (width / rect.width);
    const py = (e.clientY - rect.top)  * (height / rect.height);
    if (px >= padL && px <= padL + W && py >= padT && py <= padT + H) {
      setHover({ px, py, x: isx(px), y: isy(py) });
    } else setHover(null);
    if (!dragRef.current) return;
    const d = dragRef.current;
    const sxRatio = (d.x0[1] - d.x0[0]) / (d.W * (rect.width / width));
    const syRatio = (d.y0[1] - d.y0[0]) / (d.H * (rect.height / height));
    const dx = (e.clientX - d.sx) * sxRatio;
    const dy = (e.clientY - d.sy) * syRatio;
    setViewport({ x: [d.x0[0] - dx, d.x0[1] - dx], y: [d.y0[0] + dy, d.y0[1] + dy] });
  }
  function onMouseUp(e)    { dragRef.current = null; if (e.currentTarget) e.currentTarget.style.cursor = 'grab'; }
  function onMouseLeave(e) { setHover(null); onMouseUp(e); }
  function onDblClick()    { if (interactive) setViewport({ x: figure.xRange.slice(), y: figure.yRange.slice() }); }
  function onContextMenu(e) {
    if (!interactive) return;
    e.preventDefault();
    setCtxMenu({ x: e.clientX, y: e.clientY });
  }
  const ctxItems = [
    { label: 'Reset to default',
      onClick: () => setViewport({ x: figure.xRange.slice(), y: figure.yRange.slice() }) },
    { label: 'Save as SVG (vector)',
      onClick: () => exportSvgNode(svgRef.current, `figure_${figure.id}.svg`) },
    { label: 'Save as PNG (screen 2×)',
      onClick: () => exportPngNode(svgRef.current, width, height, 2, `figure_${figure.id}.png`) },
    { head: 'Save for print (300 DPI)' },
    { label: 'PNG · 1 column (85 mm)',
      onClick: () => exportPngForPrint(svgRef.current, width, height, 85, 300, `figure_${figure.id}`) },
    { label: 'PNG · 2 columns (170 mm)',
      onClick: () => exportPngForPrint(svgRef.current, width, height, 170, 300, `figure_${figure.id}`) },
    { label: 'PNG · A4 width (210 mm)',
      onClick: () => exportPngForPrint(svgRef.current, width, height, 210, 300, `figure_${figure.id}`) },
  ];

  useEffect(() => {
    if (!interactive) return;
    const el = svgRef.current; if (!el) return;
    function onWheel(e) {
      e.preventDefault();
      const rect = el.getBoundingClientRect();
      const px = (e.clientX - rect.left) * (width / rect.width);
      const py = (e.clientY - rect.top)  * (height / rect.height);
      const cx = isx(px), cy = isy(py);
      const factor = Math.exp(e.deltaY * 0.0015);
      setViewport({ x: [cx - (cx - xMin) * factor, cx + (xMax - cx) * factor],
                    y: [cy - (cy - yMin) * factor, cy + (yMax - cy) * factor] });
    }
    el.addEventListener('wheel', onWheel, { passive: false });
    return () => el.removeEventListener('wheel', onWheel);
  });

  const clipId = `clip-h-${figure.id}-${Math.round(width)}`;
  // The heatmap image is stretched to fill the figure's xRange × yRange in
  // viewport coordinates — pan/zoom moves the SVG rect, the image follows.
  const imgX = sx(figure.xRange[0]);
  const imgY = sy(figure.yRange[1]);   // top-left of viewport in screen space
  const imgW = sx(figure.xRange[1]) - sx(figure.xRange[0]);
  const imgH = sy(figure.yRange[0]) - sy(figure.yRange[1]);

  /* ─── colorbar (right of plot area) ─── */
  const cbarW = 12;
  const cbarX = padL + W + 14;
  const cbarH = H;
  const cbarTicks = niceTicks(figure.cmin, figure.cmax, 5);
  const cbarInterp = getColormap(figure.colormap);
  const cbarStops = Array.from({ length: 11 }, (_, i) => ({
    offset: `${i * 10}%`,
    color:  cbarInterp(i / 10),
  }));
  const cbarGradId = `cbar-${figure.id}-${Math.round(width)}`;

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
        <linearGradient id={cbarGradId} x1="0" y1="1" x2="0" y2="0">
          {cbarStops.map((s, i) => <stop key={i} offset={s.offset} stopColor={s.color} />)}
        </linearGradient>
      </defs>

      <rect x={0} y={0} width={width} height={height} fill="var(--bg-1)" />
      <rect x={padL} y={padT} width={W} height={H} fill="var(--plot-bg)" />

      {/* Heatmap image — pixel data, scaled to viewport. preserveAspectRatio="none"
          stretches in both axes to match the data extent. */}
      {dataURL && (
        <g clipPath={`url(#${clipId})`}>
          <image href={dataURL}
            x={imgX} y={imgY} width={imgW} height={imgH}
            preserveAspectRatio="none"
            imageRendering="pixelated" />
        </g>
      )}

      {/* Optional minor + major grid (faint, over the heatmap) */}
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

      <rect x={padL} y={padT} width={W} height={H} fill="none" stroke="var(--plot-frame)" />

      {/* Tick labels */}
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

      {/* Colorbar */}
      <rect x={cbarX} y={padT} width={cbarW} height={cbarH}
        fill={`url(#${cbarGradId})`}
        stroke="var(--plot-frame)" strokeWidth="0.5" />
      {cbarTicks.major.map((v, i) => {
        const y = padT + cbarH - ((v - figure.cmin) / (figure.cmax - figure.cmin)) * cbarH;
        if (y < padT - 1 || y > padT + cbarH + 1) return null;
        return (
          <g key={`cb${i}`}>
            <line x1={cbarX + cbarW} x2={cbarX + cbarW + 3} y1={y} y2={y} stroke="var(--plot-tick)" />
            <text x={cbarX + cbarW + 6} y={y + 3} fill="var(--plot-text)" fontSize={9 * fontScale} textAnchor="start">{fmtTick(v)}</text>
          </g>
        );
      })}

      {/* Axis titles */}
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

      {/* Crosshair + value at hover */}
      {hover && (
        <g pointerEvents="none">
          <line x1={hover.px} x2={hover.px} y1={padT} y2={padT + H} stroke="var(--plot-cross)" strokeDasharray="2 3"/>
          <line x1={padL} x2={padL + W} y1={hover.py} y2={hover.py} stroke="var(--plot-cross)" strokeDasharray="2 3"/>
          {/* Sample z(row, col) closest to cursor in data coords */}
          {(() => {
            if (!figure.z) return null;
            const nR = figure.z.length, nC = figure.z[0]?.length || 0;
            if (!nR || !nC) return null;
            const u = (hover.x - figure.xRange[0]) / (figure.xRange[1] - figure.xRange[0]);
            const v = (figure.yRange[1] - hover.y) / (figure.yRange[1] - figure.yRange[0]);
            const c = Math.max(0, Math.min(nC - 1, Math.floor(u * nC)));
            const r = Math.max(0, Math.min(nR - 1, Math.floor(v * nR)));
            const z = figure.z[r]?.[c];
            return (
              <g transform={`translate(${Math.min(hover.px + 8, padL + W - 110)}, ${Math.max(hover.py - 38, padT + 4)})`}>
                <rect width="104" height="36" fill="var(--plot-tip-bg)" stroke="var(--plot-cross)" rx="3" />
                <text x="6" y="11" fill="var(--plot-tip-text)" fontSize="10">x = {fmtTick(hover.x)}</text>
                <text x="6" y="22" fill="var(--plot-tip-text)" fontSize="10">y = {fmtTick(hover.y)}</text>
                <text x="6" y="33" fill="var(--plot-tip-text)" fontSize="10">z = {z != null ? fmtTick(z) : '—'}</text>
              </g>
            );
          })()}
        </g>
      )}
    </svg>
    </>
  );
}
