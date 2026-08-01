// layerRenderers.jsx — per-layer SVG renderers for CompositePlot. Each
// figure layer (a series of any mode, or a text annotation) is drawn here.
// ctx bundles the component's projection + viewport state so this stays a
// pure (ly, idx, ctx) => JSX function, decoupled from the component body.
import { MarkerGlyph, DASH_FOR } from './compositePlot.helpers';

export function renderLayer(ly, idx, ctx) {
  const {
    sx, sy, syOf, padL, padT, W, H, dropOnLog, decimatedSeries,
    cometProgress, previewStride, interactive, seriesLayers,
    yLogActive, yMin, xLogActive, xMin, fontScale, glRouted,
  } = ctx;
            if (glRouted.has(idx)) return null;                // drawn on the WebGL overlay
            if (ly.kind === 'heatmap') return null;            // image already drawn above
            if (ly.kind === 'series') {
              const mode = ly.mode || 'line';
              const w = ly.width || 1.5;
              const op = ly.opacity ?? 1;
              // yyaxis: pick the right-side mapping if this layer is on
              // the right axis. mySy is then used in place of sy for
              // every data-point coordinate produced by this block.
              const mySy = syOf(ly);
              if (mode === 'xline' || mode === 'yline') {
                // Reference line spanning the visible viewport.
                // xline: vertical at x=ly.x[0]; yline: horizontal at
                // y=ly.y[0].
                const lineColor = ly.color || 'var(--plot-text)';
                const lw = ly.width || 1.2;
                if (mode === 'xline') {
                  const x = sx(ly.x[0]);
                  if (!Number.isFinite(x)) return null;
                  return (
                    <line key={`ly${idx}`} x1={x} x2={x}
                      y1={padT} y2={padT + H}
                      stroke={lineColor} strokeWidth={lw}
                      strokeDasharray={ly.lineStyle === '--' ? '6,4' : undefined}
                      opacity={op} />
                  );
                }
                // yline
                const y = mySy(ly.x[0]);   // engine packed y in xJson[0]
                if (!Number.isFinite(y)) return null;
                return (
                  <line key={`ly${idx}`} x1={padL} x2={padL + W}
                    y1={y} y2={y}
                    stroke={lineColor} strokeWidth={lw}
                    strokeDasharray={ly.lineStyle === '--' ? '6,4' : undefined}
                    opacity={op} />
                );
              }
              if (mode === 'scatter') {
                const mk = ly.marker || 'o';
                const r = ly.size || 3;
                // Preview thumbnails subsample: one SVG node per marker would
                // jank the window at 100k+ points (interactive uses GL).
                const N = ly.x.length;
                const step = previewStride(N, interactive);
                const markers = [];
                for (let i = 0; i < N; i += step) {
                  const xv = ly.x[i], yv = ly.y[i];
                  if (!Number.isFinite(xv) || !Number.isFinite(yv)) continue;
                  const px = sx(xv), py = mySy(yv);
                  if (!Number.isFinite(px) || !Number.isFinite(py)) continue;
                  markers.push(<MarkerGlyph key={i} idx={i} marker={mk}
                    cx={px} cy={py} r={r} color={ly.color} filled={ly.filled} />);
                }
                return <g key={`ly${idx}`} opacity={op}>{markers}</g>;
              }
              if (mode === 'stem') {
                return (
                  <g key={`ly${idx}`} opacity={op}>
                    {ly.x.map((xv, i) => {
                      const yv = ly.y[i];
                      if (!Number.isFinite(xv) || !Number.isFinite(yv)) return null;
                      const px = sx(xv), py = mySy(yv);
                      const py0 = mySy(yLogActive ? yMin : 0);
                      if (!Number.isFinite(px) || !Number.isFinite(py)) return null;
                      return (
                        <g key={i}>
                          <line x1={px} x2={px} y1={py0} y2={py}
                            stroke={ly.color} strokeWidth={w * 0.7} />
                          <circle cx={px} cy={py} r={2.5} fill={ly.color} />
                        </g>
                      );
                    })}
                  </g>
                );
              }
              if (mode === 'bar') {
                // Bar mode: filled rects centred on x; width derived from
                // inter-x spacing. Per-series offset spreads multiple bar
                // layers so they don't overlap exactly.
                const xs = ly.x.filter(Number.isFinite);
                let bw = 8;
                if (xs.length > 1) {
                  const spacing = Math.abs(sx(xs[1]) - sx(xs[0]));
                  bw = Math.max(2, spacing * 0.7);
                }
                const baseY = mySy(yLogActive ? yMin : Math.max(0, yMin));
                const sIdx = seriesLayers.indexOf(ly);
                const off = (sIdx - (seriesLayers.length - 1) / 2) * bw * 1.05;
                return (
                  <g key={`ly${idx}`} opacity={op}>
                    {ly.x.map((xv, i) => {
                      const px = sx(xv) + off, py = mySy(ly.y[i]);
                      if (!Number.isFinite(px) || !Number.isFinite(py)) return null;
                      const top = Math.min(py, baseY);
                      const h = Math.abs(py - baseY);
                      return <rect key={i} x={px - bw / 2} y={top}
                        width={bw} height={h} fill={ly.color} stroke="none" />;
                    })}
                  </g>
                );
              }
              if (mode === 'quiver') {
                // Vector field: per-point arrow from (x[i], y[i]) to
                // (x[i] + u[i]*s, y[i] + v[i]*s). Three SVG segments
                // per arrow: shaft + two head fins forming a chevron.
                const u = ly.u || [];
                const v = ly.v || [];
                const s = Number.isFinite(ly.scale) ? ly.scale : 1;
                const headLen = 6;        // head fin length, pixels
                const headAng = Math.PI / 7;  // head opening angle (radians)
                return (
                  <g key={`ly${idx}`} opacity={op}>
                    {ly.x.map((xv, i) => {
                      const yv = ly.y[i];
                      const uv = Number(u[i]);
                      const vv = Number(v[i]);
                      if (!Number.isFinite(xv) || !Number.isFinite(yv)
                       || !Number.isFinite(uv) || !Number.isFinite(vv)) return null;
                      const px1 = sx(xv),       py1 = mySy(yv);
                      const px2 = sx(xv + uv * s), py2 = mySy(yv + vv * s);
                      if (!Number.isFinite(px1) || !Number.isFinite(py1)
                       || !Number.isFinite(px2) || !Number.isFinite(py2)) return null;
                      // Skip degenerate zero-length arrows so the head
                      // doesn't end up as two overlapping points.
                      const dx = px2 - px1, dy = py2 - py1;
                      const mag = Math.hypot(dx, dy);
                      if (mag < 0.5) return null;
                      const ang = Math.atan2(dy, dx);
                      const fx1 = px2 - headLen * Math.cos(ang - headAng);
                      const fy1 = py2 - headLen * Math.sin(ang - headAng);
                      const fx2 = px2 - headLen * Math.cos(ang + headAng);
                      const fy2 = py2 - headLen * Math.sin(ang + headAng);
                      return (
                        <g key={i}>
                          <line x1={px1} y1={py1} x2={px2} y2={py2}
                            stroke={ly.color} strokeWidth={Math.max(1, w * 0.8)} />
                          <line x1={px2} y1={py2} x2={fx1} y2={fy1}
                            stroke={ly.color} strokeWidth={Math.max(1, w * 0.8)} />
                          <line x1={px2} y1={py2} x2={fx2} y2={fy2}
                            stroke={ly.color} strokeWidth={Math.max(1, w * 0.8)} />
                        </g>
                      );
                    })}
                  </g>
                );
              }
              if (mode === 'area') {
                // Filled polygon under the curve. Path: (x[0],base) →
                // (x[0],y[0]) → … → (x[N-1],y[N-1]) → (x[N-1],base) → close.
                // NaN points break the polygon — start a new sub-path.
                // A ≤0 value on a log axis is dropped (connected across),
                // matching the line builder. lastPlottedX tracks the last
                // vertex actually drawn so the baseline-drop close lands on
                // a plottable (positive-on-log) x, never a NaN.
                const base = Number.isFinite(ly.baseline) ? ly.baseline : 0;
                const subpaths = [];
                let cur = '';
                let lastPlottedX = null;
                const closeSub = () => {
                  if (cur && lastPlottedX != null) {
                    cur += `L${sx(lastPlottedX).toFixed(2)},${mySy(base).toFixed(2)} Z `;
                    subpaths.push(cur);
                  }
                  cur = ''; lastPlottedX = null;
                };
                for (let i = 0; i < ly.x.length; i++) {
                  const xv = ly.x[i], yv = ly.y[i];
                  if (!Number.isFinite(xv) || !Number.isFinite(yv)) { closeSub(); continue; }
                  if (dropOnLog(xv, yv)) continue;   // log ≤0 → connect across
                  const px = sx(xv), py = mySy(yv);
                  if (!cur) {
                    cur = `M${px.toFixed(2)},${mySy(base).toFixed(2)} L${px.toFixed(2)},${py.toFixed(2)} `;
                  } else {
                    cur += `L${px.toFixed(2)},${py.toFixed(2)} `;
                  }
                  lastPlottedX = xv;
                }
                closeSub();
                const d = subpaths.join(' ');
                return (
                  <g key={`ly${idx}`} opacity={op}>
                    <path d={d} fill={ly.color} fillOpacity="0.3"
                      stroke={ly.color} strokeWidth={w}
                      strokeLinejoin="round" strokeLinecap="round" />
                  </g>
                );
              }
              if (mode === 'polygon') {
                // Filled polygon(s) — patch / fill / pie wedges / etc.
                // null in x or y starts a new sub-polygon. Each sub-path
                // is closed automatically (Z command) so the renderer
                // doesn't depend on the user repeating the first point.
                let d = '';
                let inSub = false;
                for (let i = 0; i < ly.x.length; i++) {
                  const xv = ly.x[i], yv = ly.y[i];
                  if (!Number.isFinite(xv) || !Number.isFinite(yv)) {
                    if (inSub) { d += 'Z '; inSub = false; }
                    continue;
                  }
                  if (dropOnLog(xv, yv)) continue;   // log ≤0 → skip vertex, connect across
                  const px = sx(xv), py = mySy(yv);
                  d += (inSub ? 'L' : 'M') + px.toFixed(2) + ',' + py.toFixed(2) + ' ';
                  inSub = true;
                }
                if (inSub) d += 'Z';
                // fillOpacity defaults to 0.7 (slight see-through so
                // overlapping polygons stay readable). Caller can tune
                // via ly.fillOpacity.
                const fa = Number.isFinite(ly.fillOpacity) ? ly.fillOpacity : 0.7;
                return (
                  <g key={`ly${idx}`} opacity={op}>
                    <path d={d} fill={ly.color} fillOpacity={fa}
                      stroke={ly.color} strokeWidth={Math.max(0.5, w * 0.6)}
                      strokeLinejoin="round" strokeLinecap="round" />
                  </g>
                );
              }
              if (mode === 'barh') {
                // Horizontal bars. Adapter has already swapped the
                // engine's xJson/yJson so `ly.y` holds positions
                // (vertical / Y axis) and `ly.x` holds lengths
                // (horizontal / X axis). Bars extend from x=0 to x=ly.x[i].
                const ys = ly.y.filter(Number.isFinite);
                let bh = 8;
                if (ys.length > 1) {
                  const spacing = Math.abs(mySy(ys[1]) - mySy(ys[0]));
                  bh = Math.max(2, spacing * 0.7);
                }
                const baseX = sx(xLogActive ? xMin : Math.max(0, xMin));
                const sIdx = seriesLayers.indexOf(ly);
                const off = (sIdx - (seriesLayers.length - 1) / 2) * bh * 1.05;
                return (
                  <g key={`ly${idx}`} opacity={op}>
                    {ly.y.map((yv, i) => {
                      const py = mySy(yv) + off;
                      const px = sx(ly.x[i]);
                      if (!Number.isFinite(px) || !Number.isFinite(py)) return null;
                      const left = Math.min(px, baseX);
                      const w2 = Math.abs(px - baseX);
                      return <rect key={i} x={left} y={py - bh / 2}
                        width={w2} height={bh} fill={ly.color} stroke="none" />;
                    })}
                  </g>
                );
              }
              if (mode === 'contour') {
                const Xs = ly.surfaceGrid?.Xs || [];
                const Ys = ly.surfaceGrid?.Ys || [];
                const Z = ly.surfaceGrid?.Z || [];
                const Nc = Xs.length, Nr = Ys.length;
                if (Nc >= 2 && Nr >= 2) {
                  let zmn = Infinity, zmx = -Infinity;
                  for (let r = 0; r < Nr; r++) {
                    for (let c = 0; c < Nc; c++) {
                      const v = Z[r] ? Z[r][c] : NaN;
                      if (Number.isFinite(v)) { if (v < zmn) zmn = v; if (v > zmx) zmx = v; }
                    }
                  }
                  if (Number.isFinite(zmn)) {
                    let levels = ly.levels;
                    if (!Array.isArray(levels) || levels.length === 0) {
                      const n = ly.n || 10;
                      const step = (zmx - zmn) / (n + 1);
                      levels = [];
                      for (let i = 1; i <= n; i++) levels.push(zmn + i * step);
                    }

                    const interp = (a, b, va, vb, L) => {
                      if (Math.abs(vb - va) < 1e-15) return a;
                      return a + (L - va) / (vb - va) * (b - a);
                    };

                    const colorAt = (t) => {
                      const h = (1 - Math.max(0, Math.min(1, t))) * 240;
                      return `hsl(${h}, 100%, 45%)`;
                    };
                    const zSpan = zmx - zmn;
                    const norm = (v) => (zSpan > 0 ? (v - zmn) / zSpan : 0.5);

                    const paths = [];
                    for (const L of levels) {
                      const col = colorAt(norm(L));
                      let d = '';
                      for (let r = 0; r + 1 < Nr; r++) {
                        for (let c = 0; c + 1 < Nc; c++) {
                          const vTL = Z[r][c], vTR = Z[r][c + 1];
                          const vBL = Z[r + 1][c], vBR = Z[r + 1][c + 1];
                          if (!Number.isFinite(vTL) || !Number.isFinite(vTR)
                           || !Number.isFinite(vBL) || !Number.isFinite(vBR)) continue;
                          let code = 0;
                          if (vTL > L) code |= 1;
                          if (vTR > L) code |= 2;
                          if (vBR > L) code |= 4;
                          if (vBL > L) code |= 8;
                          if (code === 0 || code === 15) continue;

                          const xL = Xs[c], xR = Xs[c + 1];
                          const yT = Ys[r], yB = Ys[r + 1];
                          const T  = [interp(xL, xR, vTL, vTR, L), yT];
                          const RE = [xR, interp(yT, yB, vTR, vBR, L)];
                          const B  = [interp(xL, xR, vBL, vBR, L), yB];
                          const LE = [xL, interp(yT, yB, vTL, vBL, L)];
                          const segs = [];
                          switch (code) {
                            case 1: case 14: segs.push([LE, T]); break;
                            case 2: case 13: segs.push([T, RE]); break;
                            case 3: case 12: segs.push([LE, RE]); break;
                            case 4: case 11: segs.push([RE, B]); break;
                            case 6: case 9:  segs.push([T, B]); break;
                            case 7: case 8:  segs.push([LE, B]); break;
                            case 5:  segs.push([LE, T], [RE, B]); break;
                            case 10: segs.push([LE, B], [T, RE]); break;
                          }
                          for (const [a, b] of segs) {
                            const px1 = sx(a[0]), py1 = mySy(a[1]);
                            const px2 = sx(b[0]), py2 = mySy(b[1]);
                            if (Number.isFinite(px1) && Number.isFinite(py1) &&
                                Number.isFinite(px2) && Number.isFinite(py2)) {
                              d += `M${px1.toFixed(1)},${py1.toFixed(1)} L${px2.toFixed(1)},${py2.toFixed(1)} `;
                            }
                          }
                        }
                      }
                      if (d) paths.push(<path key={L} d={d} stroke={col} fill="none" strokeWidth={w} />);
                    }
                    if (paths.length > 0) return <g key={`ly${idx}`} opacity={op}>{paths}</g>;
                  }
                }
              }
              if (mode === 'errorbar') {
                // Three SVG elements per data point:
                //   • centre dot (the y value)
                //   • vertical bar from y-eNeg to y+ePos
                //   • horizontal cap at each end of the bar
                // eNeg / ePos arrays are indexed parallel to x/y. Missing
                // entries fall back to 0 (no bar drawn).
                const eN = ly.eNeg || [];
                const eP = ly.ePos || [];
                const cap = 5;  // pixel half-width of the end caps
                return (
                  <g key={`ly${idx}`} opacity={op}>
                    {ly.x.map((xv, i) => {
                      const yv = ly.y[i];
                      if (!Number.isFinite(xv) || !Number.isFinite(yv)) return null;
                      const px = sx(xv), py = mySy(yv);
                      if (!Number.isFinite(px) || !Number.isFinite(py)) return null;
                      const eNeg = Number(eN[i]) || 0;
                      const ePos = Number(eP[i]) || 0;
                      const yLo = mySy(yv - eNeg);
                      const yHi = mySy(yv + ePos);
                      return (
                        <g key={i}>
                          {(eNeg !== 0 || ePos !== 0) && (
                            <>
                              <line x1={px} x2={px} y1={yLo} y2={yHi}
                                stroke={ly.color} strokeWidth={Math.max(1, w * 0.7)} />
                              <line x1={px - cap} x2={px + cap} y1={yLo} y2={yLo}
                                stroke={ly.color} strokeWidth={Math.max(1, w * 0.7)} />
                              <line x1={px - cap} x2={px + cap} y1={yHi} y2={yHi}
                                stroke={ly.color} strokeWidth={Math.max(1, w * 0.7)} />
                            </>
                          )}
                          <circle cx={px} cy={py} r={2.5} fill={ly.color} />
                        </g>
                      );
                    })}
                  </g>
                );
              }
              // 'line' or 'stairs'
              let d = '';
              let started = false;
              const markerPts = [];   // collect finite pts for overlay
              // Downsample the visible x-range to ~W pixel columns for huge
              // series — render cost O(N) → O(W). Skipped for comet
              // animation (it steps through the raw points one by one).
              // Decimated points precomputed once per (viewport, width,
              // algorithm) in the decimatedSeries memo above — O(W) per frame
              // at any zoom (engine tile / LOD pyramid / preview). comet falls
              // through to raw.
              const sr = decimatedSeries[idx] || { x: ly.x, y: ly.y };
              const SX = sr.x, SY = sr.y;
              // Comet animation: render only first floor(progress·N) points.
              const totalN = SX.length;
              const animN = ly.cometAnim
                ? Math.max(1, Math.floor(cometProgress * totalN))
                : totalN;
              for (let i = 0; i < animN; i++) {
                const xv = SX[i], yv = SY[i];
                // Genuine non-finite data → break the line (MATLAB gap
                // semantics for plot([1 NaN 3])).
                if (!Number.isFinite(xv) || !Number.isFinite(yv)) { started = false; continue; }
                // Log axis: skip a ≤0 value WITHOUT breaking (connect across).
                if (dropOnLog(xv, yv)) continue;
                const px = sx(xv), py = mySy(yv);
                if (!Number.isFinite(px) || !Number.isFinite(py)) { started = false; continue; }
                if (mode === 'stairs' && started) {
                  d += `L${Math.round(px)},${Math.round(mySy(SY[i - 1]))} `;
                }
                d += (started ? 'L' : 'M') + Math.round(px) + ',' + Math.round(py) + ' ';
                started = true;
                if (ly.marker) markerPts.push({ px, py });
              }
              // Linespec dash pattern. 'none' (or '') line-style would
              // suppress the path entirely (markers-only call) — emit no
              // <path> in that case.
              const ls = ly.lineStyle || '-';
              const dash = DASH_FOR[ls];
              const drawPath = ls !== 'none' && ls !== '';
              const r = ly.size || 4;
              return (
                <g key={`ly${idx}`} opacity={op}>
                  {drawPath && (
                    <path d={d} stroke={ly.color} fill="none"
                      strokeWidth={w} strokeDasharray={dash}
                      strokeLinejoin="round" strokeLinecap="round" />
                  )}
                  {ly.marker && markerPts.map((p, i) => (
                    <MarkerGlyph key={`m${i}`} idx={i} marker={ly.marker}
                      cx={p.px} cy={p.py} r={r} color={ly.color} filled={ly.filled} />
                  ))}
                </g>
              );
            }
            if (ly.kind === 'text') {
              const px = sx(ly.x), py = sy(ly.y);
              if (!Number.isFinite(px) || !Number.isFinite(py)) return null;
              return (
                <text key={`ly${idx}`} x={px} y={py}
                  fill={ly.color} fontSize={(ly.fontSize || 11) * fontScale}
                  className="hm-overlay-text"
                  pointerEvents="none">{ly.text}</text>
              );
            }
            return null;
}
