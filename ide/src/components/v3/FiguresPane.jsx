import { useEffect, useLayoutEffect, useRef, useState } from 'react';
import InteractivePlot from './InteractivePlot';
import Heatmap from './Heatmap';
import PolarPlot from './PolarPlot';

/** Pick the right renderer for a figure based on its `kind`. */
function renderFigure(figure, props) {
  if (figure.kind === 'heatmap') return <Heatmap figure={figure} {...props} />;
  if (figure.kind === 'polar')   return <PolarPlot figure={figure} {...props} />;
  return <InteractivePlot figure={figure} {...props} />;
}

/**
 * Preview card. The body uses CSS `aspect-ratio` so it fills the pane width
 * and computes its height proportionally. We measure the actual rendered
 * size with `getBoundingClientRect` on every render and re-measure on
 * resize/RO so the SVG inside InteractivePlot redraws at the matching pixel
 * dimensions. Aspect comes from CSS — the JS just mirrors it for SVG.
 */
const PREVIEW_ASPECT  = 1.7; // width / height — keep in sync with CSS rule below

function FigurePreviewCard({ figure, onExpand, onClose }) {
  // Polar plots don't pan/zoom — they have no Cartesian viewport to track.
  const initialVp = (figure.xRange && figure.yRange)
    ? { x: figure.xRange.slice(), y: figure.yRange.slice() }
    : { x: [-1, 1], y: [-1, 1] };
  const [viewport, setViewport] = useState(initialVp);
  const ref = useRef(null);
  const [size, setSize] = useState({ w: 320, h: Math.round(320 / PREVIEW_ASPECT) });

  // Single measure pipeline — used both for the initial mount-time read
  // (useLayoutEffect, runs synchronously before paint) and for resize signals
  // (ResizeObserver / window.resize). Both deps arrays are `[]` so the effect
  // bodies don't rerun on every render, which would otherwise feed back into
  // setSize and trip React's "Maximum update depth exceeded" check.
  useLayoutEffect(() => {
    const el = ref.current;
    if (!el) return;
    const r = el.getBoundingClientRect();
    if (!r.width) return;
    const ww = Math.max(200, Math.round(r.width));
    const hh = Math.max(120, Math.round(ww / PREVIEW_ASPECT));
    setSize((prev) => (Math.abs(prev.w - ww) > 0.5 || Math.abs(prev.h - hh) > 0.5
      ? { w: ww, h: hh } : prev));
  }, []);

  useEffect(() => {
    const el = ref.current;
    if (!el) return;
    const remeasure = () => {
      const r = el.getBoundingClientRect();
      if (!r.width) return;
      const ww = Math.max(200, Math.round(r.width));
      const hh = Math.max(120, Math.round(ww / PREVIEW_ASPECT));
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
  }, []);

  return (
    <div className="fp-card">
      <div className="fp-card-head">
        <span className="fp-card-title">Figure {figure.id}</span>
        <button className="fp-card-icon" title="Expand" onClick={onExpand}>
          <svg width="10" height="10" viewBox="0 0 12 12">
            <path d="M2 2h4M2 2v4M10 10H6M10 10V6" stroke="currentColor" strokeWidth="1.4" fill="none"/>
          </svg>
        </button>
        <button className="fp-card-icon" title="Close" onClick={onClose}>×</button>
      </div>
      <div className="fp-card-body" ref={ref}
        style={{
          aspectRatio: String(PREVIEW_ASPECT),
          width: '100%',
          position: 'relative',
          overflow: 'hidden',
        }}
        onDoubleClick={onExpand} onClick={onExpand}>
        {/* SVG is positioned absolutely so its intrinsic pixel size doesn't
            override the body's aspect-ratio-driven height. */}
        <div style={{ position: 'absolute', inset: 0 }}>
          {renderFigure(figure, {
            width: size.w, height: size.h,
            viewport, setViewport,
            minor: true, fontScale: 0.9, interactive: false,
          })}
        </div>
      </div>
    </div>
  );
}

/**
 * Right-pane Figures stack. Receives the live `figures` array (from the
 * engine) and surfaces expand / close-one / close-all actions to the parent.
 *
 * `unsupportedCount` is the number of figures that the new InteractivePlot
 * can't render (heatmap, surf, quiver, etc.). When > 0 a small banner offers
 * to open the legacy d3 renderer in a modal.
 */
export default function FiguresPane({
  figures, unsupportedCount = 0,
  onOpenLegacy,
  onExpand, onCloseFigure, onCloseAll,
}) {
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
          {figures.length > 0 && (
            <button className="fp-closeall" onClick={onCloseAll}>
              Close all <span style={{ marginLeft: 4 }}>×</span>
            </button>
          )}
        </div>
      </div>
      {unsupportedCount > 0 && (
        <button onClick={onOpenLegacy}
          style={{
            margin: '6px 10px', padding: '6px 10px',
            background: 'var(--bg-2)', border: '1px solid var(--line)',
            borderRadius: 6, color: 'var(--fg-2)', cursor: 'pointer',
            fontSize: 11, textAlign: 'left',
          }}>
          {unsupportedCount} figure{unsupportedCount === 1 ? '' : 's'} need legacy
          renderer (heatmap / surface / etc.) — click to view
        </button>
      )}
      <div className="fp-stack">
        {figures.map((fig) => (
          <FigurePreviewCard key={fig.id} figure={fig}
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
