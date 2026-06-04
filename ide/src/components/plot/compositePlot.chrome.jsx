// compositePlot.chrome.jsx — non-data render chrome for CompositePlot.
// renderLegend draws the legend box (a swatch + label per series) when the
// script called legend() or set a Location. Pure (ctx) => JSX | null.
import { resolveSeriesName } from './compositePlot.helpers';

export function renderLegend(ctx) {
  const { showLegend, figure, seriesLayers, fontScale, legendLocationProp,
    padL, padT, W, H } = ctx;
        if (showLegend === false) return null;
        const userAsked = (figure.legend && figure.legend.length > 0)
                       || (figure.legendLocation && figure.legendLocation !== 'none');
        if (!userAsked || seriesLayers.length === 0) return null;
        // When legendLocation is set but no labels — fall back to the
        // per-layer auto-names so the box still has content.
        const labels = (figure.legend && figure.legend.length > 0)
          ? figure.legend
          : seriesLayers.map((l) => l.name).filter(Boolean);
        const haveLabels = labels.some((s) => s && s.trim() !== '');
        if (!haveLabels) return null;
        const items = seriesLayers.slice(0, labels.length).map((l, i) => ({
          color: l.color,
          text: resolveSeriesName(figure, l, i),
          // Mode drives swatch shape: 'circle' for point-like marks,
          // 'rect' for filled-region marks, 'line' for everything else.
          mode: l.mode || 'line',
        }));
        if (items.length === 0) return null;
        const fontSize = 10 * fontScale;
        const lineH    = fontSize + 4;
        const swatchW  = 14;
        const padInner = 6;
        // Approximate text width: 6.5 px per char at fontSize ≈ 10.
        // Conservative; SVG won't reflow but the box won't be tiny.
        const longest = items.reduce((m, it) => Math.max(m, it.text.length), 0);
        const boxW = padInner * 2 + swatchW + 4 + Math.min(longest, 24) * 6.5;
        const boxH = padInner * 2 + items.length * lineH;
        const loc = ((legendLocationProp != null ? legendLocationProp : figure.legendLocation) || 'best')
                    .replace(/outside$/, '');
        // Resolve box anchor to (x, y) inside the panel rect.
        const anchorMargin = 8;
        let bx, by;
        switch (loc) {
          case 'north':
            bx = padL + (W - boxW) / 2; by = padT + anchorMargin; break;
          case 'south':
            bx = padL + (W - boxW) / 2; by = padT + H - boxH - anchorMargin; break;
          case 'east':
            bx = padL + W - boxW - anchorMargin;
            by = padT + (H - boxH) / 2; break;
          case 'west':
            bx = padL + anchorMargin; by = padT + (H - boxH) / 2; break;
          case 'northwest':
            bx = padL + anchorMargin; by = padT + anchorMargin; break;
          case 'southeast':
            bx = padL + W - boxW - anchorMargin;
            by = padT + H - boxH - anchorMargin; break;
          case 'southwest':
            bx = padL + anchorMargin;
            by = padT + H - boxH - anchorMargin; break;
          case 'none':
            return null;
          // 'best' / 'northeast' / unrecognised → top-right corner.
          default:
            bx = padL + W - boxW - anchorMargin;
            by = padT + anchorMargin; break;
        }
        return (
          <g pointerEvents="none">
            {/* legendBoxOn=false hides the frame stroke + bg fill;
                the swatches + labels still render. */}
            {figure.legendBoxOn !== false && (
              <rect x={bx} y={by} width={boxW} height={boxH}
                fill="var(--plot-bg)" stroke="var(--plot-frame)" strokeWidth="0.5"
                rx="3" opacity="0.92" />
            )}
            {items.map((it, i) => {
              const cy = by + padInner + i * lineH + lineH / 2;
              const swX0 = bx + padInner;
              const swX1 = swX0 + swatchW;
              // Swatch matches the series mode so the legend keeps a
              // visual link to the actual mark on the plot.
              let swatch;
              if (it.mode === 'scatter' || it.mode === 'stem') {
                swatch = <circle cx={(swX0 + swX1) / 2} cy={cy} r="3"
                  fill={it.color} stroke="var(--plot-frame)" strokeWidth="0.6" />;
              } else if (it.mode === 'bar' || it.mode === 'barh' || it.mode === 'area') {
                swatch = <rect x={swX0} y={cy - 4} width={swatchW} height="8"
                  fill={it.color} fillOpacity={it.mode === 'area' ? 0.3 : 1}
                  stroke={it.color} strokeWidth="1" />;
              } else {
                // line / stairs / errorbar / quiver / default
                swatch = <line x1={swX0} x2={swX1} y1={cy} y2={cy}
                  stroke={it.color} strokeWidth="2" />;
              }
              return (
                <g key={`leg${i}`}>
                  {swatch}
                  <text x={swX1 + 4} y={cy + fontSize / 3}
                    fill="var(--plot-text)" fontSize={fontSize} textAnchor="start">
                    {it.text}
                  </text>
                </g>
              );
            })}
          </g>
        );
}
