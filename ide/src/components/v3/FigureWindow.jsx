import { useEffect, useLayoutEffect, useRef, useState } from 'react';
import CompositePlot from './CompositePlot';
import Composite3DPlot from './Composite3DPlot';
import FigureErrorBoundary from './FigureErrorBoundary';
import PolarPlot, { defaultPolarViewport, nicePolarMax } from './PolarPlot';
import SubplotGrid from './SubplotGrid';
import { computeFitViewport, fitCellViewport,
  composeSvgsToString, exportSvgString, exportPngString,
  downloadBlob as utilDownloadBlob } from './plotUtils';
import { initAxesFromCell, getProp, setProp, setAllAxes, setAxesAt,
  everyAxes, isOn, onOff,
  defaultViewport, cellsArrayFromFigure, aggColormap } from './figureSchema';

function renderFigure(figure, props, threeRef) {
  if (figure.kind === 'subplot')     return <SubplotGrid     figure={figure} {...props} />;
  if (figure.kind === 'composite3d') {
    return (
      <FigureErrorBoundary label="composite3d-modal" figureId={figure.id}
        width={props.width} height={props.height}>
        <Composite3DPlot ref={threeRef} figure={figure} {...props} />
      </FigureErrorBoundary>
    );
  }
  if (figure.kind === 'composite')   return <CompositePlot   figure={figure} {...props} />;
  if (figure.kind === 'polar')       return <PolarPlot       figure={figure} {...props} />;
  return <CompositePlot figure={figure} {...props} />;
}

/** True when the 3-D viewport is still the (−1, 1) cube placeholder
 *  set up at mount before onBBox reports the real data extent. */
function isPlaceholder3D(v) {
  if (!v || !v.x || !v.y || !v.z) return true;
  return v.x[0] === -1 && v.x[1] === 1
      && v.y[0] === -1 && v.y[1] === 1
      && v.z[0] === -1 && v.z[1] === 1;
}

function NumberInput({ value, onCommit, width = 88 }) {
  const [v, setV] = useState(value);
  useEffect(() => { setV(value); }, [value]);
  return (
    <input
      type="text"
      value={typeof v === 'number' ? Number(v.toFixed(6)).toString() : v}
      onChange={(e) => setV(e.target.value)}
      onKeyDown={(e) => {
        if (e.key === 'Enter')  { e.target.blur(); }
        if (e.key === 'Escape') { setV(value); e.target.blur(); }
      }}
      onBlur={() => {
        const n = parseFloat(v);
        if (Number.isFinite(n)) onCommit(n);
        else setV(value);
      }}
      className="fw-num-input"
      style={{ width }}
    />
  );
}

/** Per-series rows for fit ▾. When `rows.length` is over `threshold`
 *  (default 5), wraps the list in a side-opening submenu so the
 *  popover doesn't grow tall. Below the threshold, renders inline.
 *  Each row is a JSX element produced by the caller.
 *
 *  Submenu uses position:fixed with coords computed from the trigger
 *  button's getBoundingClientRect — bypasses the parent .fw-pop's
 *  overflow:auto (which would otherwise clip the absolute-positioned
 *  child and surface a scrollbar instead of opening). */
/** Side-opening submenu for picking one location value from a fixed
 *  list. Used by display ▾ for legend / colorbar location pickers.
 *  `value` is the current selection; null = "follow script". `options`
 *  is `[{ value, label }]`. ✓ marks the active one. */
function FwPopLocationSubmenu({ label, value, options, onPick }) {
  const [open, setOpen] = useState(false);
  const [coords, setCoords] = useState(null);
  const triggerRef = useRef(null);
  useLayoutEffect(() => {
    if (!open) return;
    const el = triggerRef.current;
    if (!el) return;
    const r = el.getBoundingClientRect();
    setCoords({ left: r.right + 2, top: r.top - 4 });
  }, [open]);
  return (
    <div className={`fw-pop-sub-wrap ${open ? 'is-open' : ''}`}
         onMouseEnter={() => setOpen(true)}
         onMouseLeave={() => setOpen(false)}>
      <button ref={triggerRef}
              className="fw-pop-sub-trigger"
              onClick={(e) => { e.stopPropagation(); setOpen((o) => !o); }}>
        <span>{label}</span>
        <span className="fw-pop-sub-arrow">▶</span>
      </button>
      {open && coords && (
        <div className="fw-pop fw-pop-sub"
             style={{ position: 'fixed', left: coords.left, top: coords.top }}>
          {options.map((o) => {
            const active = (value || null) === o.value;
            return (
              <button key={String(o.value)}
                      className="fw-pop-toggle"
                      onClick={() => { onPick(o.value); setOpen(false); }}>
                <span>{o.label}</span>
                <span className="fw-pop-check">{active ? '✓' : ''}</span>
              </button>
            );
          })}
        </div>
      )}
    </div>
  );
}

function FwPopRowsOrSubmenu({ rows, label, threshold = 5 }) {
  const [open, setOpen] = useState(false);
  const [coords, setCoords] = useState(null);
  const triggerRef = useRef(null);
  useLayoutEffect(() => {
    if (!open) return;
    const el = triggerRef.current;
    if (!el) return;
    const r = el.getBoundingClientRect();
    setCoords({ left: r.right + 2, top: r.top - 4 });
  }, [open]);
  if (!Array.isArray(rows) || rows.length === 0) return null;
  if (rows.length <= threshold) {
    return <>{rows}</>;
  }
  return (
    <div className={`fw-pop-sub-wrap ${open ? 'is-open' : ''}`}
         onMouseEnter={() => setOpen(true)}
         onMouseLeave={() => setOpen(false)}>
      <button ref={triggerRef}
              className="fw-pop-sub-trigger"
              onClick={(e) => { e.stopPropagation(); setOpen((o) => !o); }}>
        <span>{label}</span>
        <span className="fw-pop-sub-arrow">▶</span>
      </button>
      {open && coords && (
        <div className="fw-pop fw-pop-sub"
             style={{ position: 'fixed', left: coords.left, top: coords.top }}>
          {rows}
        </div>
      )}
    </div>
  );
}

/** Toggle row for the axes / decoration popovers. Two-column grid:
 *  [ label | ✓ ]. No per-item icon (button-level icon already telegraphs
 *  the menu's purpose). No active colour tint — only the ✓ marker.
 *
 *  `masked` = state-preserving "this toggle is currently no-op because
 *  another setting masks it" hint. Renders dimmed via `is-masked` CSS
 *  but stays fully clickable (the user can pre-set a value that'll
 *  apply once the mask lifts). Distinct from `disabled`, which blocks
 *  the click entirely. */
function DisplayToggle({ label, active, disabled = false, disabledHint = '',
                        masked = false, maskedHint = '', onClick }) {
  const title = disabled ? disabledHint : (masked ? maskedHint : '');
  return (
    <button className={`fw-pop-toggle${masked ? ' is-masked' : ''}`}
            disabled={disabled}
            title={title}
            onClick={onClick}>
      <span>{label}</span>
      <span className="fw-pop-check">{active ? '✓' : ''}</span>
    </button>
  );
}

/** Header row for any matrix-layout popover section — labels the
 *  N button columns. Rendered above the per-axis rows so users read
 *  the column headings once. */
function MatrixHead({ labels }) {
  return (
    <div className="fw-pop-mrow fw-pop-mrow-head">
      <span className="fw-pop-mrow-label" />
      {labels.map((l, i) => <span key={i}>{l}</span>)}
    </div>
  );
}

/** Generic per-row matrix toggle. `label` is the row name (axis
 *  letter); `cols` is an array of column descriptors:
 *    { active, onClick, disabled?, title? }
 *  Each column renders as a square checkbox button (✓ when active).
 *  Used by:
 *    • grid ▾ matrix — cols = [major, minor]
 *    • axes ▾ matrix — cols = [reverse, log scale]
 *  Same `.fw-pop-mbtn` styling. Grid template columns set by the
 *  parent `.fw-pop-matrix` block based on column count. */
function MatrixToggleRow({ label, cols }) {
  return (
    <div className="fw-pop-mrow">
      <span className="fw-pop-mrow-label">{label}</span>
      {cols.map((c, i) => (
        <button key={i}
                className={`fw-pop-mbtn${c.active ? ' is-active' : ''}`}
                disabled={!!c.disabled}
                title={c.title || ''}
                onClick={c.onClick}>
          {c.active ? '✓' : ''}
        </button>
      ))}
    </div>
  );
}

