/**
 * Subplot grid renderer. Lays out adapted cells in a rows×cols tile and
 * forwards each cell to the right per-kind renderer (CompositePlot for the
 * unified 2-D path, PolarPlot for polar). Each cell keeps its own viewport
 * so pan/zoom is independent per panel.
 *
 * Figure shape:
 *   { kind: 'subplot', id, title, grid:[rows, cols],
 *     cells: [{ ...adaptedFigure, subplotIndex }] }
 */
import { useState, useEffect, useRef } from 'react';
import CompositePlot from './CompositePlot';
import Composite3DPlot from './Composite3DPlot';
import FigureErrorBoundary from './FigureErrorBoundary';
import PolarPlot, { defaultPolarViewport } from './PolarPlot';

function renderCell(cell, props) {
  if (cell.kind === 'composite3d') {
    return (
      <FigureErrorBoundary label="composite3d-cell" figureId={cell.id}
        width={props.width} height={props.height}>
        <Composite3DPlot figure={cell} {...props} />
      </FigureErrorBoundary>
    );
  }
  if (cell.kind === 'composite')   return <CompositePlot   figure={cell} {...props} />;
  if (cell.kind === 'polar')       return <PolarPlot       figure={cell} {...props} />;
  return <CompositePlot figure={cell} {...props} />;
}

function defaultViewport(cell) {
  if (cell.kind === 'polar') return defaultPolarViewport(cell);
  if (cell.xRange && cell.yRange) {
    return { x: cell.xRange.slice(), y: cell.yRange.slice() };
  }
  return { x: [-1, 1], y: [-1, 1] };
}

function sameViewport(a, b) {
  if (!a || !b) return a === b;
  if (a.x && a.y && b.x && b.y) {
    return a.x[0] === b.x[0] && a.x[1] === b.x[1]
        && a.y[0] === b.y[0] && a.y[1] === b.y[1];
  }
  // polar viewport shape: { rmin, rmax, ... }
  return a.rmin === b.rmin && a.rmax === b.rmax;
}

