import { useEffect, useRef, useState } from 'react';
import InteractivePlot from './InteractivePlot';

function FigurePreviewCard({ figure, onExpand, onClose }) {
  const [viewport, setViewport] = useState({ x: figure.xRange.slice(), y: figure.yRange.slice() });
  const ref = useRef(null);
  const [size, setSize] = useState({ w: 380, h: 240 });
  useEffect(() => {
    function update() {
      const el = ref.current; if (!el) return;
      const r = el.getBoundingClientRect();
      setSize({ w: Math.max(280, r.width), h: 220 });
    }
    update();
    window.addEventListener('resize', update);
    return () => window.removeEventListener('resize', update);
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
      <div className="fp-card-body" ref={ref} onDoubleClick={onExpand} onClick={onExpand}>
        <InteractivePlot figure={figure} width={size.w} height={size.h}
          viewport={viewport} setViewport={setViewport}
          minor={true} fontScale={0.9} interactive={false} />
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
