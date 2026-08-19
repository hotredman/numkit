import { useEffect, useLayoutEffect, useRef, useState } from 'react';
import CompositePlot from './CompositePlot';
import Composite3DPlot from './Composite3DPlot';
import FigureErrorBoundary from './FigureErrorBoundary';
import PolarPlot from './PolarPlot';
import SubplotGrid from './SubplotGrid';
import { previewViewport } from './plotUtils';

import { loadSettings } from '../../settings';

/** Pick the right renderer for a figure based on its `kind`. */
function renderFigure(figure, props) {
  if (figure.kind === 'subplot')     return <SubplotGrid     figure={figure} {...props} />;
  if (figure.kind === 'composite3d') {
    return (
      <FigureErrorBoundary label="composite3d" figureId={figure.id}
        width={props.width} height={props.height}>
        <Composite3DPlot figure={figure} {...props} />
      </FigureErrorBoundary>
    );
  }
  if (figure.kind === 'composite')   return <CompositePlot   figure={figure} {...props} />;
  if (figure.kind === 'polar')       return <PolarPlot       figure={figure} {...props} />;
  return <CompositePlot figure={figure} {...props} />;
}

function getAspectRatio(str) {
  if (str === '4:3') return 4 / 3;
  if (str === '16:10') return 16 / 10;
  if (str === '1:1') return 1;
  return 16 / 9;
}

