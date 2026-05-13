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

  // ── Per-cell display overrides ────────────────────────────────────
  // Right-click toggles inside a single cell store overrides here so
  // a sub-plot user can flip grid/log/labels for ONE cell without
  // affecting the others. Shape: array<{ major?, minor?, xLog?, yLog?,
  // showTitle?, showXLabel?, showYLabel?, showLegend? }>. An undefined
  // entry means "use the figure-wide value passed via props".
  //
  // The toolbar Reset / display ▾ Reset increments displayResetSignal
  // which clears every override back to {} so the cells follow the
  // global toggles again.
  const [cellDisplay, setCellDisplay] = useState(() => figure.cells.map(() => ({})));
  useEffect(() => {
    setCellDisplay((prev) => {
      // Re-init when cell count changed; otherwise keep existing overrides.
      if (prev.length === figure.cells.length) return prev;
      return figure.cells.map((_, i) => prev[i] || {});
    });
  }, [figure.cells.length]);
  const lastDisplayResetNRef = useRef(0);
  useEffect(() => {
    if (!displayResetSignal || displayResetSignal.n === lastDisplayResetNRef.current) return;
    lastDisplayResetNRef.current = displayResetSignal.n;
    setCellDisplay(figure.cells.map(() => ({})));
  }, [displayResetSignal, figure.cells.length]);
  // Per-cell setter factory. Returns a function with the same updater
  // shape as React's setState, so callers can write `cellSetter((v) => !v)`.
  // When the override is unset, we read the figure-wide effective value
  // (passed in via the resolveDefault closure) so toggling starts from
  // the visible state.
  const makeCellSetter = (idx, key, resolveDefault) => (updater) => {
    setCellDisplay((prev) => {
      const next = prev.slice();
      const entry = next[idx] || {};
      const cur = entry[key] !== undefined ? entry[key] : resolveDefault();
      const value = typeof updater === 'function' ? updater(cur) : updater;
      next[idx] = { ...entry, [key]: value };
      return next;
    });
  };
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
                // Resolve effective per-cell display values: per-cell
                // override wins, otherwise fall back to the figure-wide
                // value passed in via props. Per-cell setters wrap
                // makeCellSetter so a ПКМ click only mutates this cell.
                const o = cellDisplay[idx] || {};
                const eMajor     = o.major     !== undefined ? o.major     : major;
                const eMinor     = o.minor     !== undefined ? o.minor     : minor;
                const eXLog      = o.xLog      !== undefined ? o.xLog      : xLog;
                const eYLog      = o.yLog      !== undefined ? o.yLog      : yLog;
                const eShowTitle  = o.showTitle  !== undefined ? o.showTitle  : showTitle;
                const eShowXLabel = o.showXLabel !== undefined ? o.showXLabel : showXLabel;
                const eShowYLabel = o.showYLabel !== undefined ? o.showYLabel : showYLabel;
                const eShowLegend = o.showLegend !== undefined ? o.showLegend : true;
                return {
                  major: eMajor, minor: eMinor,
                  xLog: eXLog, yLog: eYLog,
                  showTitle: eShowTitle, showXLabel: eShowXLabel,
                  showYLabel: eShowYLabel, showZLabel,
                  showLegend: eShowLegend,
                  setShowMajor:  makeCellSetter(idx, 'major',     () => eMajor),
                  setShowMinor:  makeCellSetter(idx, 'minor',     () => eMinor),
                  setXLog:       makeCellSetter(idx, 'xLog',      () => eXLog),
                  setYLog:       makeCellSetter(idx, 'yLog',      () => eYLog),
                  setShowTitle:  makeCellSetter(idx, 'showTitle',  () => eShowTitle),
                  setShowXLabel: makeCellSetter(idx, 'showXLabel', () => eShowXLabel),
                  setShowYLabel: makeCellSetter(idx, 'showYLabel', () => eShowYLabel),
                  setShowLegend: makeCellSetter(idx, 'showLegend', () => eShowLegend),
                };
              })(),
              zLog,
              onResetAll,
              onExportSvg, onExportPng2x,
              onExportPngPrint85, onExportPngPrint170, onExportPngPrint210,
              onExportCsv, onExportTsv, onExportJson,
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
