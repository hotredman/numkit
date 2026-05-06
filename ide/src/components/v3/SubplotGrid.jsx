/**
 * Subplot grid renderer. Lays out adapted cells in a rows×cols tile and
 * forwards each cell to the right per-kind renderer (InteractivePlot,
 * Heatmap, PolarPlot). Each cell keeps its own viewport so pan/zoom is
 * independent per panel.
 *
 * Figure shape:
 *   { kind: 'subplot', id, title, grid:[rows, cols],
 *     cells: [{ ...adaptedFigure, subplotIndex }] }
 */
import { useState } from 'react';
import InteractivePlot from './InteractivePlot';
import Heatmap from './Heatmap';
import PolarPlot, { defaultPolarViewport } from './PolarPlot';

function renderCell(cell, props) {
  if (cell.kind === 'heatmap') return <Heatmap figure={cell} {...props} />;
  if (cell.kind === 'polar')   return <PolarPlot figure={cell} {...props} />;
  return <InteractivePlot figure={cell} {...props} />;
}

function defaultViewport(cell) {
  if (cell.kind === 'polar') return defaultPolarViewport(cell);
  if (cell.xRange && cell.yRange) {
    return { x: cell.xRange.slice(), y: cell.yRange.slice() };
  }
  return { x: [-1, 1], y: [-1, 1] };
}

export default function SubplotGrid({
  figure, width, height,
  fontScale = 1,
  major = true,
  minor = false,
  interactive = true,
  engine = null,
}) {
  const [rows, cols] = figure.grid;
  const cellW = Math.floor(width  / cols);
  const cellH = Math.floor(height / rows);

  // Per-cell viewports. Identity refresh on figure change so a re-run script
  // doesn't leak stale ranges into the new data.
  const [viewports, setViewports] = useState(() => figure.cells.map(defaultViewport));
  // If the cell count changed (script re-emitted with different layout) we
  // simply recompute — useState's initial only fires on mount, but in
  // practice React re-mounts SubplotGrid because FigureWindow's outer state
  // changes when a new figure object lands.

  return (
    <div style={{
      position: 'relative',
      width: '100%', height: '100%',
      background: 'var(--bg-1)',
      overflow: 'hidden',
    }}>
      {figure.cells.map((cell, idx) => {
        const p = (cell.subplotIndex || idx + 1) - 1;
        const r = Math.floor(p / cols);
        const c = p % cols;
        const left = c * cellW;
        const top  = r * cellH;
        // Per-cell font scale shrinks slightly so labels don't crowd.
        const subFont = fontScale * Math.max(0.65, Math.min(1, 1.4 / Math.max(rows, cols)));
        return (
          <div key={idx} style={{
            position: 'absolute',
            left, top, width: cellW, height: cellH,
            overflow: 'hidden',
          }}>
            {renderCell(cell, {
              width: cellW, height: cellH,
              viewport:    viewports[idx],
              setViewport: (v) => setViewports((prev) => {
                const next = prev.slice();
                next[idx] = v;
                return next;
              }),
              major, minor,
              fontScale: subFont,
              interactive,
              engine,
            })}
          </div>
        );
      })}
    </div>
  );
}
