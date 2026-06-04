// figureWindow.controls.jsx — small presentational controls used by the
// FigureWindow toolbar popovers: a debounced number input, the legend /
// colorbar location submenus, the display toggle, and the matrix grid rows.
import { useState, useEffect, useLayoutEffect, useRef } from 'react';

export function NumberInput({ value, onCommit, width = 88 }) {
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
export function FwPopLocationSubmenu({ label, value, options, onPick }) {
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

export function FwPopRowsOrSubmenu({ rows, label, threshold = 5 }) {
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
export function DisplayToggle({ label, active, disabled = false, disabledHint = '',
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
export function MatrixHead({ labels }) {
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
export function MatrixToggleRow({ label, cols }) {
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