function FigurePreviewCard({ figure, aspectStr, onExpand, onClose }) {
  // Preview is non-interactive (no pan/zoom), so derive the viewport directly
  // from the figure's data extent on every render — useState would freeze it
  // at mount and stale ranges from a previous run would leak in when the
  // figure is replaced by a new script execution under the same id.
  const viewport = previewViewport(figure);
  const setViewport = () => {};   // no-op for non-interactive preview
  const ref = useRef(null);
  
  const aspect = getAspectRatio(aspectStr);
  const [size, setSize] = useState({ w: 320, h: Math.round(320 / aspect) });

  useLayoutEffect(() => {
    const el = ref.current;
    if (!el) return;
    const r = el.getBoundingClientRect();
    if (!r.width) return;
    const ww = Math.max(100, Math.round(r.width));
    const hh = Math.max(80, Math.round(ww / aspect));
    setSize((prev) => (Math.abs(prev.w - ww) > 0.5 || Math.abs(prev.h - hh) > 0.5
      ? { w: ww, h: hh } : prev));
  }, [aspect]);

  useEffect(() => {
    const el = ref.current;
    if (!el) return;
    const remeasure = () => {
      const r = el.getBoundingClientRect();
      if (!r.width) return;
      const ww = Math.max(100, Math.round(r.width));
      const hh = Math.max(80, Math.round(ww / aspect));
      setSize((prev) => (Math.abs(prev.w - ww) > 0.5 || Math.abs(prev.h - hh) > 0.5
        ? { w: ww, h: hh } : prev));
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
  }, [aspect]);

  return (
    <div
      className="fp-card"
      onClick={onExpand}
      role="button"
      aria-label={`Open Figure ${figure.id}`}
    >
      <div className="fp-card-head">
        <span className="fp-card-title">Figure {figure.id}</span>
        <button className="fp-card-icon" title="Close"
          onClick={(e) => { e.stopPropagation(); onClose(); }}>×</button>
      </div>
      <div className="fp-card-body" ref={ref}
        style={{
          aspectRatio: String(aspect),
          width: '100%',
          position: 'relative',
          overflow: 'hidden',
        }}>
        <div style={{ position: 'absolute', inset: 0 }}>
          {renderFigure(figure, {
            width: size.w, height: size.h,
            viewport, setViewport,
            major: figure.grid === 'on',
            minor: figure.gridMinor === 'on',
            xLog: figure.xscale === 'log',
            yLog: figure.yscale === 'log',
            fontScale: 0.9, interactive: false,
          })}
        </div>
      </div>
    </div>
  );
}

/**
 * Right-pane Figures stack. Receives the live `figures` array (from the
 * engine) and surfaces expand / close-one / close-all actions to the parent.
 */
export default function FiguresPane({
  figures, unsupportedCount = 0,
  onExpand, onCloseFigure, onCloseAll,
}) {
  const [columns, setColumns] = useState(() => {
    try {
      const saved = localStorage.getItem('numkit_figures_cols');
      if (saved) {
        const val = parseInt(saved, 10);
        if (val >= 1 && val <= 4) return val;
      }
    } catch (_) {}
    return 1;
  });

  const [aspectRatio, setAspectRatio] = useState(() => {
    try {
      const saved = localStorage.getItem('numkit_figures_aspect');
      if (saved && (saved === '16:9' || saved === '4:3' || saved === '16:10' || saved === '1:1')) {
        return saved;
      }
    } catch (_) {}
    return loadSettings().plotAspectRatio || '16:9';
  });

  const [colsOpen, setColsOpen] = useState(false);
  const [aspectOpen, setAspectOpen] = useState(false);

  const colsMenuRef = useRef(null);
  const aspectMenuRef = useRef(null);

  // Sync settings when external settings modal changes
  useEffect(() => {
    const onSettings = (e) => {
      if (e.detail?.plotAspectRatio) {
        setAspectRatio(e.detail.plotAspectRatio);
      }
    };
    window.addEventListener('numkitSettingsChanged', onSettings);
    return () => window.removeEventListener('numkitSettingsChanged', onSettings);
  }, []);

  // Close menus on outside click or Esc
  useEffect(() => {
    const onMouseDown = (e) => {
      if (colsMenuRef.current && !colsMenuRef.current.contains(e.target)) {
        setColsOpen(false);
      }
      if (aspectMenuRef.current && !aspectMenuRef.current.contains(e.target)) {
        setAspectOpen(false);
      }
    };
    const onKeyDown = (e) => {
      if (e.key === 'Escape') {
        setColsOpen(false);
        setAspectOpen(false);
      }
    };
    document.addEventListener('mousedown', onMouseDown);
    document.addEventListener('keydown', onKeyDown);
    return () => {
      document.removeEventListener('mousedown', onMouseDown);
      document.removeEventListener('keydown', onKeyDown);
    };
  }, []);

  const handleSetColumns = (n) => {
    setColumns(n);
    try {
      localStorage.setItem('numkit_figures_cols', String(n));
    } catch (_) {}
    setColsOpen(false);
  };

  const handleSetAspect = (ratio) => {
    setAspectRatio(ratio);
    try {
      localStorage.setItem('numkit_figures_aspect', ratio);
    } catch (_) {}
    setAspectOpen(false);
  };

  return (
    <div className="figures">
      <div className="figures-head">
        <div className="figures-tabs">
          <span className="figures-title">
            <svg width="12" height="12" viewBox="0 0 12 12">
              <rect x="1" y="1" width="4" height="4" fill="#7fd99a"/>
              <rect x="6" y="1" width="5" height="4" fill="#5fb3d4"/>
              <rect x="1" y="6" width="5" height="5" fill="#e9b870"/>
              <rect x="7" y="6" width="4" height="5" fill="#9b8cf2"/>
            </svg>
            Figures <span className="fp-count">{figures.length}</span>
          </span>
        </div>
        <div className="figures-actions">
          {/* Columns dropdown */}
          <div className="fp-menu-wrap" ref={colsMenuRef}>
            <button
              className={`fp-head-btn ${colsOpen ? 'active' : ''}`}
              onClick={() => { setColsOpen((v) => !v); setAspectOpen(false); }}
              title="Number of columns in figures panel"
            >
              <span className="fp-btn-label">Cols: {columns}</span>
              <span className="fp-arrow">▾</span>
            </button>
            {colsOpen && (
              <div className="fp-dropdown-menu">
                <div className="fp-dropdown-head">COLUMNS</div>
                {[1, 2, 3, 4].map((n) => (
                  <button
                    key={n}
                    className={`fp-dropdown-item ${columns === n ? 'selected' : ''}`}
                    onClick={() => handleSetColumns(n)}
                  >
                    <span>{n} {n === 1 ? 'column' : 'columns'}</span>
                    {columns === n && <span className="fp-check">✓</span>}
                  </button>
                ))}
              </div>
            )}
          </div>

          {/* Aspect ratio dropdown */}
          <div className="fp-menu-wrap" ref={aspectMenuRef}>
            <button
              className={`fp-head-btn ${aspectOpen ? 'active' : ''}`}
              onClick={() => { setAspectOpen((v) => !v); setColsOpen(false); }}
              title="Aspect ratio for figure previews"
            >
              <span className="fp-btn-label">{aspectRatio}</span>
              <span className="fp-arrow">▾</span>
            </button>
            {aspectOpen && (
              <div className="fp-dropdown-menu">
                <div className="fp-dropdown-head">ASPECT RATIO</div>
                {['16:9', '4:3', '16:10', '1:1'].map((ratio) => (
                  <button
                    key={ratio}
                    className={`fp-dropdown-item ${aspectRatio === ratio ? 'selected' : ''}`}
                    onClick={() => handleSetAspect(ratio)}
                  >
                    <span>{ratio}</span>
                    {aspectRatio === ratio && <span className="fp-check">✓</span>}
                  </button>
                ))}
              </div>
            )}
          </div>

          <button
            className="fp-closeall"
            onClick={onCloseAll}
            disabled={figures.length === 0}
            title="Close all figures"
          >
            Close all <span style={{ marginLeft: 4 }}>×</span>
          </button>
        </div>
      </div>
      {unsupportedCount > 0 && (
        <div
          style={{
            margin: '6px 10px', padding: '6px 10px',
            background: 'var(--bg-2)', border: '1px solid var(--line)',
            borderRadius: 6, color: 'var(--fg-2)',
            fontSize: 11,
          }}>
          {unsupportedCount} figure{unsupportedCount === 1 ? '' : 's'} of an
          unsupported plot type — render skipped.
        </div>
      )}
      <div
        className={`fp-stack ${columns > 1 ? 'fp-grid' : ''}`}
        style={columns > 1 ? {
          display: 'grid',
          gridTemplateColumns: `repeat(${columns}, minmax(0, 1fr))`,
          gap: '10px',
          alignContent: 'start',
        } : undefined}
      >
        {figures.map((fig) => (
          <FigurePreviewCard key={fig.id} figure={fig}
            aspectStr={aspectRatio}
            onExpand={() => onExpand(fig)}
            onClose={() => onCloseFigure(fig.id)} />
        ))}
        {figures.length === 0 && unsupportedCount === 0 && (
          <div className="fp-empty">No figures. Run a script to create plots.</div>
        )}
      </div>
    </div>
  );
}