export default function SubplotGrid({
  figure, width, height,
  fontScale = 1,
  major = true,
  minor = false,
  // Visibility flags from FigureWindow's display ▾ menu — fanned out
  // to every cell so a single toggle applies to the whole subplot
  // grid. CompositePlot / Composite3DPlot inside each cell decides if
  // the flag actually has anything to apply (e.g. cells without an
  // ylabel ignore showYLabel).
  showTitle  = true,
  showXLabel = true,
  showYLabel = true,
  showZLabel = true,
  // Display setters fanned out to each cell so the per-cell right-click
  // menu can offer the same Display submenu as the toolbar. Toggles
  // mutate the figure-wide state in FigureWindow, so a click in any cell
  // updates every cell at once — matches the toolbar semantics.
  setShowMajor  = null,
  setShowMinor  = null,
  setShowTitle  = null,
  setShowXLabel = null,
  setShowYLabel = null,
  setShowLegend = null,
  // Colorbar visibility — figure-wide; falls through to each cell as
  // showColorbar prop, with per-cell override pattern (same as
  // major/minor/title etc.) for the per-cell ПКМ.
  showColorbar = null,
  setShowColorbar = null,
  // Figure-wide colormap override from FigureWindow's toolbar. Used as
  // the FALLBACK for cells without their own per-cell override; when it
  // changes (toolbar pick), per-cell overrides are dropped — toolbar
  // wins ("панель = ко всем").
  colormapOverride = null,
  setColormapOverride = null,
  // ПКМ bridge — top-level reset + save/export handlers. Same per-cell
  // ContextMenu ends up surfacing them.
  onResetAll          = null,
  onExportSvg         = null,
  onExportPng2x       = null,
  onExportPngPrint85  = null,
  onExportPngPrint170 = null,
  onExportPngPrint210 = null,
  onExportCsv         = null,
  onExportTsv         = null,
  onExportJson        = null,
  xLog,
  yLog,
  zLog,
  // FigureWindow's fit ▾ button increments fitSignal.n to ask SubplotGrid
  // to reset every cell's viewport along fitSignal.axis ('both'|'x'|'y'|'z').
  // Counter pattern so React reliably notices a re-fit even when the same
  // axis is requested twice in a row.
  fitSignal = null,
  // Toolbar Reset / display ▾ Reset increments displayResetSignal.n to
  // tell SubplotGrid to drop every cell's per-cell display override
  // back to the figure-wide value. Counter pattern (same as fitSignal).
  displayResetSignal = null,
  // Per-cell state (single source of truth) lives in FigureWindow now.
  // SubplotGrid receives the resolved cellState array + per-cell setter
  // factories from the parent and fans them out to each cell's
  // CompositePlot.
  cellState = [],
  makeCellDisplaySetter = null,
  makeCellColormapSetter = null,
  makeCellDisplayReset = null,
  makeCellColormapReset = null,
  interactive = true,
  engine = null,
}) {
  const [rows, cols] = figure.grid;
  const cellW = Math.floor(width  / cols);
  const cellH = Math.floor(height / rows);

  // Empty subplot slots — script called subplot(R,C,k) for some but not all
  // k in [1, R*C]. MATLAB shows these as plain figure background; we draw a
  // faded placeholder frame + "not set" label so the user sees the grid
  // shape and can tell that slot was skipped intentionally.
  const filledSlots = new Set(
    figure.cells.map((cell, idx) => (cell.subplotIndex || idx + 1) - 1)
  );
  const emptySlots = [];
  for (let p = 0; p < rows * cols; p++) {
    if (!filledSlots.has(p)) emptySlots.push(p);
  }

  // Per-cell viewports. Identity refresh on figure change so a re-run script
  // doesn't leak stale ranges into the new data.
  const [viewports, setViewports] = useState(() => figure.cells.map(defaultViewport));
  // SubplotGrid CAN persist across figure re-runs when the figure id is
  // reused (close all + run a different script that lands on Figure 1).
  // useState's initial only fires on mount, so when cells change shape
  // (e.g. 3 → 5 cells, polar → cartesian) viewports stays stale and
  // viewports[idx] is undefined for new cells → CompositePlot crashes on
  // viewport.x[0]. Re-init on cell count or figure-id change.
  //
  // We also re-init when a cell's xRange/yRange changes while the cell's
  // viewport still matches its previous default. This catches the
  // imhist-in-subplot case: subplot(2,2,3) creates cell 3 with the
  // inherited axes from cell 2 (e.g. an imshow's [0.5, 64.5]), then
  // imhist pushes bar data and the cell's xRange updates to [-0.04, 1.04].
  // Without this refresh the bars compress to a 1-pixel strip on the
  // left of the preview because viewports[2] is locked at [0.5, 64.5].
  // We only refresh cells whose viewport still equals the previous default
  // (i.e. user hasn't pan/zoomed), so interactive panning isn't reset on
  // every script re-run.
  const lastShapeRef = useRef('');
  const lastDefaultsRef = useRef([]);
  // Track the last-handled fitSignal counter so the effect doesn't
  // re-fit on every render — only when n changes.
  const lastFitNRef = useRef(0);
  useEffect(() => {
    if (!fitSignal || fitSignal.n === lastFitNRef.current) return;
    lastFitNRef.current = fitSignal.n;
    setViewports((prev) => figure.cells.map((cell, idx) => {
      const def = defaultViewport(cell);
      const cur = prev[idx] || def;
      const axis = fitSignal.axis;
      // Polar cells use a different viewport shape — applying x/y axis
      // fits to them is undefined; fall back to the default viewport
      // for 'both' only.
      if (cell.kind === 'polar') return axis === 'both' ? def : cur;
      if (axis === 'both') return def;
      if (axis === 'x') return { ...cur, x: def.x };
      if (axis === 'y') return { ...cur, y: def.y };
      if (axis === 'z') return def.z ? { ...cur, z: def.z } : cur;
      return cur;
    }));
  }, [fitSignal, figure.cells]);

  // Per-cell display + colormap overrides are owned by FigureWindow now
  // (it needs the data to compute aggregate "✓ on every cell?" marks
  // for the toolbar). We only consume them here via the props above.
  useEffect(() => {
    const shape = `${figure.id}:${figure.cells.length}:${figure.cells.map((c) => c.kind).join(',')}`;
    const newDefaults = figure.cells.map(defaultViewport);
    if (shape !== lastShapeRef.current) {
      lastShapeRef.current = shape;
      lastDefaultsRef.current = newDefaults;
      setViewports(newDefaults);
      return;
    }
    // Same shape — refresh per-cell viewports whose previous default
    // matches the current viewport (== untouched by user pan/zoom).
    setViewports((prev) => {
      const next = prev.slice();
      let changed = false;
      for (let i = 0; i < figure.cells.length; i++) {
        const oldDef = lastDefaultsRef.current[i];
        const newDef = newDefaults[i];
        if (!sameViewport(oldDef, newDef) && sameViewport(prev[i], oldDef)) {
          next[i] = newDef;
          changed = true;
        }
      }
      lastDefaultsRef.current = newDefaults;
      return changed ? next : prev;
    });
  }, [figure.id, figure.cells]);

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
              // Fallback for the brief render BEFORE useEffect re-inits
              // viewports on shape change — without it, viewports[idx]
              // could be undefined and CompositePlot crashes on
              // viewport.x[0].
              viewport:    viewports[idx] || defaultViewport(cell),
              setViewport: (v) => setViewports((prev) => {
                const next = prev.slice();
                // linkaxes — mirror the new viewport's linked axes across
                // every other cell. We don't mirror polar cells: their
                // viewport shape is { rmin, rmax, ... }, not {x, y}.
                const mode = figure.linkMode || '';
                const linkX = mode === 'x' || mode === 'xy';
                const linkY = mode === 'y' || mode === 'xy';
                if (linkX || linkY) {
                  for (let i = 0; i < next.length; i++) {
                    if (i === idx) continue;
                    const cur = next[i];
                    if (!cur || !Array.isArray(cur.x) || !Array.isArray(cur.y)) continue;
                    next[i] = {
                      ...cur,
                      x: linkX ? v.x.slice() : cur.x.slice(),
                      y: linkY ? v.y.slice() : cur.y.slice(),
                    };
                  }
                }
                next[idx] = v;
                return next;
              }),
              ...(() => {
                // Read this cell's state directly — single source of
                // truth, no override resolution. Setter factories may
                // be absent in preview-card mode (FiguresPane doesn't
                // pass them); guard with no-op fallbacks.
                const s = cellState[idx] || {};
                const mks = (...a) => makeCellDisplaySetter ? makeCellDisplaySetter(...a) : null;
                const mkc = (...a) => makeCellColormapSetter ? makeCellColormapSetter(...a) : null;
                const mkdr = (...a) => makeCellDisplayReset ? makeCellDisplayReset(...a) : null;
                const mkcr = (...a) => makeCellColormapReset ? makeCellColormapReset(...a) : null;
                return {
                  major: !!s.showMajor, minor: !!s.showMinor,
                  xLog: !!s.xLog, yLog: !!s.yLog,
                  showTitle: s.showTitle !== false,
                  showXLabel: s.showXLabel !== false,
                  showYLabel: s.showYLabel !== false,
                  showZLabel: s.showZLabel !== false,
                  showLegend: !!s.showLegend,
                  showColorbar: !!s.showColorbar,
                  setShowMajor:    mks(idx, 'showMajor'),
                  setShowMinor:    mks(idx, 'showMinor'),
                  setXLog:         mks(idx, 'xLog'),
                  setYLog:         mks(idx, 'yLog'),
                  setShowTitle:    mks(idx, 'showTitle'),
                  setShowXLabel:   mks(idx, 'showXLabel'),
                  setShowYLabel:   mks(idx, 'showYLabel'),
                  setShowLegend:   mks(idx, 'showLegend'),
                  setShowColorbar: mks(idx, 'showColorbar'),
                  // colormap field on the cell entry IS the override
                  // (null = follow script). CompositePlot reads it as
                  // colormapOverride.
                  colormapOverride: s.colormap != null ? s.colormap : null,
                  setColormapOverride: mkc(idx),
                  onDisplayReset:  mkdr(idx),
                  onColormapReset: mkcr(idx),
                };
              })(),
              zLog,
              // ПКМ Reset row inside a subplot cell must reset ONLY this
              // cell — never fan out to siblings. Combine the per-cell
              // display+colormap resets with a per-cell viewport reset
              // (defaultViewport for this cell). Toolbar 🏠 Reset still
              // does figure-wide via FigureWindow's resetAll.
              onResetAll: () => {
                if (makeCellDisplayReset)  makeCellDisplayReset(idx)();
                if (makeCellColormapReset) makeCellColormapReset(idx)();
                setViewports((prev) => {
                  const next = prev.slice();
                  next[idx] = defaultViewport(figure.cells[idx]);
                  return next;
                });
              },
              // Image + data exports inside a subplot cell must save
              // ONLY that cell's content. We deliberately DON'T forward
              // the figure-wide handlers from FigureWindow — CompositePlot
              // falls back to its local exporters which act on the cell's
              // own SVG ref + the cell's `figure.layers` data.
              fontScale: subFont,
              interactive,
              engine,
            })}
          </div>
        );
      })}
      {emptySlots.map((p) => {
        const r = Math.floor(p / cols);
        const c = p % cols;
        const inset = 12;
        return (
          <div key={`empty-${p}`} className="sg-empty-slot" style={{
            position: 'absolute',
            left: c * cellW + inset, top: r * cellH + inset,
            width: cellW - 2 * inset, height: cellH - 2 * inset,
          }}>
            <span>not set</span>
          </div>
        );
      })}
    </div>
  );
}