export default function FigureWindow({ figure, onClose, engine = null }) {
  const isPolar   = figure.kind === 'polar';
  const isSubplot = figure.kind === 'subplot';
  const isComposite = figure.kind === 'composite';
  const is3D = figure.kind === 'composite3d';
  // Imperative handle on Composite3DPlot — exposed when the modal
  // hosts a 3-D figure. FigureWindow uses it for fit-3D, X/Y/Z input
  // wiring, and PNG / CSV export (canvas geometry has no SVG to
  // serialise).
  const threeRef = useRef(null);
  // Composite figures carry a heterogeneous layers[] array. Heatmap-specific
  // toolbar bits (color autoscale, colormap select, log toggle) gate on the
  // presence of a heatmap layer; the rest of the toolbar (fit, legend, range)
  // works off the series layers.
  const compositeLayers = isComposite && Array.isArray(figure.layers) ? figure.layers : [];
  const heatmapLayer = compositeLayers.find((l) => l.kind === 'heatmap') || null;
  const seriesLayers = compositeLayers.filter((l) => l.kind === 'series');
  const isHeatmap = !!heatmapLayer;
  const hasSeries = seriesLayers.length > 0;
  // Polar plots use {r:[lo,hi]}; cartesian use {x:[…], y:[…]}; subplots have
  // per-cell viewports managed inside SubplotGrid, so the top-level viewport
  // is just a placeholder that the toolbar's range/status helpers branch off.
  // For 3-D the data extent isn't on the figure prop directly — it's
  // computed by Composite3DPlot via its bbox helper. We start with a
  // safe placeholder ([-1, 1] cube) and fill in the real extent
  // through the onBBox callback below.
  // figDefault & viewport now live in cells[0].viewport (single source
  // of truth — see figureCellState.js). The non-subplot getter below
  // exposes a `viewport` / `setViewport` compat pair for legacy call
  // sites; subplot uses per-cell viewports inside SubplotGrid.
  const figDefault = isSubplot ? null : defaultViewport(figure);
  // 3-D bbox cache — Composite3DPlot reports it via onBBox each
  // figure rebuild. Used as the "fit to data" target.
  const [bbox3d, setBbox3d] = useState(null);
  // Reset 3-D viewport on ACTUAL figure swap (a different figure.id)
  // so stale lims from a previous figure don't carry over. Skip the
  // initial mount — Composite3DPlot's onBBox would otherwise lose to
  // this reset (child effects run before parent effects, so the auto-
  // fill viewport gets clobbered back to the placeholder before the
  // user sees anything).
  const lastFigureIdRef = useRef(figure.id);
  useEffect(() => {
    if (!is3D) return;
    if (lastFigureIdRef.current !== figure.id) {
      setViewport({ x: [-1, 1], y: [-1, 1], z: [-1, 1] });
      lastFigureIdRef.current = figure.id;
    }
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [figure.id]);
  // First bbox report fills the viewport with the actual data extent
  // — without this the X/Y/Z inputs would show -1 / 1 instead of the
  // data range until the user touched the Fit menu.
  function onComposite3DBBox(bbox) {
    setBbox3d(bbox);
    setViewport((cur) => {
      // Only auto-fill if the viewport is still the placeholder; once
      // the user committed an explicit input, leave it alone.
      if (!isPlaceholder3D(cur)) return cur;
      return {
        x: [bbox.xMin, bbox.xMax],
        y: [bbox.yMin, bbox.yMax],
        z: [bbox.zMin, bbox.zMax],
      };
    });
  }
  // ── AXES STATE — MATLAB HG2 schema (single source of truth) ──────
  //
  // Schema lives in ./figureSchema.js. We model the figure as an array
  // of Axes objects (one per cell; non-subplot figures have one).
  // Property names (XGrid, YGrid, XScale, Title.Visible, ...) match
  // MATLAB R2025b — see figureSchema.js for the type definition.
  //
  // Toolbar setters fan an update across every Axes; ПКМ setters
  // mutate one. Aggregate ✓ in the toolbar = `every Axes has prop on`.
  // Reset = re-init from script via initAxesFromCell().
  const cellsArr = cellsArrayFromFigure(figure);
  const [axesArr, setAxesArr] = useState(() => cellsArr.map(initAxesFromCell));
  // Re-init on figure identity / shape change. Same-id same-shape
  // re-run keeps user toggles (ПКМ tweaks survive script re-runs).
  useEffect(() => {
    setAxesArr((prev) => {
      if (prev.length === cellsArr.length) return prev;
      return cellsArr.map((c, i) => prev[i] || initAxesFromCell(c));
    });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [figure.id, cellsArr.length]);

  // ── Legacy compat layer — reads ──────────────────────────────────
  // Down-stream code (CompositePlot, SubplotGrid, ПКМ Display submenu
  // etc.) still consumes flat boolean props (showMajor, xLog, ...).
  // Derive them from axesArr aggregates. This boundary lets us keep
  // the canonical state in MATLAB schema while not touching every
  // call site of the legacy API.
  // `grid on` in MATLAB lights every axis grid the current axes type
  // supports — XGrid/YGrid/ZGrid for cartesian + 3-D, RGrid/ThetaGrid
  // for polar. The combined toolbar "grid" toggle mirrors that: the
  // aggregate reads true when ANY of those is on; the fan-out
  // (legacyWrite) sets all of them. Polar fields stay 'off' on
  // cartesian cells per the schema initializer, so flipping the
  // combined toggle on a cartesian figure visually lights only X+Y
  // (R/θ are set in state but their renderer is in PolarPlot).
  function axisGridOn(axes) {
    return isOn(axes && axes.XGrid)     || isOn(axes && axes.YGrid)
        || isOn(axes && axes.ZGrid)
        || isOn(axes && axes.RGrid)     || isOn(axes && axes.ThetaGrid);
  }
  function axisGridMinorOn(axes) {
    // Minor grids on every axis the schema models — cartesian X/Y/Z
    // and polar R/θ. Mirrors MATLAB R2025b: `grid minor` lights the
    // minor grid for every axis the current axes type supports.
    return isOn(axes && axes.XMinorGrid)     || isOn(axes && axes.YMinorGrid)
        || isOn(axes && axes.ZMinorGrid)
        || isOn(axes && axes.RMinorGrid)     || isOn(axes && axes.ThetaMinorGrid);
  }
  // Adapter — same shape the old `cells: CellSettings[]` exposed.
  // Used by SubplotGrid (fed via the cellState renderFigure prop).
  function axesToLegacyCell(axes) {
    if (!axes) return {};
    return {
      showMajor:    axisGridOn(axes),
      showMinor:    axisGridMinorOn(axes),
      // Per-axis grid (preserves XGrid/YGrid info for SubplotGrid →
      // CompositePlot per-axis renderer split).
      xGrid:        isOn(axes.XGrid),
      yGrid:        isOn(axes.YGrid),
      xMinor:       isOn(axes.XMinorGrid),
      yMinor:       isOn(axes.YMinorGrid),
      zMinor:       isOn(axes.ZMinorGrid),
      rMinor:       isOn(axes.RMinorGrid),
      thetaMinor:   isOn(axes.ThetaMinorGrid),
      xLog:         axes.XScale === 'log',
      yLog:         axes.YScale === 'log',
      zLog:         axes.ZScale === 'log',
      showTitle:    isOn(axes.Title    && axes.Title.Visible),
      showXLabel:   isOn(axes.XLabel   && axes.XLabel.Visible),
      showYLabel:   isOn(axes.YLabel   && axes.YLabel.Visible),
      showZLabel:   isOn(axes.ZLabel   && axes.ZLabel.Visible),
      showLegend:   isOn(axes.Legend   && axes.Legend.Visible),
      showColorbar: isOn(axes.Colorbar && axes.Colorbar.Visible),
      showAxis:     isOn(axes.Visible),
      showBox:      isOn(axes.Box),
      xReverse:     axes.XDir === 'reverse',
      yReverse:     axes.YDir === 'reverse',
      zReverse:     axes.ZDir === 'reverse',
      // Aspect mode — UI-set value flows to CompositePlot's panel-
      // shrink path. Defaults to '' (auto) when neither script nor UI
      // set it. CompositePlot reads `axisMode` prop with fallback to
      // figure.axisMode (script value).
      axisMode:     axes.AxisMode || '',
      legendLocation:   axes.Legend   && axes.Legend.Location,
      colorbarLocation: axes.Colorbar && axes.Colorbar.Location,
      colormap:     axes.Colormap || null,
    };
  }
  const cells = axesArr.map(axesToLegacyCell);

  // Compat: viewport / setViewport read-write pair, derived from
  // axesArr[0]'s XLim/YLim/ZLim (or RLim for polar). For subplot the
  // figure-level viewport is null — per-cell viewports live inside
  // each axes entry.
  const viewport = isSubplot ? null : (viewportFromAxes(axesArr[0]) || figDefault);
  const setViewport = isSubplot ? null
    : (vOrFn) => setCellKey(0, 'viewport', vOrFn);

  // Aggregates (read-only views).
  // ── Legacy ↔ MATLAB schema bridges ───────────────────────────────
  // Map flat boolean keys used throughout the existing UI / renderer
  // to MATLAB property paths.
  function legacyRead(a, key) {
    if (!a) return undefined;
    switch (key) {
      case 'showMajor':    return axisGridOn(a);
      case 'showMinor':    return axisGridMinorOn(a);
      case 'xGrid':        return isOn(a.XGrid);
      case 'yGrid':        return isOn(a.YGrid);
      case 'zGrid':        return isOn(a.ZGrid);
      case 'rGrid':        return isOn(a.RGrid);
      case 'thetaGrid':    return isOn(a.ThetaGrid);
      case 'xMinor':       return isOn(a.XMinorGrid);
      case 'yMinor':       return isOn(a.YMinorGrid);
      case 'zMinor':       return isOn(a.ZMinorGrid);
      case 'rMinor':       return isOn(a.RMinorGrid);
      case 'thetaMinor':   return isOn(a.ThetaMinorGrid);
      case 'xLog':         return a.XScale === 'log';
      case 'yLog':         return a.YScale === 'log';
      case 'zLog':         return a.ZScale === 'log';
      case 'showTitle':    return isOn(a.Title    && a.Title.Visible);
      case 'showXLabel':   return isOn(a.XLabel   && a.XLabel.Visible);
      case 'showYLabel':   return isOn(a.YLabel   && a.YLabel.Visible);
      case 'showZLabel':   return isOn(a.ZLabel   && a.ZLabel.Visible);
      case 'showLegend':   return isOn(a.Legend   && a.Legend.Visible);
      case 'showColorbar': return isOn(a.Colorbar && a.Colorbar.Visible);
      case 'showAxis':     return isOn(a.Visible);
      case 'showBox':      return isOn(a.Box);
      case 'xReverse':     return a.XDir === 'reverse';
      case 'yReverse':     return a.YDir === 'reverse';
      case 'zReverse':     return a.ZDir === 'reverse';
      case 'axisMode':     return a.AxisMode || 'auto';
      case 'legendLocation':   return a.Legend   && a.Legend.Location;
      case 'colorbarLocation': return a.Colorbar && a.Colorbar.Location;
      case 'colormap':     return a.Colormap;
      case 'viewport':     return viewportFromAxes(a);
      default: return undefined;
    }
  }
  function legacyWrite(a, key, value) {
    switch (key) {
      case 'showMajor':    {
        // Combined "grid" toggle — fan out to every per-axis major
        // grid we model (MATLAB `grid on` does the same: lights every
        // grid the axes type supports). Polar fields fan too so polar
        // figures actually toggle visibly.
        const f = onOff(!!value);
        return { ...a, XGrid: f, YGrid: f, ZGrid: f, RGrid: f, ThetaGrid: f };
      }
      case 'showMinor':    {
        // Combined "minor" toggle — fan out to every per-axis minor
        // grid we model. Same fan rule as showMajor but for the minor
        // family.
        const f = onOff(!!value);
        return { ...a,
          XMinorGrid: f, YMinorGrid: f, ZMinorGrid: f,
          RMinorGrid: f, ThetaMinorGrid: f };
      }
      case 'xGrid':        return { ...a, XGrid:          onOff(!!value) };
      case 'yGrid':        return { ...a, YGrid:          onOff(!!value) };
      case 'zGrid':        return { ...a, ZGrid:          onOff(!!value) };
      case 'rGrid':        return { ...a, RGrid:          onOff(!!value) };
      case 'thetaGrid':    return { ...a, ThetaGrid:      onOff(!!value) };
      case 'xMinor':       return { ...a, XMinorGrid:     onOff(!!value) };
      case 'yMinor':       return { ...a, YMinorGrid:     onOff(!!value) };
      case 'zMinor':       return { ...a, ZMinorGrid:     onOff(!!value) };
      case 'rMinor':       return { ...a, RMinorGrid:     onOff(!!value) };
      case 'thetaMinor':   return { ...a, ThetaMinorGrid: onOff(!!value) };
      case 'xLog':         return { ...a, XScale: value ? 'log' : 'linear' };
      case 'yLog':         return { ...a, YScale: value ? 'log' : 'linear' };
      case 'zLog':         return { ...a, ZScale: value ? 'log' : 'linear' };
      case 'showTitle':    return setProp(a, ['Title',    'Visible'], onOff(!!value));
      case 'showXLabel':   return setProp(a, ['XLabel',   'Visible'], onOff(!!value));
      case 'showYLabel':   return setProp(a, ['YLabel',   'Visible'], onOff(!!value));
      case 'showZLabel':   return setProp(a, ['ZLabel',   'Visible'], onOff(!!value));
      case 'showLegend':   return setProp(a, ['Legend',   'Visible'], onOff(!!value));
      case 'showColorbar': return setProp(a, ['Colorbar', 'Visible'], onOff(!!value));
      case 'showAxis':     return { ...a, Visible: onOff(!!value) };
      case 'showBox':      return { ...a, Box:     onOff(!!value) };
      case 'xReverse':     return { ...a, XDir: value ? 'reverse' : 'normal' };
      case 'yReverse':     return { ...a, YDir: value ? 'reverse' : 'normal' };
      case 'zReverse':     return { ...a, ZDir: value ? 'reverse' : 'normal' };
      case 'axisMode':     {
        // Aspect — keeps the AxisMode shorthand in sync with the
        // derived MATLAB property pair (DataAspectRatioMode /
        // PlotBoxAspectRatioMode). Mirrors initAxesFromCell's mapping.
        const v = String(value || 'auto');
        return { ...a,
          AxisMode: v,
          DataAspectRatioMode:    (v === 'equal' || v === 'image') ? 'manual' : 'auto',
          PlotBoxAspectRatioMode: (v === 'square') ? 'manual' : 'auto',
        };
      }
      case 'legendLocation':   return setProp(a, ['Legend',   'Location'], value);
      case 'colorbarLocation': return setProp(a, ['Colorbar', 'Location'], value);
      case 'colormap':     return { ...a, Colormap: value };
      case 'viewport':     return applyViewport(a, value);
      default: return a;
    }
  }
  function viewportFromAxes(a) {
    if (!a) return null;
    if (Array.isArray(a.RLim)) {
      return { rmin: a.RLim[0], rmax: a.RLim[1] };
    }
    const out = {};
    if (Array.isArray(a.XLim)) out.x = a.XLim.slice();
    if (Array.isArray(a.YLim)) out.y = a.YLim.slice();
    if (Array.isArray(a.ZLim)) out.z = a.ZLim.slice();
    return Object.keys(out).length > 0 ? out : null;
  }
  function applyViewport(a, vp) {
    if (!vp) return a;
    const out = { ...a };
    if (Array.isArray(vp.x)) out.XLim = vp.x.slice();
    if (Array.isArray(vp.y)) out.YLim = vp.y.slice();
    if (Array.isArray(vp.z)) out.ZLim = vp.z.slice();
    if (vp.rmin != null && vp.rmax != null) out.RLim = [vp.rmin, vp.rmax];
    return out;
  }

  // ── Legacy aggregate readers ─────────────────────────────────────
  // `showMajor` ✓ shows when EVERY cell has at least one grid axis on
  // (cartesian X/Y/Z or polar R/θ). Matches `axisGridOn`'s per-cell
  // semantics so the toolbar ✓ flips coherently with the click action.
  const showMajor    = everyAxes(axesArr, ['XGrid'],     isOn)
                    || everyAxes(axesArr, ['YGrid'],     isOn)
                    || everyAxes(axesArr, ['ZGrid'],     isOn)
                    || everyAxes(axesArr, ['RGrid'],     isOn)
                    || everyAxes(axesArr, ['ThetaGrid'], isOn);
  // Same shape as showMajor — true iff EVERY cell has at least one
  // minor-grid axis on. Includes polar (R/θ) so the toolbar minor row
  // ✓ flips coherently for polar figures too.
  const showMinor    = everyAxes(axesArr, ['XMinorGrid'],     isOn)
                    || everyAxes(axesArr, ['YMinorGrid'],     isOn)
                    || everyAxes(axesArr, ['ZMinorGrid'],     isOn)
                    || everyAxes(axesArr, ['RMinorGrid'],     isOn)
                    || everyAxes(axesArr, ['ThetaMinorGrid'], isOn);
  const xLog         = axesArr.length > 0 && axesArr.every((a) => a.XScale === 'log');
  const yLog         = axesArr.length > 0 && axesArr.every((a) => a.YScale === 'log');
  const zLog         = axesArr.length > 0 && axesArr.every((a) => a.ZScale === 'log');
  const showTitle    = everyAxes(axesArr, ['Title',    'Visible']);
  const showXLabel   = everyAxes(axesArr, ['XLabel',   'Visible']);
  const showYLabel   = everyAxes(axesArr, ['YLabel',   'Visible']);
  const showZLabel   = everyAxes(axesArr, ['ZLabel',   'Visible']);
  const showLegend   = everyAxes(axesArr, ['Legend',   'Visible']);
  const showColorbar = everyAxes(axesArr, ['Colorbar', 'Visible']);
  // Per-axis aggregates — used by the new X grid / Y grid display ▾ rows.
  const xGrid        = everyAxes(axesArr, ['XGrid'], isOn);
  const yGrid        = everyAxes(axesArr, ['YGrid'], isOn);
  const zGrid        = everyAxes(axesArr, ['ZGrid'], isOn);
  const rGrid        = everyAxes(axesArr, ['RGrid'], isOn);
  const thetaGrid    = everyAxes(axesArr, ['ThetaGrid'], isOn);
  // Per-axis MINOR aggregates — fed to the new grid ▾ matrix layout
  // (one row per axis, two state-buttons per row: major + minor).
  const xMinor       = everyAxes(axesArr, ['XMinorGrid'],     isOn);
  const yMinor       = everyAxes(axesArr, ['YMinorGrid'],     isOn);
  const zMinor       = everyAxes(axesArr, ['ZMinorGrid'],     isOn);
  const rMinor       = everyAxes(axesArr, ['RMinorGrid'],     isOn);
  const thetaMinor   = everyAxes(axesArr, ['ThetaMinorGrid'], isOn);
  // MATLAB Visible / Box / XDir / YDir aggregates.
  const showAxis     = everyAxes(axesArr, ['Visible'], isOn);
  const showBox      = everyAxes(axesArr, ['Box'], isOn);
  const xReverse     = axesArr.length > 0 && axesArr.every((a) => a.XDir === 'reverse');
  const yReverse     = axesArr.length > 0 && axesArr.every((a) => a.YDir === 'reverse');
  const zReverse     = axesArr.length > 0 && axesArr.every((a) => a.ZDir === 'reverse');
  // Aspect aggregate — uniform across cells → that value; mixed → null.
  // 'auto' (or empty) is the universal default. Drives the radio in
  // axes ▾ + the fit X/Y disable rule (equal/image lock single-axis fit).
  const axisModeAgg = (() => {
    if (axesArr.length === 0) return 'auto';
    const norm = (a) => (a && a.AxisMode ? a.AxisMode : 'auto');
    const v0 = norm(axesArr[0]);
    return axesArr.every((a) => norm(a) === v0) ? v0 : null;
  })();
  // Single-axis fit is meaningless under equal/image — both axes are
  // locked together by DataAspectRatio. Per toolbar=universal policy
  // we DON'T disable the rows. Instead the click handler upgrades
  // 'x'/'y'/'z' → 'both' when the cell's aspect locks the axes,
  // so the button still does something useful (refits both, panel
  // stays at script aspect). Implemented in applyFit + SubplotGrid's
  // fitSignal effect with per-cell axisMode awareness.
  // Legend / colorbar location aggregates — uniform across cells →
  // that value; mixed → null. null also means "follow script".
  const legendLocationAgg = (() => {
    if (axesArr.length === 0) return null;
    const v0 = axesArr[0].Legend && axesArr[0].Legend.Location;
    return axesArr.every((a) => (a.Legend && a.Legend.Location) === v0) ? v0 : null;
  })();
  const colorbarLocationAgg = (() => {
    if (axesArr.length === 0) return null;
    const v0 = axesArr[0].Colorbar && axesArr[0].Colorbar.Location;
    return axesArr.every((a) => (a.Colorbar && a.Colorbar.Location) === v0) ? v0 : null;
  })();
  // Colormap aggregate — uniform across heatmap-bearing axes; mixed → null.
  const colormapOverride = (() => {
    if (axesArr.length === 0) return null;
    const v0 = axesArr[0].Colormap;
    return axesArr.every((a) => a.Colormap === v0) ? v0 : null;
  })();
  // Color autoscale (CLim) — figure-wide for the toolbar.
  const colorOverride = axesArr.length > 0 && axesArr[0].CLim
    ? { cmin: axesArr[0].CLim[0], cmax: axesArr[0].CLim[1] }
    : null;
  function setColorOverride(v) {
    const clim = v ? [v.cmin, v.cmax] : null;
    setAxesArr((prev) => prev.map((a) => ({ ...a, CLim: clim })));
  }

  // ── Setters (toolbar fan-all + per-cell) ─────────────────────────
  // Toolbar setters take the React updater shape (value | fn). When a
  // function we evaluate it against the current AGGREGATE so the
  // common pattern `(v) => !v` flips from the visible aggregate state.
  function fanAll(key, updater) {
    setAxesArr((prev) => {
      const cur = prev.length > 0 ? !!prev.every((a) => !!legacyRead(a, key)) : false;
      const next = typeof updater === 'function' ? updater(cur) : updater;
      return prev.map((a) => legacyWrite(a, key, next));
    });
  }
  const setShowMajor    = (u) => fanAll('showMajor',    u);
  const setShowMinor    = (u) => fanAll('showMinor',    u);
  const setXLog         = (u) => fanAll('xLog',         u);
  const setYLog         = (u) => fanAll('yLog',         u);
  const setZLog         = (u) => fanAll('zLog',         u);
  const setShowTitle    = (u) => fanAll('showTitle',    u);
  const setShowXLabel   = (u) => fanAll('showXLabel',   u);
  const setShowYLabel   = (u) => fanAll('showYLabel',   u);
  const setShowZLabel   = (u) => fanAll('showZLabel',   u);
  const setShowLegend   = (u) => fanAll('showLegend',   u);
  const setShowColorbar = (u) => fanAll('showColorbar', u);
  const setShowAxis     = (u) => fanAll('showAxis',     u);
  const setShowBox      = (u) => fanAll('showBox',      u);
  const setXReverse     = (u) => fanAll('xReverse',     u);
  const setYReverse     = (u) => fanAll('yReverse',     u);
  const setZReverse     = (u) => fanAll('zReverse',     u);
  const setAxisMode     = (v) => fanAll('axisMode',     v);
  const setLegendLocation   = (v) => fanAll('legendLocation',   v);
  const setColorbarLocation = (v) => fanAll('colorbarLocation', v);
  function setColormapOverride(value) {
    setAxesArr((prev) => prev.map((a) => ({ ...a, Colormap: value })));
  }
  // Path-based setter: writes the same value to every Axes at the
  // given MATLAB property path. Used by the new per-axis display ▾
  // rows (X grid, Y grid, ...).
  function fanAllPath(path, updater) {
    setAxesArr((prev) => {
      const cur = prev.length > 0
                  ? prev.every((a) => isOn(getProp(a, path))) : false;
      const next = typeof updater === 'function' ? updater(cur) : updater;
      return prev.map((a) => setProp(a, path, onOff(!!next)));
    });
  }
  const setXGrid     = (u) => fanAllPath(['XGrid'], u);
  const setYGrid     = (u) => fanAllPath(['YGrid'], u);
  const setZGrid     = (u) => fanAllPath(['ZGrid'], u);
  const setRGrid     = (u) => fanAllPath(['RGrid'], u);
  const setThetaGrid = (u) => fanAllPath(['ThetaGrid'], u);
  // Per-axis MINOR setters — companions to the major setters above.
  // Drive the second button column in the new grid ▾ matrix layout.
  const setXMinor     = (u) => fanAllPath(['XMinorGrid'],     u);
  const setYMinor     = (u) => fanAllPath(['YMinorGrid'],     u);
  const setZMinor     = (u) => fanAllPath(['ZMinorGrid'],     u);
  const setRMinor     = (u) => fanAllPath(['RMinorGrid'],     u);
  const setThetaMinor = (u) => fanAllPath(['ThetaMinorGrid'], u);

  // Per-cell setters — write to one Axes by index.
  function setCellKey(idx, key, updater) {
    setAxesArr((prev) => {
      const a = prev[idx] || initAxesFromCell(cellsArr[idx]);
      const cur = legacyRead(a, key);
      const value = typeof updater === 'function' ? updater(cur) : updater;
      const out = prev.slice();
      out[idx] = legacyWrite(a, key, value);
      return out;
    });
  }
  const makeCellDisplaySetter  = (idx, key) => (u) => setCellKey(idx, key, u);
  const makeCellColormapSetter = (idx) => (v) => setCellKey(idx, 'colormap', v);
  const makeCellDisplayReset = (idx) => () => {
    setAxesArr((prev) => {
      const init = initAxesFromCell(cellsArr[idx] || {});
      const cur  = prev[idx] || init;
      // Reset display flags but keep Colormap, CLim, XLim/YLim/ZLim, View.
      const next = prev.slice();
      next[idx] = {
        ...cur,
        Visible: init.Visible, Box: init.Box,
        XGrid: init.XGrid, YGrid: init.YGrid, ZGrid: init.ZGrid,
        XMinorGrid: init.XMinorGrid, YMinorGrid: init.YMinorGrid, ZMinorGrid: init.ZMinorGrid,
        RGrid: init.RGrid, ThetaGrid: init.ThetaGrid,
        RMinorGrid: init.RMinorGrid, ThetaMinorGrid: init.ThetaMinorGrid,
        XScale: init.XScale, YScale: init.YScale, ZScale: init.ZScale,
        XDir: init.XDir, YDir: init.YDir, ZDir: init.ZDir,
        // Aspect — UI-set axisMode override gets cleared back to the
        // script value (`axis equal/square/image/tight/auto`). Three
        // schema fields kept in sync: AxisMode (shorthand) and the
        // two MATLAB-style mode flags.
        AxisMode:               init.AxisMode,
        DataAspectRatioMode:    init.DataAspectRatioMode,
        PlotBoxAspectRatioMode: init.PlotBoxAspectRatioMode,
        Title: init.Title, Subtitle: init.Subtitle,
        XLabel: init.XLabel, YLabel: init.YLabel, ZLabel: init.ZLabel, YLabel2: init.YLabel2,
        Legend: init.Legend, Colorbar: init.Colorbar,
      };
      return next;
    });
  };
  const makeCellColormapReset = (idx) => () => {
    setAxesArr((prev) => {
      const out = prev.slice();
      out[idx] = { ...(out[idx] || initAxesFromCell(cellsArr[idx] || {})), Colormap: null };
      return out;
    });
  };

  // Compute new color override from a coarse-LOD scan of the visible
  // source-rect. Mirrors Heatmap.fitColorsToVisible. Lives here so the
  // toolbar fit menu can invoke it without lifting / via callback ref.
  function fitColorsToVisible() {
    if (!engine || typeof engine.getFigureTile !== 'function') return;
    if (!heatmapLayer) return;
    const figId = heatmapLayer._figId;
    if (typeof figId !== 'number' || figId < 0) return;
    const fullCols = heatmapLayer.originalCols;
    const fullRows = heatmapLayer.originalRows;
    if (!fullCols || !fullRows) return;
    const xExt = figure.xRange[1] - figure.xRange[0];
    const yExt = figure.yRange[1] - figure.yRange[0];
    const colsPerUnit = fullCols / (xExt || 1);
    const rowsPerUnit = fullRows / (yExt || 1);
    const xMin = viewport.x[0], xMax = viewport.x[1];
    const yMin = viewport.y[0], yMax = viewport.y[1];
    const c0 = Math.max(0, Math.floor((Math.min(xMin, xMax) - figure.xRange[0]) * colsPerUnit));
    const c1 = Math.min(fullCols, Math.ceil((Math.max(xMin, xMax) - figure.xRange[0]) * colsPerUnit));
    const r0 = Math.max(0, Math.floor((Math.min(yMin, yMax) - figure.yRange[0]) * rowsPerUnit));
    const r1 = Math.min(fullRows, Math.ceil((Math.max(yMin, yMax) - figure.yRange[0]) * rowsPerUnit));
    const tileW = c1 - c0, tileH = r1 - r0;
    if (tileW <= 0 || tileH <= 0) return;
    const lod = Math.max(1, Math.ceil(Math.max(tileH, tileW) / 256));
    const tile = engine.getFigureTile(figId, heatmapLayer._axIdx, heatmapLayer._dsIdx,
                                      r0, c0, tileH, tileW, lod);
    if (!tile || tile.error || !tile.data) return;
    let idxMn = 256, idxMx = -1;
    for (let i = 0; i < tile.data.length; i++) {
      const idx = tile.data[i];
      if (idx === 255) continue;
      if (idx < idxMn) idxMn = idx;
      if (idx > idxMx) idxMx = idx;
    }
    if (idxMn > 254 || idxMx < 0 || idxMn === idxMx) return;
    const cminOrig = heatmapLayer.cminOrig ?? heatmapLayer.cmin;
    const cmaxOrig = heatmapLayer.cmaxOrig ?? heatmapLayer.cmax;
    const range = cmaxOrig - cminOrig;
    setColorOverride({
      cmin: cminOrig + (idxMn / 254) * range,
      cmax: cminOrig + (idxMx / 254) * range,
    });
  }

  // Toggle that also auto-clamps the viewport's lo bound to a positive
  // value when entering log mode. Two paths:
  //   - heatmap: clamp to half a cell width (smallest meaningful unit
  //     that still lands inside the data grid).
  //   - line/scatter/etc: clamp to xRange[1]/1e4 (gives ~4 decades of
  //     visible range — a sane default for plot(1:1000) where the
  //     adapter's 4% padding pushed viewport.x[0] negative).
  // Without the clamp, xLogActive in CompositePlot stays false (its
  // guard is xMin > 0 && xMax > 0) and the toggle has no visual effect.
  function toggleAxisLog(axis) {
    if (axis === 'x') {
      const next = !xLog;
      // Top-level viewport is null for subplot figures (per-cell
      // viewports live in SubplotGrid). Skip the clamp when viewport
      // isn't an object — CompositePlot inside each cell now has a
      // useEffect on xLog/yLog that does the per-cell clamp.
      if (next && viewport && Array.isArray(viewport.x)
          && (viewport.x[0] <= 0 || viewport.x[1] <= 0)) {
        let lo;
        if (isHeatmap) {
          const fullCols = heatmapLayer?.originalCols || 1;
          const cellW = (figure.xRange[1] - figure.xRange[0]) / fullCols;
          lo = Math.max(cellW * 0.5, 1e-6);
        } else {
          // Use the highest-positive data extent we know about as the
          // anchor — fall back to figure.xRange or viewport upper.
          const hi = Math.max(figure.xRange?.[1] || viewport.x[1], 1e-6);
          lo = Math.max(hi / 1e4, 1e-6);
        }
        const hiClamped = Math.max(lo * 10, figure.xRange?.[1] || viewport.x[1]);
        setViewport({ ...viewport, x: [lo, hiClamped] });
      }
      setXLog(next);
    } else {
      const next = !yLog;
      if (next && viewport && Array.isArray(viewport.y)
          && (viewport.y[0] <= 0 || viewport.y[1] <= 0)) {
        let lo;
        if (isHeatmap) {
          const fullRows = heatmapLayer?.originalRows || 1;
          const cellH = (figure.yRange[1] - figure.yRange[0]) / fullRows;
          lo = Math.max(cellH * 0.5, 1e-6);
        } else {
          const hi = Math.max(figure.yRange?.[1] || viewport.y[1], 1e-6);
          lo = Math.max(hi / 1e4, 1e-6);
        }
        const hiClamped = Math.max(lo * 10, figure.yRange?.[1] || viewport.y[1]);
        setViewport({ ...viewport, y: [lo, hiClamped] });
      }
      setYLog(next);
    }
  }
  const [fitOpen, setFitOpen]     = useState(false);
  // fitSignal fires every toolbar Fit click. SubplotGrid watches it
  // (counter-based change detection) and resets every cell's viewport
  // along the requested axis. Non-subplot figures use the legacy
  // applyFit path directly; this signal is only consumed by SubplotGrid.
  const [fitSignal, setFitSignal] = useState({ axis: '', n: 0 });
  function fitAllCells(axis) {
    setFitSignal((prev) => ({ axis, n: prev.n + 1 }));
  }
  // ── Reset helpers ────────────────────────────────────────────────
  // Three flavours, each with its own scope:
  //   • fit ▾ → reset      — viewport only (zoom/pan defaults)
  //   • display ▾ → reset  — display state + colormap, ALL cells back
  //                          to script defaults
  //   • 🏠 Reset toolbar   — viewport + display
  // Display reset re-inits cells from script defaults — covers every
  // flag, the colormap, and (now) the viewport in one operation. No
  // cascade effects, no signal counters.
  function displayReset() {
    setAxesArr(cellsArr.map(initAxesFromCell));
  }
  function viewportReset() {
    if (isSubplot) {
      fitAllCells('both');
    } else if (is3D) {
      if (bbox3d) {
        setViewport({
          x: [bbox3d.xMin, bbox3d.xMax],
          y: [bbox3d.yMin, bbox3d.yMax],
          z: [bbox3d.zMin, bbox3d.zMax],
        });
      }
    } else if (isHeatmap) {
      setViewport(figDefault);
      setColorOverride(null);
    } else {
      setViewport(figDefault);
    }
  }
  function resetAll() {
    viewportReset();
    displayReset();
  }
  const [axesOpen, setAxesOpen] = useState(false);
  const [gridOpen, setGridOpen] = useState(false);
  const [displayOpen, setDisplayOpen] = useState(false);
  const [cmapOpen, setCmapOpen]   = useState(false);
  const [saveOpen, setSaveOpen]   = useState(false);
  const [viewOpen, setViewOpen]   = useState(false);
  const [maximized, setMaximized] = useState(false);
  const fitRef  = useRef(null);
  const axesRef = useRef(null);
  const gridRef = useRef(null);
  const displayRef = useRef(null);
  const cmapRef = useRef(null);
  const saveRef = useRef(null);
  const viewRef = useRef(null);

  // ── display-menu disabled rules ──────────────────────────────────────
  // For non-subplot figures we look at top-level fields. For subplots,
  // we scan cells — a toggle is enabled if AT LEAST one cell satisfies
  // the precondition (the toggle is global; per-cell handlers ignore
  // cells that can't apply).
  const cellsList = isSubplot && Array.isArray(figure.cells) ? figure.cells : [figure];
  const anyCellHas = (pred) => cellsList.some(pred);
  // Find the first heatmap layer ANYWHERE in the figure (top-level or
  // inside a subplot cell). Drives the toolbar Colormap ▾ visibility +
  // its "script default" anchor for marking the active palette.
  const findFirstHeatmap = () => {
    for (const c of cellsList) {
      const layers = (c && Array.isArray(c.layers)) ? c.layers : [];
      const hm = layers.find((l) => l && l.kind === 'heatmap');
      if (hm) return hm;
    }
    return null;
  };
  const anyHeatmap = findFirstHeatmap();
  // has3DCell still used by ПКМ Display submenu Z-row gating in
  // CompositePlot (passed implicitly via the figure prop). Toolbar
  // toggles themselves are never disabled — that was deliberately
  // dropped because the aggregate disabled-rule lied about per-cell
  // state for fresh subplots.
  const has3DCell = isSubplot
    ? cellsList.some((c) => c.kind === 'composite3d')
    : is3D;
  const wrapRef = useRef(null);
  const [size, setSize] = useState({ w: 1100, h: 600 });

  useEffect(() => {
    function onKey(e) {
      if (e.key === 'Escape') onClose();
      if (e.key === '0' && figDefault) setViewport(figDefault);
    }
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [onClose, figure]);

  useEffect(() => {
    function onDoc(e) {
      if (fitRef.current     && !fitRef.current.contains(e.target))     setFitOpen(false);
      if (axesRef.current    && !axesRef.current.contains(e.target))    setAxesOpen(false);
      if (gridRef.current    && !gridRef.current.contains(e.target))    setGridOpen(false);
      if (displayRef.current && !displayRef.current.contains(e.target)) setDisplayOpen(false);
      if (cmapRef.current    && !cmapRef.current.contains(e.target))    setCmapOpen(false);
      if (saveRef.current    && !saveRef.current.contains(e.target))    setSaveOpen(false);
      if (viewRef.current    && !viewRef.current.contains(e.target))    setViewOpen(false);
    }
    document.addEventListener('mousedown', onDoc);
    return () => document.removeEventListener('mousedown', onDoc);
  }, []);

  // Synchronous measure before paint so the SVG is sized correctly on the
  // very first frame the modal opens. `[]` deps so this only runs once at
  // mount — without it React would rerun the effect after every state
  // change, feeding setSize back into another render and tripping the
  // "Maximum update depth exceeded" guard.
  useLayoutEffect(() => {
    const el = wrapRef.current;
    if (!el) return;
    const r = el.getBoundingClientRect();
    if (!r.width) return;
    setSize((prev) => {
      const w = Math.max(400, Math.round(r.width  - 32));
      const h = Math.max(300, Math.round(r.height - 32));
      return (Math.abs(prev.w - w) > 0.5 || Math.abs(prev.h - h) > 0.5) ? { w, h } : prev;
    });
  }, []);

  // Re-measure on resize signals: window resize (modal is 85vw / 80vh) plus
  // ResizeObserver in modern browsers.
  useEffect(() => {
    const el = wrapRef.current;
    if (!el) return;
    const remeasure = () => {
      const r = el.getBoundingClientRect();
      if (!r.width) return;
      setSize((prev) => {
        const w = Math.max(400, Math.round(r.width  - 32));
        const h = Math.max(300, Math.round(r.height - 32));
        return (Math.abs(prev.w - w) > 0.5 || Math.abs(prev.h - h) > 0.5) ? { w, h } : prev;
      });
    };
    let ro = null;
    if (typeof ResizeObserver !== 'undefined') {
      ro = new ResizeObserver(remeasure);
      ro.observe(el);
    }
    window.addEventListener('resize', remeasure);
    return () => {
      ro?.disconnect();
      window.removeEventListener('resize', remeasure);
    };
  }, []);

  // Local downloadBlob for CSV/TSV/JSON paths below — image exports go through
  // plotUtils helpers so they share the light-theme + variable-resolution
  // logic with the per-plot ПКМ menus.
  const downloadBlob = utilDownloadBlob;

  /**
   * For subplot figures, gather every cell <svg> + its position relative to
   * the canvas wrap, and compose them into a single SVG string. Otherwise
   * just serialise the one SVG inside the wrap. Returns { xml, w, h } or
   * null if nothing's there yet.
   */
  function gatherFigureSvg() {
    const wrap = wrapRef.current;
    if (!wrap) return null;
    if (figure.kind === 'subplot') {
      const svgs = wrap.querySelectorAll('svg');
      if (svgs.length === 0) return null;
      const wrapRect = wrap.getBoundingClientRect();
      const layouts = Array.from(svgs).map((s) => {
        const r = s.getBoundingClientRect();
        return {
          x: r.left - wrapRect.left, y: r.top - wrapRect.top,
          w: r.width, h: r.height,
        };
      });
      return {
        xml: composeSvgsToString(svgs, layouts, wrapRect.width, wrapRect.height),
        w: wrapRect.width, h: wrapRect.height,
      };
    }
    const svg = wrap.querySelector('svg');
    if (!svg) return null;
    return {
      xml: new XMLSerializer().serializeToString(svg),
      w: size.w, h: size.h,
    };
  }
  function exportSvg() {
    if (is3D) return;   // SVG export not available for WebGL geometry
    const g = gatherFigureSvg();
    if (!g) return;
    exportSvgString(g.xml, `figure_${figure.id}.svg`);
  }
  function dataUrlToBlob(dataUrl) {
    // data:image/png;base64,...
    const idx = dataUrl.indexOf(',');
    const meta = dataUrl.substring(5, idx);                    // image/png;base64
    const mime = meta.split(';')[0];
    const b64  = dataUrl.substring(idx + 1);
    const bin  = atob(b64);
    const arr  = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) arr[i] = bin.charCodeAt(i);
    return new Blob([arr], { type: mime });
  }
  function exportPng(scale = 2, suffix = '') {
    if (is3D) {
      const url = threeRef.current?.getCanvasDataURL?.(scale);
      if (!url) return;
      utilDownloadBlob(dataUrlToBlob(url), `figure_${figure.id}${suffix}.png`);
      return;
    }
    const g = gatherFigureSvg();
    if (!g) return;
    exportPngString(g.xml, g.w, g.h, scale, `figure_${figure.id}${suffix}.png`);
  }
  function exportPngPrint(mmWidth, dpi = 300) {
    const targetPx = (mmWidth / 25.4) * dpi;
    if (is3D) {
      // 3-D path now resizes the renderer's drawing buffer to the
      // target pixel count, renders once at high res, snapshots, and
      // restores. The on-screen canvas CSS size is untouched.
      const canvas = threeRef.current?.getCanvas?.();
      // We don't expose a getCanvas helper today — derive scale from
      // the live CSS width via the imperative handle's own caller-
      // visible size hint. Fall back to scale=2 if unknown.
      const ctxW = (threeRef.current?.getCanvasCssSize?.()?.width)
                    || (typeof window !== 'undefined' && window.innerWidth)
                    || 800;
      const scale = Math.max(1, targetPx / ctxW);
      const url = threeRef.current?.getCanvasDataURL?.(scale);
      if (!url) return;
      utilDownloadBlob(dataUrlToBlob(url), `figure_${figure.id}_${mmWidth}mm.png`);
      return;
    }
    const g = gatherFigureSvg();
    if (!g) return;
    const scale = targetPx / g.w;
    exportPngString(g.xml, g.w, g.h, scale, `figure_${figure.id}_${mmWidth}mm.png`);
  }
  // Build a CSV/TSV "name<sep>x<sep>y[<sep>z]" body from a series
  // source. Accepts either a polar figure (`series` with theta/rho),
  // an array of 2-D series layers (`x`, `y`), or 3-D series with z[].
  function seriesBody(source, sep) {
    const list = Array.isArray(source) ? source : (source.series || []);
    const has3D = list.some((s) => Array.isArray(s.z));
    const rows = [`name${sep}x${sep}y${has3D ? sep + 'z' : ''}`];
    list.forEach((s) => {
      const xs = s.x || s.theta || [];
      const ys = s.y || s.rho   || [];
      const zs = Array.isArray(s.z) ? s.z : null;
      for (let i = 0; i < xs.length; i++) {
        let row = `${s.name}${sep}${xs[i]}`;
        if (ys[i] != null) row += sep + ys[i];
        if (zs && zs[i] != null) row += sep + zs[i];
        rows.push(row);
      }
    });
    return rows.join('\n');
  }
  function get3DRows() {
    return threeRef.current?.getCsvData?.() || [];
  }
  // Composite cell exporter — pulls heatmap layer's z if present, else series.
  function compositeCellBody(cell, sep) {
    const layers = cell.layers || [];
    const hl = layers.find((l) => l.kind === 'heatmap');
    if (hl) return hl.z.map((row) => row.map((v) => v == null ? '' : v).join(sep)).join('\n');
    return seriesBody(layers.filter((l) => l.kind === 'series'), sep);
  }
  function exportCsv() {
    if (is3D) {
      downloadBlob(new Blob([seriesBody(get3DRows(), ',')], { type: 'text/csv' }),
                   `figure_${figure.id}.csv`);
      return;
    }
    if (isHeatmap) {
      const z = heatmapLayer.z;
      const rows = z.map((row) => row.map((v) => v == null ? '' : v).join(','));
      downloadBlob(new Blob([rows.join('\n')], { type: 'text/csv' }), `figure_${figure.id}.csv`);
      return;
    }
    if (isSubplot) {
      const parts = figure.cells.map((c, i) => {
        const tag = `# subplot ${c.subplotIndex || i + 1} — ${c.title || c.kind}`;
        if (c.kind === 'composite') return `${tag}\n` + compositeCellBody(c, ',');
        return `${tag}\n` + seriesBody(c, ',');
      });
      downloadBlob(new Blob([parts.join('\n\n')], { type: 'text/csv' }), `figure_${figure.id}.csv`);
      return;
    }
    if (isComposite) {
      downloadBlob(new Blob([seriesBody(seriesLayers, ',')], { type: 'text/csv' }), `figure_${figure.id}.csv`);
      return;
    }
    downloadBlob(new Blob([seriesBody(figure, ',')], { type: 'text/csv' }), `figure_${figure.id}.csv`);
  }
  function exportTsv() {
    if (is3D) {
      downloadBlob(new Blob([seriesBody(get3DRows(), '\t')], { type: 'text/tab-separated-values' }),
                   `figure_${figure.id}.tsv`);
      return;
    }
    if (isHeatmap) {
      const z = heatmapLayer.z;
      const rows = z.map((row) => row.map((v) => v == null ? '' : v).join('\t'));
      downloadBlob(new Blob([rows.join('\n')], { type: 'text/tab-separated-values' }), `figure_${figure.id}.tsv`);
      return;
    }
    if (isSubplot) {
      const parts = figure.cells.map((c, i) => {
        const tag = `# subplot ${c.subplotIndex || i + 1} — ${c.title || c.kind}`;
        if (c.kind === 'composite') return `${tag}\n` + compositeCellBody(c, '\t');
        return `${tag}\n` + seriesBody(c, '\t');
      });
      downloadBlob(new Blob([parts.join('\n\n')], { type: 'text/tab-separated-values' }), `figure_${figure.id}.tsv`);
      return;
    }
    if (isComposite) {
      downloadBlob(new Blob([seriesBody(seriesLayers, '\t')], { type: 'text/tab-separated-values' }), `figure_${figure.id}.tsv`);
      return;
    }
    downloadBlob(new Blob([seriesBody(figure, '\t')], { type: 'text/tab-separated-values' }), `figure_${figure.id}.tsv`);
  }
  function exportJson() {
    if (is3D) {
      const obj = {
        id: figure.id, kind: 'composite3d',
        title: figure.title,
        xLabel: figure.xLabel, yLabel: figure.yLabel, zLabel: figure.zLabel,
        view: figure.view,
        series: get3DRows(),
      };
      downloadBlob(new Blob([JSON.stringify(obj, null, 2)], { type: 'application/json' }),
                   `figure_${figure.id}.json`);
      return;
    }
    if (isHeatmap) {
      const obj = {
        id: figure.id, kind: 'heatmap', title: figure.title,
        xRange: figure.xRange, yRange: figure.yRange,
        cmin: heatmapLayer.cmin, cmax: heatmapLayer.cmax,
        colormap: heatmapLayer.colormap, z: heatmapLayer.z,
      };
      downloadBlob(new Blob([JSON.stringify(obj, null, 2)], { type: 'application/json' }), `figure_${figure.id}.json`);
      return;
    }
    if (isSubplot) {
      const obj = {
        id: figure.id, kind: 'subplot', title: figure.title, grid: figure.grid,
        cells: figure.cells.map((c) => {
          if (c.kind === 'composite') {
            const layers = c.layers || [];
            return {
              subplotIndex: c.subplotIndex, kind: 'composite', title: c.title,
              xLabel: c.xLabel, yLabel: c.yLabel,
              xRange: c.xRange, yRange: c.yRange,
              layers: layers.map((ly) => {
                if (ly.kind === 'heatmap') return { kind: 'heatmap', z: ly.z, cmin: ly.cmin, cmax: ly.cmax };
                if (ly.kind === 'series')  return { kind: 'series', mode: ly.mode, name: ly.name, color: ly.color, x: ly.x, y: ly.y };
                return { ...ly };
              }),
            };
          }
          return { subplotIndex: c.subplotIndex, kind: c.kind, title: c.title,
            xLabel: c.xLabel, yLabel: c.yLabel,
            series: (c.series || []).map((s) => ({
              name: s.name, color: s.color, x: s.x ?? s.theta, y: s.y ?? s.rho,
            })),
          };
        }),
      };
      downloadBlob(new Blob([JSON.stringify(obj, null, 2)], { type: 'application/json' }), `figure_${figure.id}.json`);
      return;
    }
    if (isComposite) {
      const obj = {
        id: figure.id, kind: 'composite', title: figure.title,
        xLabel: figure.xLabel, yLabel: figure.yLabel,
        xRange: figure.xRange, yRange: figure.yRange,
        layers: compositeLayers.map((ly) => {
          if (ly.kind === 'heatmap') return { kind: 'heatmap', z: ly.z, cmin: ly.cmin, cmax: ly.cmax, colormap: ly.colormap };
          if (ly.kind === 'series')  return { kind: 'series', mode: ly.mode, name: ly.name, color: ly.color, x: ly.x, y: ly.y };
          return { ...ly };
        }),
      };
      downloadBlob(new Blob([JSON.stringify(obj, null, 2)], { type: 'application/json' }), `figure_${figure.id}.json`);
      return;
    }
    const obj = {
      id: figure.id, kind: figure.kind, title: figure.title,
      xLabel: figure.xLabel, yLabel: figure.yLabel,
      series: (figure.series || []).map((s) => ({
        name: s.name, color: s.color,
        x: s.x ?? s.theta, y: s.y ?? s.rho,
      })),
    };
    downloadBlob(new Blob([JSON.stringify(obj, null, 2)], { type: 'application/json' }), `figure_${figure.id}.json`);
  }

  function applyFit(mode, axisMode) {
    if (isSubplot) return;                 // subplot fit lives per-cell; close menu
    // Axis-equal upgrade lives inside fitCellViewport now — see the
    // shared cartesian dispatch at the bottom of this function.
    // 3D + polar branches have their own data-extent computation
    // (bbox / rho-max) so they keep their own dispatch.
    if (is3D) {
      // 3-D fit pulls the data bbox from Composite3DPlot's imperative
      // handle (Composite3DPlot reports it via onBBox each rebuild;
      // bbox3d is the cached copy). Each axis is reset independently
      // so "X only" leaves Y/Z lims untouched.
      const b = bbox3d || (threeRef.current?.getBBox?.() ?? null);
      if (!b) { setFitOpen(false); return; }
      const next = {
        x: viewport?.x?.slice() || [-1, 1],
        y: viewport?.y?.slice() || [-1, 1],
        z: viewport?.z?.slice() || [-1, 1],
      };
      if (axisMode === 'both' || axisMode === 'x') next.x = [b.xMin, b.xMax];
      if (axisMode === 'both' || axisMode === 'y') next.y = [b.yMin, b.yMax];
      if (axisMode === 'both' || axisMode === 'z') next.z = [b.zMin, b.zMax];
      setViewport(next);
      setFitOpen(false);
      return;
    }
    if (isPolar) {
      // Polar fit: two axes — R (radial extent) and θ (angular sweep).
      // axisMode picks which to reset:
      //   'r'              → R only, θ stays at current viewport
      //   'theta'          → θ only, R stays
      //   'rtheta'/'both'  → both
      // Anything else (e.g. legacy 'x'/'y'/'z') falls through to fit-
      // both for back-compat — the only direct call sites this matters
      // for are the cartesian section, which only fires on
      // !isPolar paths above.
      const list = mode === 'all'
        ? figure.series
        : figure.series.filter((s) => s.name === mode);
      const wantR = axisMode === 'r'      || axisMode === 'both' || axisMode === 'rtheta' || !axisMode;
      const wantT = axisMode === 'theta'  || axisMode === 'both' || axisMode === 'rtheta';
      const cur = viewport || {};
      const next = {
        r:     Array.isArray(cur.r)     ? cur.r.slice()     : [0, 1],
        theta: Array.isArray(cur.theta) ? cur.theta.slice() : [0, 360],
      };
      if (wantR) {
        let m = 0;
        list.forEach((s) => s.rho?.forEach((v) => {
          if (Number.isFinite(v) && Math.abs(v) > m) m = Math.abs(v);
        }));
        const lo = (Array.isArray(figure.rlim) && figure.rlim.length === 2)
                   ? figure.rlim[0] : 0;
        next.r = [lo, nicePolarMax(m || 1)];
      }
      if (wantT) {
        next.theta = (Array.isArray(figure.thetalim) && figure.thetalim.length === 2)
                     ? figure.thetalim.slice() : [0, 360];
      }
      setViewport(next);
      setFitOpen(false);
      return;
    }
    // All cartesian (non-3D, non-polar) paths route through the
    // shared fitCellViewport. Toolbar=universal: this is the SAME
    // per-cell action SubplotGrid + CompositePlot ПКМ use, so the
    // outcome doesn't drift between menus.
    //
    // Log axes get a half-cell lo-bound override (figure.xRange
    // straddles zero for heatmap padding and would silently flip
    // back to linear). Applied after the base fit so axis-equal
    // upgrade is honoured first.
    let next = fitCellViewport(figure, viewport, axisMode, { aspectMode: axisModeAgg });
    const xLogNow = figure.xscale === 'log';
    const yLogNow = figure.yscale === 'log';
    if ((axisMode === 'both' || axisMode === 'x') && xLogNow) {
      const fullCols = (isHeatmap ? heatmapLayer.originalCols : 0) || 1;
      const cellW = (figure.xRange[1] - figure.xRange[0]) / fullCols;
      const lo = Math.max(cellW * 0.5, 1e-6);
      next = { ...next, x: [lo, Math.max(lo * 10, figure.xRange[1])] };
    }
    if ((axisMode === 'both' || axisMode === 'y') && yLogNow) {
      const fullRows = (isHeatmap ? heatmapLayer.originalRows : 0) || 1;
      const cellH = (figure.yRange[1] - figure.yRange[0]) / fullRows;
      const lo = Math.max(cellH * 0.5, 1e-6);
      next = { ...next, y: [lo, Math.max(lo * 10, figure.yRange[1])] };
    }
    setViewport(next);
    setFitOpen(false);
  }

  const fmtVp = (n) => Number.isFinite(n) ? Number(n.toPrecision(5)).toString() : '—';

  return (
    <div className="fw-overlay" onClick={(e) => { if (e.target === e.currentTarget) onClose(); }}>
      <div className={`fw-window ${maximized ? 'is-max' : ''}`}
        role="dialog" aria-label={`Figure ${figure.id}`}>
        <div className="fw-titlebar">
          <div className="fw-title-left">
            <span className="ve-tag" style={{
              color: 'var(--accent)',
              background: 'rgba(127,217,154,0.10)',
              borderColor: 'rgba(127,217,154,0.30)',
            }}>▦ figure</span>
            <span className="fw-name">Figure {figure.id}</span>
            <span className="ve-dim">{figure.title}</span>
            <span className="fw-meta">
              {isSubplot
                ? `subplot ${figure.grid[0]}×${figure.grid[1]} · ${figure.cells.length} axes`
                : isPolar
                  ? `${figure.series?.length ?? 0} series · ${(figure.series || []).reduce((s, x) => s + (x.theta?.length ?? 0), 0)} points`
                  : isHeatmap
                    ? `${heatmapLayer.z?.length ?? 0} × ${heatmapLayer.z?.[0]?.length ?? 0} cells · range [${Number(heatmapLayer.cmin).toPrecision(3)} … ${Number(heatmapLayer.cmax).toPrecision(3)}]${hasSeries ? ` · ${seriesLayers.length} overlay${seriesLayers.length === 1 ? '' : 's'}` : ''}`
                    : `${seriesLayers.length} series · ${seriesLayers.reduce((s, x) => s + (x.x?.length ?? 0), 0)} points`}
            </span>
          </div>
          <div className="fw-title-right">
            <button className="ve-close" onClick={() => setMaximized((m) => !m)}
              title={maximized ? 'Restore' : 'Maximise'} aria-label="Maximise">
              {maximized ? (
                <svg width="13" height="13" viewBox="0 0 12 12" fill="none">
                  <rect x="1.5" y="3.5" width="7" height="7"
                    stroke="currentColor" strokeWidth="1.2" fill="var(--bg-2)"/>
                  <rect x="3.5" y="1.5" width="7" height="7"
                    stroke="currentColor" strokeWidth="1.2" fill="var(--bg-2)"/>
                </svg>
              ) : (
                <svg width="13" height="13" viewBox="0 0 12 12" fill="none">
                  <rect x="1.5" y="1.5" width="9" height="9" stroke="currentColor" strokeWidth="1.2"/>
                </svg>
              )}
            </button>
            <button className="ve-close" onClick={onClose} aria-label="Close">×</button>
          </div>
        </div>

        <div className="fw-toolbar">
          {/* 🏠 Reset — full reset of viewport AND display state. For
              subplot it fans out to every cell. Standalone toolbar
              button (not a popover) for one-click access. */}
          <button className="ve-btn"
                  onClick={resetAll}
                  title="Reset viewport + display state"
                  data-fw-reset="all">
            <svg width="11" height="11" viewBox="0 0 12 12">
              <path d="M1 6l5-5 5 5 M2 5v6h8V5"
                    stroke="currentColor" strokeWidth="1.2" fill="none" strokeLinejoin="round"/>
            </svg>
            reset
          </button>
          {/* fit ▾ — always shown, applies to EVERY cell in subplot mode.
              Per-series rows live in the right-click menu only; the
              toolbar version is global by design. */}
          <div className="ve-tools-group" ref={fitRef}>
            <button className="ve-btn" onClick={() => setFitOpen((o) => !o)} title="Fit viewport">
              <svg width="11" height="11" viewBox="0 0 12 12">
                <path d="M2 2L10 10 M2 6V2H6 M10 6v4H6" stroke="currentColor" strokeWidth="1.2" fill="none" strokeLinecap="round"/>
              </svg>
              fit ▾
            </button>
            {fitOpen && (
              <div className="fw-pop">
                <div className="fw-pop-section">
                  <button onClick={() => { viewportReset(); setFitOpen(false); }}>default</button>
                </div>
                <div className="fw-pop-section">
                  {/* `all` sits on its own — it's the "fit everything"
                      master that resets each cell to its default
                      viewport regardless of coordinate system. Below
                      it the per-axis rows split into Cartesian and
                      Polar so the labels read unambiguously. */}
                  <button onClick={() => {
                    if (isSubplot) fitAllCells('both'); else applyFit('all', 'both');
                    setFitOpen(false);
                  }}>all</button>
                </div>
                <div className="fw-pop-section">
                  <div className="fw-pop-head">Cartesian</div>
                  {/* Toolbar=universal: per-axis rows are ALWAYS
                      clickable. For cells with aspect=equal/image
                      the click handler upgrades single-axis fit to
                      fit-both (handled per-cell in SubplotGrid /
                      figure-wide in applyFit). Plain auto/tight
                      cells get the literal single-axis fit. */}
                  <button onClick={() => {
                    if (isSubplot) fitAllCells('x'); else applyFit('all', 'x');
                    setFitOpen(false);
                  }}>X</button>
                  <button onClick={() => {
                    if (isSubplot) fitAllCells('y'); else applyFit('all', 'y');
                    setFitOpen(false);
                  }}>Y</button>
                  <button onClick={() => {
                    if (isSubplot) fitAllCells('z'); else applyFit('all', 'z');
                    setFitOpen(false);
                  }}>Z</button>
                </div>
                <div className="fw-pop-section">
                  <div className="fw-pop-head">Polar</div>
                  {/* Polar fits use a separate viewport shape
                      ({r, theta}); cartesian cells ignore the polar
                      signal — same universal-brush policy. */}
                  <button onClick={() => {
                    if (isSubplot) fitAllCells('r'); else applyFit('all', 'r');
                    setFitOpen(false);
                  }}>R</button>
                  <button onClick={() => {
                    if (isSubplot) fitAllCells('theta'); else applyFit('all', 'theta');
                    setFitOpen(false);
                  }}>θ</button>
                </div>
                {/* Heatmap colour-fit / reset-colors previously sat
                    here gated on `isHeatmap`. Dropped from the toolbar
                    so fit ▾ stays identical across figure kinds —
                    those operations now live in the ПКМ "Color range"
                    section (specialised, heatmap-only). */}
              </div>
            )}
          </div>

          {/* View presets — 3-D only. Six standard MATLAB camera
              orientations: top (XY plane down), bottom (XY plane up),
              front (XZ), back, left (YZ), right, plus iso (default
              -37.5° / 30°). Each calls the Composite3DPlot.setView
              imperative method without re-emitting the figure JSON,
              so script-set view(az, el) is preserved as the "iso"
              fallback. */}
          {is3D && !isSubplot && (
            <div className="ve-tools-group" ref={viewRef}>
              <button className="ve-btn"
                      onClick={() => setViewOpen((o) => !o)}
                      title="Camera presets">
                view ▾
              </button>
              {viewOpen && (
                <div className="fw-pop">
                  <div className="fw-pop-section">
                    <div className="fw-pop-head">presets</div>
                    <button data-fw-view="iso" onClick={() => {
                      threeRef.current?.setView?.(-37.5, 30);
                      setViewOpen(false);
                    }}>iso (default)</button>
                    <button data-fw-view="top" onClick={() => {
                      threeRef.current?.setView?.(0, 90);
                      setViewOpen(false);
                    }}>top (XY)</button>
                    <button data-fw-view="bottom" onClick={() => {
                      threeRef.current?.setView?.(0, -90);
                      setViewOpen(false);
                    }}>bottom (XY-)</button>
                    <button data-fw-view="front" onClick={() => {
                      threeRef.current?.setView?.(0, 0);
                      setViewOpen(false);
                    }}>front (XZ)</button>
                    <button data-fw-view="back" onClick={() => {
                      threeRef.current?.setView?.(180, 0);
                      setViewOpen(false);
                    }}>back (XZ-)</button>
                    <button data-fw-view="right" onClick={() => {
                      threeRef.current?.setView?.(90, 0);
                      setViewOpen(false);
                    }}>right (YZ)</button>
                    <button data-fw-view="left" onClick={() => {
                      threeRef.current?.setView?.(-90, 0);
                      setViewOpen(false);
                    }}>left (YZ-)</button>
                  </div>
                </div>
              )}
            </div>
          )}

          {/* Range inputs (X / Y / Z / r) live in the footer status
              bar now — see below. The toolbar keeps fit / grid / log /
              save / export buttons only. */}

          {/* axes ▾ — every property of the Axes object itself:
              Visible / Box / XGrid·YGrid·ZGrid / XDir·YDir·ZDir /
              XScale·YScale·ZScale. Grouping rule: if it's a field on
              the MATLAB HG2 Axes object, it lives here. Children of
              the axes (Title, Legend, Colorbar, ...) live in
              `decoration ▾` next. Both popovers share `displayReset`. */}
          <div className="ve-tools-group" ref={axesRef}>
            <button className="ve-btn"
                    onClick={() => setAxesOpen((o) => !o)}
                    title="Axes object: visible / grid / direction / scale">
              <svg width="11" height="11" viewBox="0 0 12 12">
                <path d="M2 1v10h10" stroke="currentColor" strokeWidth="1.2" fill="none"/>
              </svg>
              axes ▾
            </button>
            {axesOpen && (
              <div className="fw-pop">
                <div className="fw-pop-section">
                  <button onClick={() => { displayReset(); setAxesOpen(false); }}>default</button>
                </div>
                <div className="fw-pop-section">
                  <div className="fw-pop-head">visible</div>
                  <DisplayToggle label="axis" active={showAxis}
                                 onClick={() => setShowAxis((v) => !v)} />
                  {/* MATLAB HG2 rule: Axes.Visible='off' is the master
                      switch — it hides ticks, axis lines AND the Box.
                      We honour that: when axis is off the box doesn't
                      render even with Box='on'. Dim the box row so the
                      user sees their ✓ is currently no-op; click still
                      flips the cell-state flag so it'll take effect
                      once axis is turned back on. */}
                  <DisplayToggle label="box"  active={showBox}
                                 masked={!showAxis}
                                 maskedHint="Box is hidden because axis is off — turn axis on to see it."
                                 onClick={() => setShowBox((v) => !v)} />
                </div>
                {/* Grid sub-sections moved into their own toolbar
                    button `grid ▾` (next to axes ▾). Keeping them out
                    of this popover so axes ▾ stays focused on the
                    HG2 Axes object's non-grid properties (visible /
                    direction / scale). */}
                {/* Direction + scale collapsed into a single matrix —
                    rows = X/Y/Z, columns = reverse / log. Replaces
                    the two `reverse:` + `log scale:` sections (3 rows
                    each = 8 lines) with a 4-line block. Same UX
                    pattern as grid ▾'s matrix (checkbox-style toggle
                    cells with ✓ on active). */}
                <div className="fw-pop-section fw-pop-matrix">
                  <div className="fw-pop-head">reverse · log scale</div>
                  <MatrixHead labels={['rev', 'log']} />
                  <MatrixToggleRow label="X" cols={[
                    { active: xReverse, onClick: () => setXReverse((v) => !v), title: 'reverse direction' },
                    { active: xLog,     onClick: () => toggleAxisLog('x'),     title: 'log scale' },
                  ]} />
                  <MatrixToggleRow label="Y" cols={[
                    { active: yReverse, onClick: () => setYReverse((v) => !v), title: 'reverse direction' },
                    { active: yLog,     onClick: () => toggleAxisLog('y'),     title: 'log scale' },
                  ]} />
                  <MatrixToggleRow label="Z" cols={[
                    { active: zReverse, onClick: () => setZReverse((v) => !v), title: 'reverse direction' },
                    { active: zLog,     onClick: () => setZLog((v) => !v),     title: 'log scale' },
                  ]} />
                </div>
                {/* aspect: MATLAB `axis equal/square/image/tight/auto`
                    shorthand surfaced as a 5-button radio group.
                    Mutually-exclusive — clicking one sets it; the
                    current value is highlighted. `auto` is the default
                    (panel fills cell, no aspect lock). Active value
                    flows into CompositePlot's panel-shrink path which
                    enforces the corresponding contract. */}
                <div className="fw-pop-section">
                  {/* Section head names the active aggregate value
                      (`aspect: equal`) — when cells are mixed we show
                      `aspect: mixed` so the user sees there's no
                      uniform value before they look at the pills. */}
                  <div className="fw-pop-head">
                    aspect{axisModeAgg === null ? ': mixed'
                                                : (axisModeAgg && axisModeAgg !== 'auto' ? `: ${axisModeAgg}` : '')}
                  </div>
                  <div className="fw-pop-radio-row">
                    {['auto', 'equal', 'square', 'image', 'tight'].map((m) => {
                      // Active = uniform aggregate equals m. MIXED
                      // (axisModeAgg === null) → no pill active so
                      // the user doesn't think `auto` is the current
                      // value when really it's different per cell.
                      // Empty-string aggregate normalises to `auto`.
                      const isActive = axisModeAgg !== null
                                    && ((axisModeAgg || 'auto') === m);
                      return (
                        <button key={m}
                                className={`fw-pop-radio${isActive ? ' is-active' : ''}`}
                                title={({
                                  auto:   'panel fills cell; no aspect lock',
                                  equal:  '1 data unit X = 1 data unit Y (DataAspectRatio = [1 1 1])',
                                  square: 'plot box square regardless of data',
                                  image:  'equal + tight (default for imshow)',
                                  tight:  'limits exactly at data extent — no padding',
                                })[m] || ''}
                                onClick={() => setAxisMode(m)}>
                          {m}
                        </button>
                      );
                    })}
                  </div>
                </div>
              </div>
            )}
          </div>

          {/* grid ▾ — every per-axis grid the schema models, plus the
              combined master row. Lives in its own toolbar group
              (between axes ▾ and decoration ▾) because the grid
              surface alone is 3 sub-sections (master / Cartesian /
              Polar) and would dominate the axes ▾ popover otherwise.
              Mirrors MATLAB R2025b figure toolbar where Grid is a
              primary control. */}
          <div className="ve-tools-group" ref={gridRef}>
            <button className="ve-btn"
                    onClick={() => setGridOpen((o) => !o)}
                    title="Grid lines: cartesian (X/Y/Z) + polar (R/θ)">
              <svg width="11" height="11" viewBox="0 0 12 12">
                <path d="M4 0v12 M8 0v12 M0 4h12 M0 8h12"
                      stroke="currentColor" strokeWidth="1.2" fill="none"/>
              </svg>
              grid ▾
            </button>
            {gridOpen && (
              <div className="fw-pop">
                <div className="fw-pop-section">
                  <button onClick={() => { displayReset(); setGridOpen(false); }}>default</button>
                </div>
                {/* Matrix layout: one row per axis, two columns of
                    state-buttons (major / minor). Replaces the 2-row-
                    per-axis split (Cartesian: X / Cartesian minor: X)
                    so adding the minor family doesn't double the
                    popover height. Per the toolbar=universal policy
                    every row stays clickable regardless of figure
                    kind; clicks on inapplicable axes flip the schema
                    flag without visual effect (parity-clean). */}
                <div className="fw-pop-section fw-pop-matrix">
                  <div className="fw-pop-head">grid</div>
                  <MatrixHead labels={['maj', 'min']} />
                  <MatrixToggleRow label="all" cols={[
                    { active: showMajor, onClick: () => setShowMajor((v) => !v), title: 'major' },
                    { active: showMinor, onClick: () => setShowMinor((v) => !v), title: 'minor' },
                  ]} />
                </div>
                <div className="fw-pop-section fw-pop-matrix">
                  <div className="fw-pop-head">Cartesian</div>
                  <MatrixToggleRow label="X" cols={[
                    { active: xGrid,  onClick: () => setXGrid((v) => !v),  title: 'major' },
                    { active: xMinor, onClick: () => setXMinor((v) => !v), title: 'minor' },
                  ]} />
                  <MatrixToggleRow label="Y" cols={[
                    { active: yGrid,  onClick: () => setYGrid((v) => !v),  title: 'major' },
                    { active: yMinor, onClick: () => setYMinor((v) => !v), title: 'minor' },
                  ]} />
                  <MatrixToggleRow label="Z" cols={[
                    { active: zGrid,  onClick: () => setZGrid((v) => !v),  title: 'major' },
                    { active: zMinor, onClick: () => setZMinor((v) => !v), title: 'minor' },
                  ]} />
                </div>
                <div className="fw-pop-section fw-pop-matrix">
                  <div className="fw-pop-head">Polar</div>
                  <MatrixToggleRow label="R" cols={[
                    { active: rGrid,  onClick: () => setRGrid((v) => !v),  title: 'major' },
                    { active: rMinor, onClick: () => setRMinor((v) => !v), title: 'minor' },
                  ]} />
                  <MatrixToggleRow label="θ" cols={[
                    { active: thetaGrid,  onClick: () => setThetaGrid((v) => !v),  title: 'major' },
                    { active: thetaMinor, onClick: () => setThetaMinor((v) => !v), title: 'minor' },
                  ]} />
                </div>
              </div>
            )}
          </div>

          <div className="ve-tools-group" ref={displayRef}>
            <button className="ve-btn"
                    onClick={() => setDisplayOpen((o) => !o)}
                    title="Decoration objects: labels / legend / colorbar">
              <svg width="11" height="11" viewBox="0 0 12 12">
                <path d="M2 3h8M2 6h5M2 9h6"
                      stroke="currentColor" strokeWidth="1.2" fill="none" strokeLinecap="round"/>
              </svg>
              decoration ▾
            </button>
            {displayOpen && (
              <div className="fw-pop">
                <div className="fw-pop-section">
                  <button onClick={() => { displayReset(); setDisplayOpen(false); }}>default</button>
                </div>
                <div className="fw-pop-section">
                  <div className="fw-pop-head">labels</div>
                  {/* Pure text children of the Axes (HG2 Title / XLabel
                      / YLabel / ZLabel objects). No Location picker —
                      MATLAB Position is set by the renderer. */}
                  <DisplayToggle label="title" active={showTitle}
                                 onClick={() => setShowTitle((v) => !v)} />
                  <DisplayToggle label="X label" active={showXLabel}
                                 onClick={() => setShowXLabel((v) => !v)} />
                  <DisplayToggle label="Y label" active={showYLabel}
                                 onClick={() => setShowYLabel((v) => !v)} />
                  <DisplayToggle label="Z label" active={showZLabel}
                                 onClick={() => setShowZLabel((v) => !v)} />
                </div>
                <div className="fw-pop-section">
                  <div className="fw-pop-head">annotations</div>
                  {/* Standalone HG2 objects (Legend / Colorbar). Each
                      carries its own Location so it gets a side-opening
                      submenu next to the visibility toggle. */}
                  <DisplayToggle label="legend" active={showLegend}
                                 onClick={() => setShowLegend((v) => !v)} />
                  <FwPopLocationSubmenu
                    label="legend location"
                    value={legendLocationAgg}
                    options={[
                      { value: null,        label: 'default' },
                      { value: 'best',      label: 'best' },
                      { value: 'north',     label: 'north' },
                      { value: 'south',     label: 'south' },
                      { value: 'east',      label: 'east' },
                      { value: 'west',      label: 'west' },
                      { value: 'northeast', label: 'northeast' },
                      { value: 'northwest', label: 'northwest' },
                      { value: 'southeast', label: 'southeast' },
                      { value: 'southwest', label: 'southwest' },
                    ]}
                    onPick={(v) => setLegendLocation(v)} />
                  <DisplayToggle label="colorbar" active={showColorbar}
                                 onClick={() => setShowColorbar((v) => !v)} />
                  <FwPopLocationSubmenu
                    label="colorbar location"
                    value={colorbarLocationAgg}
                    options={[
                      { value: null,    label: 'default' },
                      { value: 'east',  label: 'east' },
                      { value: 'west',  label: 'west' },
                      { value: 'north', label: 'north' },
                      { value: 'south', label: 'south' },
                    ]}
                    onPick={(v) => setColorbarLocation(v)} />
                </div>
              </div>
            )}
          </div>

          {/* colormap ▾ — figure-wide palette picker. ALWAYS visible
              per the toolbar policy (universal brush). On a non-
              heatmap figure the picked palette is written to
              Axes.Colormap regardless — it has no visible effect now,
              but if a heatmap is later layered onto the same axes the
              UI-picked palette wins over the script default.
              SIBLING of decoration ▾ (NOT nested) so click-outside on
              the cmap trigger fires `setDisplayOpen(false)` via the
              shared document-mousedown handler. */}
          <div className="ve-tools-group" ref={cmapRef}>
            <button className="ve-btn"
                    onClick={() => setCmapOpen((o) => !o)}
                    title="Colormap (figure-wide; overrides per-cell picks)">
              <svg width="11" height="11" viewBox="0 0 12 12">
                <rect x="1" y="3" width="10" height="6" fill="none"
                      stroke="currentColor" strokeWidth="1"/>
                <rect x="1" y="3" width="2" height="6" fill="currentColor" opacity="0.25"/>
                <rect x="3" y="3" width="2" height="6" fill="currentColor" opacity="0.5"/>
                <rect x="5" y="3" width="2" height="6" fill="currentColor" opacity="0.75"/>
                <rect x="7" y="3" width="2" height="6" fill="currentColor"/>
                <rect x="9" y="3" width="2" height="6" fill="currentColor" opacity="0.5"/>
              </svg>
              colormap ▾
            </button>
            {cmapOpen && (
              <div className="fw-pop">
                <div className="fw-pop-section">
                  <button onClick={() => {
                    // Reset to script palette. setColormapOverride
                    // writes null to every cell's `colormap` field
                    // (single source of truth — no separate per-cell
                    // overrides to clear).
                    setColormapOverride(null);
                    setCmapOpen(false);
                  }}>default</button>
                </div>
                <div className="fw-pop-section">
                  <div className="fw-pop-head">palette</div>
                  {['parula', 'jet', 'hot', 'cool', 'gray', 'bone', 'copper',
                    'spring', 'summer', 'autumn', 'winter', 'hsv', 'viridis']
                    .map((cm) => {
                      const scriptDefault = (anyHeatmap && anyHeatmap.colormap) || 'parula';
                      // ✓ shown only when EVERY heatmap-bearing cell
                      // currently uses this palette (after applying
                      // per-cell overrides). For non-subplot figures
                      // it just compares the figure-wide effective
                      // value. For non-heatmap figures: ✓ tracks the
                      // user's UI pick alone (no heatmap colormap to
                      // compare against).
                      const active = aggColormap(
                        axesArr.map(axesToLegacyCell), cellsArr, cm);
                      return (
                        <button key={cm}
                                className="fw-pop-toggle"
                                onClick={() => {
                                  setColormapOverride(cm === scriptDefault ? null : cm);
                                  setCmapOpen(false);
                                }}>
                          <span>{cm}</span>
                          <span className="fw-pop-check">{active ? '✓' : ''}</span>
                        </button>
                      );
                    })}
                </div>
              </div>
            )}
          </div>

          <div className="ve-tools-spacer" />

          <div className="ve-tools-group" ref={saveRef}>
            <button className="ve-btn" onClick={() => setSaveOpen((o) => !o)}>
              <svg width="11" height="11" viewBox="0 0 12 12">
                <path d="M6 1v8M3 6l3 3 3-3M2 11h8" stroke="currentColor" fill="none" strokeLinecap="round"/>
              </svg>
              save / export ▾
            </button>
            {saveOpen && (
              <div className="fw-pop fw-pop-right">
                <div className="fw-pop-section">
                  <div className="fw-pop-head">image · screen</div>
                  <button
                    onClick={() => { exportSvg(); setSaveOpen(false); }}
                    disabled={is3D}
                    title={is3D ? 'SVG export not available for 3-D figures (WebGL geometry has no vector form). Use PNG.' : ''}>
                    SVG (vector){is3D ? ' · n/a for 3-D' : ''}
                  </button>
                  <button onClick={() => { exportPng(2); setSaveOpen(false); }}>PNG @2×</button>
                </div>
                <div className="fw-pop-section">
                  <div className="fw-pop-head">image · print (300 DPI)</div>
                  <button onClick={() => { exportPngPrint(85);  setSaveOpen(false); }}>PNG · 1 column (85 mm)</button>
                  <button onClick={() => { exportPngPrint(170); setSaveOpen(false); }}>PNG · 2 columns (170 mm)</button>
                  <button onClick={() => { exportPngPrint(210); setSaveOpen(false); }}>PNG · A4 width (210 mm)</button>
                </div>
                <div className="fw-pop-section">
                  <div className="fw-pop-head">data</div>
                  <button onClick={() => { exportCsv(); setSaveOpen(false); }}>CSV</button>
                  <button onClick={() => { exportTsv(); setSaveOpen(false); }}>TSV</button>
                  <button onClick={() => { exportJson(); setSaveOpen(false); }}>JSON</button>
                </div>
              </div>
            )}
          </div>
        </div>

        {/* sgtitle — figure-level "super-title", set via sgtitle(...).
            Always rendered so the grid row count stays stable (6 rows,
            see .fw-window grid-template-rows); CSS `.fw-supertitle:empty
            { display:none }` collapses the strip when no super-title is
            set. If we returned null instead, the grid would re-map
            children-to-rows and shift canvas-wrap into the `auto` slot. */}
        <div className="fw-supertitle">{figure.superTitle || ''}</div>

        <div className="fw-canvas-wrap" ref={wrapRef}>
          <div style={{ position: 'relative', width: '100%', height: '100%' }}>
            {renderFigure(figure, {
              width: size.w, height: size.h,
              viewport, setViewport,
              major: showMajor, minor: showMinor,
              // Per-axis grid (MATLAB XGrid / YGrid for cartesian +
              // RGrid / ThetaGrid for polar). For non-subplot the
              // aggregate is the value (single axes); for subplot
              // SubplotGrid forwards each cell's own axes-derived value.
              xGrid: xGrid, yGrid: yGrid,
              xMinor: xMinor, yMinor: yMinor,
              rGrid: rGrid, thetaGrid: thetaGrid,
              rMinor: rMinor, thetaMinor: thetaMinor,
              // MATLAB Visible / Box / XDir / YDir / ZDir overrides.
              // Pass only for non-subplot — SubplotGrid resolves per-
              // cell from each cell's axes (legacy cell prop).
              ...(isSubplot ? {} : {
                axisVisible: showAxis,
                boxOn: showBox,
                xReverse, yReverse, zReverse,
                axisMode: axisModeAgg,
                legendLocation: legendLocationAgg,
                colorbarLocation: colorbarLocationAgg,
              }),
              fontScale: 1.15,
              engine,
              xLog, yLog, zLog,
              setXLog, setYLog, setZLog,
              colorOverride, setColorOverride,
              colormapOverride, setColormapOverride,
              // showLegend gates BOTH the CompositePlot SVG-internal
              // legend block and the (now removed) HTML overlay. One
              // legend, controlled by the toolbar toggle.
              showLegend,
              // Visibility flags from display ▾. CompositePlot /
              // Composite3DPlot / SubplotGrid honour these by skipping
              // the corresponding <text> render path. State lives here
              // (not in figure JSON) so a script re-run doesn't reset.
              showTitle, showXLabel, showYLabel, showZLabel,
              showColorbar,
              // Display setters — passed so the right-click ПКМ inside
              // CompositePlot can surface Axes ▶ + Decoration ▶ submenus
              // that mirror the toolbar's axes ▾ / decoration ▾ state.
              // Non-subplot path uses figure-wide fanAll setters (one
              // axes object); subplot wraps these per-cell in
              // SubplotGrid via makeCellDisplaySetter.
              ...(isSubplot ? {} : {
                setShowMajor, setShowMinor,
                setXGrid, setYGrid,
                setRGrid, setThetaGrid,
                setXMinor, setYMinor, setZMinor,
                setRMinor, setThetaMinor,
                setAxisMode,
                setShowAxis, setShowBox,
                setXReverse, setYReverse, setZReverse,
                setShowTitle, setShowXLabel, setShowYLabel, setShowZLabel,
                setShowLegend, setShowColorbar,
                setLegendLocation, setColorbarLocation,
              }),
              // Top-level Reset + Save/Export bridge for the context
              // menu. ПКМ surfaces these as a 🏠 Reset row + Save/Export
              // submenu. Handlers run with no extra wrapping — the menu
              // closes automatically after the click via ContextMenu's
              // own onClose path.
              onResetAll: resetAll,
              onExportSvg: exportSvg,
              onExportPng2x: () => exportPng(2),
              onExportPngPrint85:  () => exportPngPrint(85),
              onExportPngPrint170: () => exportPngPrint(170),
              onExportPngPrint210: () => exportPngPrint(210),
              onExportCsv: exportCsv,
              onExportTsv: exportTsv,
              onExportJson: exportJson,
              // fitSignal — incrementing counter consumed by SubplotGrid
              // to fit each cell's viewport when the toolbar Fit X/Y/Z
              // is clicked on a subplot figure.
              fitSignal,
              // Per-cell state (single source of truth) lives in
              // FigureWindow now. SubplotGrid receives the array + per-
              // cell setter factories and fans them out to each cell's
              // CompositePlot. Non-subplot CompositePlot uses cells[0]
              // values flattened into the same prop names already passed
              // above (showMajor / xLog / ...), and the figure-wide
              // setters wrap fanAll().
              cellState: cells,
              makeCellDisplaySetter, makeCellColormapSetter,
              makeCellDisplayReset, makeCellColormapReset,
              // Non-subplot CompositePlot uses these directly (no
              // SubplotGrid in the chain). For subplot, SubplotGrid
              // overrides them with per-cell wrappers.
              onDisplayReset: displayReset,
              onColormapReset: () => setColormapOverride(null),
              // 3-D specific — Composite3DPlot ignores these for non-3-D.
              // Skip the override on the very first render when viewport
              // is still the [-1,1] placeholder cube (otherwise computeBBox
              // would clamp data to the placeholder, onBBox would echo it
              // back, and the auto-fill loop would deadlock at -1 / 1).
              viewport3d: (is3D && !isPlaceholder3D(viewport)) ? viewport : null,
              onBBox: is3D ? onComposite3DBBox : null,
            }, threeRef)}
            {/* The HTML-overlay legend that lived here used to double-
                draw on top of CompositePlot's SVG legend whenever the
                script called legend(...). Removed — CompositePlot's
                internal block (gated on showLegend prop) is the single
                source of truth now. */}
          </div>
        </div>

        {/* Range-input row — was in the toolbar, moved to the footer
            so it sits next to the live viewport readout. 3-D figures
            get a Z row in addition to X / Y. */}
        {!isSubplot && (
          <div className="fw-range-row">
            {isPolar ? (
              <div className="ve-tools-group fw-range-group">
                <span className="ve-label">r</span>
                <NumberInput value={viewport.r[0]}
                  onCommit={(n) => setViewport({ ...viewport, r: [n, viewport.r[1]] })} />
                <span className="fw-range-sep">→</span>
                <NumberInput value={viewport.r[1]}
                  onCommit={(n) => setViewport({ ...viewport, r: [viewport.r[0], n] })} />
                {/* θ in degrees, MATLAB convention. defaultPolarViewport
                    fills [0, 360] when the script didn't set thetalim;
                    user can narrow the sweep here for a pie-wedge plot. */}
                <span className="ve-label" style={{ marginLeft: 6 }}>θ°</span>
                <NumberInput value={(viewport.theta || [0, 360])[0]}
                  onCommit={(n) => setViewport({ ...viewport,
                    theta: [n, (viewport.theta || [0, 360])[1]] })} />
                <span className="fw-range-sep">→</span>
                <NumberInput value={(viewport.theta || [0, 360])[1]}
                  onCommit={(n) => setViewport({ ...viewport,
                    theta: [(viewport.theta || [0, 360])[0], n] })} />
              </div>
            ) : is3D ? (
              <div className="ve-tools-group fw-range-group">
                <span className="ve-label">x</span>
                <NumberInput value={viewport.x[0]} onCommit={(n) => setViewport({ ...viewport, x: [n, viewport.x[1]] })} />
                <span className="fw-range-sep">→</span>
                <NumberInput value={viewport.x[1]} onCommit={(n) => setViewport({ ...viewport, x: [viewport.x[0], n] })} />
                <span className="ve-label" style={{ marginLeft: 6 }}>y</span>
                <NumberInput value={viewport.y[0]} onCommit={(n) => setViewport({ ...viewport, y: [n, viewport.y[1]] })} />
                <span className="fw-range-sep">→</span>
                <NumberInput value={viewport.y[1]} onCommit={(n) => setViewport({ ...viewport, y: [viewport.y[0], n] })} />
                <span className="ve-label" style={{ marginLeft: 6 }}>z</span>
                <NumberInput value={viewport.z[0]} onCommit={(n) => setViewport({ ...viewport, z: [n, viewport.z[1]] })} />
                <span className="fw-range-sep">→</span>
                <NumberInput value={viewport.z[1]} onCommit={(n) => setViewport({ ...viewport, z: [viewport.z[0], n] })} />
              </div>
            ) : (
              <div className="ve-tools-group fw-range-group">
                <span className="ve-label">x</span>
                <NumberInput value={viewport.x[0]} onCommit={(n) => setViewport({ ...viewport, x: [n, viewport.x[1]] })} />
                <span className="fw-range-sep">→</span>
                <NumberInput value={viewport.x[1]} onCommit={(n) => setViewport({ ...viewport, x: [viewport.x[0], n] })} />
                <span className="ve-label" style={{ marginLeft: 6 }}>y</span>
                <NumberInput value={viewport.y[0]} onCommit={(n) => setViewport({ ...viewport, y: [n, viewport.y[1]] })} />
                <span className="fw-range-sep">→</span>
                <NumberInput value={viewport.y[1]} onCommit={(n) => setViewport({ ...viewport, y: [viewport.y[0], n] })} />
              </div>
            )}
          </div>
        )}

        <div className="fw-status">
          {isSubplot ? (
            <span>{figure.cells.length} axes · per-cell pan/zoom</span>
          ) : isPolar ? (
            <>
              <span>r ∈ [{fmtVp(viewport.r[0])}, {fmtVp(viewport.r[1])}]</span>
              {Array.isArray(viewport.theta) && (
                <>
                  <span className="ve-sep" />
                  <span>θ ∈ [{fmtVp(viewport.theta[0])}°, {fmtVp(viewport.theta[1])}°]</span>
                </>
              )}
            </>
          ) : is3D ? (
            <>
              <span>x ∈ [{fmtVp(viewport.x[0])}, {fmtVp(viewport.x[1])}]</span>
              <span className="ve-sep" />
              <span>y ∈ [{fmtVp(viewport.y[0])}, {fmtVp(viewport.y[1])}]</span>
              <span className="ve-sep" />
              <span>z ∈ [{fmtVp(viewport.z[0])}, {fmtVp(viewport.z[1])}]</span>
            </>
          ) : (
            <>
              <span>x ∈ [{fmtVp(viewport.x[0])}, {fmtVp(viewport.x[1])}]</span>
              <span className="ve-sep" />
              <span>y ∈ [{fmtVp(viewport.y[0])}, {fmtVp(viewport.y[1])}]</span>
            </>
          )}
          <span className="ve-spacer" />
          <span>{isPolar ? 'drag · zoom rMax' : is3D ? 'drag · orbit · wheel · dolly' : 'drag · pan'}</span>
          {!is3D && (
            <>
              <span className="ve-sep" />
              <span>{isPolar ? 'wheel · zoom' : 'wheel · zoom xy · ⌃ x · ⇧ y'}</span>
            </>
          )}
          <span className="ve-sep" />
          <span>dbl-click · reset</span>
          <span className="ve-sep" />
          <span>0 · reset · Esc · close</span>
        </div>
      </div>
    </div>
  );
}
