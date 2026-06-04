// MatrixPanel.jsx — numeric matrix viewer: a virtualized table (VirtualTable)
// + the MatrixPanel toolbar (notation / precision / heatmap / N-D slice nav)
// with tile-based fetch for huge matrices. fmt is the cell formatter; the
// inline-plot panel is reused for column plotting.
import { useState, useEffect, useRef, useCallback } from 'react';
import { valueToMatlabRHS } from './inspectorOps';
import StatsBar, { useStatChooser, StatChooserButton } from './StatsBar';
import { heatmapCellBackground } from './valueColumns';
import { pageCount, pageToSubs, subsToPage } from './sliceNav';
import { SaveAsMenu, InlinePlot } from './InlinePlot';

function fmt(v, opts = {}) {
  if (typeof v === 'string') return v;
  if (v === 0) return '0';
  if (Number.isInteger(v) && Math.abs(v) < 10000) return String(v);
  const abs = Math.abs(v);
  if (abs >= 1e5 || (abs > 0 && abs < 1e-3)) return v.toExponential(opts.exp ?? 3);
  return v.toFixed(opts.fix ?? 4);
}


/* ======================================================================== */
/* Virtualised Variable Editor table                                        */
/* ======================================================================== */
//
// Renders only the cells currently visible in the scroll viewport. With a
// 512×512 matrix that drops the live DOM-cell count from 262144 to ~30×10
// regardless of total size, which is the difference between an unusable
// editor and instant precision-slider response.
//
// Sizing is fixed per cell (ROW_H × COL_W) so we can compute the visible
// window from scroll offsets alone — no per-row measurement needed. Spacer
// rows (top/bottom) and spacer cells (left/right) preserve the natural
// scroll height/width so the browser's native scrollbar still tracks the
// full extent.

const ROW_H    = 22;     // matches CSS .ve-table tbody td natural height
const COL_W    = 88;     // matches CSS min-width: 88px
const CORNER_W = 60;     // corner cell + row-head width
const OVERSCAN = 6;      // extra rows/cols above/below to make scroll look continuous

// Tile-based fetch state for huge matrices. The cache is keyed by
// `${tileR},${tileC}` (in tile units, not cell coords).
export const TILE = 64;
export const TILE_MODE_THRESHOLD = 250000;  // tile-mode for matrices with more cells than this

export function VirtualTable({
  tableRef, rows, cols, getCellValue,
  activeCell, setActiveCell,
  editing, setEditing, editVal, setEditVal, commitEdit, inputRef,
  heatmap, stats, format,
  readOnly = false,
}) {
  const [scroll, setScroll] = useState({ top: 0, left: 0, viewW: 800, viewH: 400 });

  // Scroll + resize listeners on the wrap. ResizeObserver catches both modal
  // resize and the inline-plot toggle that shrinks the table area.
  useEffect(() => {
    const wrap = tableRef.current;
    if (!wrap) return;
    let raf = 0;
    const update = () => {
      cancelAnimationFrame(raf);
      raf = requestAnimationFrame(() => {
        setScroll({
          top:   wrap.scrollTop,
          left:  wrap.scrollLeft,
          viewW: wrap.clientWidth,
          viewH: wrap.clientHeight,
        });
      });
    };
    update();
    wrap.addEventListener('scroll', update, { passive: true });
    const ro = (typeof ResizeObserver !== 'undefined') ? new ResizeObserver(update) : null;
    ro?.observe(wrap);
    return () => {
      cancelAnimationFrame(raf);
      wrap.removeEventListener('scroll', update);
      ro?.disconnect();
    };
  }, [tableRef]);

  // Compute visible row/col window. Below 200 rows / 200 cols just render
  // everything — the virtualisation overhead isn't worth it.
  const fullRender = rows < 200 && cols < 200;

  const firstRow = fullRender ? 0
    : Math.max(0, Math.floor(scroll.top / ROW_H) - OVERSCAN);
  const lastRow  = fullRender ? rows - 1
    : Math.min(rows - 1, Math.ceil((scroll.top + scroll.viewH) / ROW_H) + OVERSCAN);

  const colScrollLeft = Math.max(0, scroll.left - CORNER_W);
  const firstCol = fullRender ? 0
    : Math.max(0, Math.floor(colScrollLeft / COL_W) - OVERSCAN);
  const lastCol  = fullRender ? cols - 1
    : Math.min(cols - 1, Math.ceil((colScrollLeft + scroll.viewW) / COL_W) + OVERSCAN);

  const topPadH    = firstRow * ROW_H;
  const bottomPadH = (rows - 1 - lastRow) * ROW_H;
  const leftPadW   = firstCol * COL_W;
  const rightPadW  = (cols - 1 - lastCol) * COL_W;

  const visibleRows = [];
  for (let r = firstRow; r <= lastRow; r++) visibleRows.push(r);
  const visibleCols = [];
  for (let c = firstCol; c <= lastCol; c++) visibleCols.push(c);

  return (
    <div className="ve-table-wrap" ref={tableRef}>
      <table className="ve-table">
        <thead>
          <tr>
            <th className="ve-corner">{rows}×{cols}</th>
            {leftPadW > 0 && <th aria-hidden="true" style={{ minWidth: leftPadW, padding: 0, border: 'none' }} />}
            {visibleCols.map((c) => (
              <th key={c} className={c === activeCell.c ? 'is-active' : ''}
                style={{ minWidth: COL_W }}>{c + 1}</th>
            ))}
            {rightPadW > 0 && <th aria-hidden="true" style={{ minWidth: rightPadW, padding: 0, border: 'none' }} />}
          </tr>
        </thead>
        <tbody>
          {topPadH > 0 && (
            <tr aria-hidden="true" style={{ height: topPadH }}>
              <td colSpan={visibleCols.length + 3} style={{ padding: 0, border: 'none' }} />
            </tr>
          )}
          {visibleRows.map((r) => (
            <tr key={r} style={{ height: ROW_H }}>
              <th className={`ve-rowhead ${r === activeCell.r ? 'is-active' : ''}`}>{r + 1}</th>
              {leftPadW > 0 && <td aria-hidden="true" style={{ minWidth: leftPadW, padding: 0, border: 'none' }} />}
              {visibleCols.map((c) => {
                const v = getCellValue(r, c);
                const isActive  = activeCell.r === r && activeCell.c === c;
                const isEditing = editing && editing.r === r && editing.c === c;
                // Heatmap background (logical booleans coerced to 1/0 inside).
                const bg = heatmapCellBackground(v, stats, heatmap);
                return (
                  <td
                    key={c}
                    className={isActive ? 'is-active' : ''}
                    style={{ background: bg, minWidth: COL_W }}
                    onClick={() => setActiveCell({ r, c })}
                    onDoubleClick={() => {
                      setActiveCell({ r, c });
                      if (readOnly) return;
                      // Same loaded-value guard as Enter / F2.
                      if (v === null || v === undefined) return;
                      setEditing({ r, c });
                      setEditVal(typeof v === 'number' ? String(v) : '');
                    }}
                  >
                    {isEditing ? (
                      <input
                        ref={inputRef}
                        value={editVal}
                        onChange={(e) => setEditVal(e.target.value)}
                        onBlur={commitEdit}
                        className="ve-cell-input"
                      />
                    ) : (
                      <span className={typeof v !== 'number' ? 've-string' : ''}>{format(v)}</span>
                    )}
                  </td>
                );
              })}
              {rightPadW > 0 && <td aria-hidden="true" style={{ minWidth: rightPadW, padding: 0, border: 'none' }} />}
            </tr>
          ))}
          {bottomPadH > 0 && (
            <tr aria-hidden="true" style={{ height: bottomPadH }}>
              <td colSpan={visibleCols.length + 3} style={{ padding: 0, border: 'none' }} />
            </tr>
          )}
        </tbody>
      </table>
    </div>
  );
}

/* ======================================================================== */
/* MatrixPanel — the polished matrix viewer/editor body                     */
/* ======================================================================== */
// Presentation + display-state only. The data layer (how rows/cols/values
// are fetched) is supplied by the caller via props, so the SAME panel
// serves top-level matrix variables (name-addressed fetch in
// VariableEditor) and drilled struct/cell matrix fields (inline data in
// StructInspector). No toolbar duplication.
//
// Props:
//   rows, cols, name, type
//   getCellValue(r,c)  — raw value
//   getSlice(axis,idx) — for the inline plot
//   stats              — { min, max, mean, n } | null   (heatmap + status)
//   readOnly           — disables cell editing
//   onCommit(r,c,rhs)  — owner performs the write + refresh; rhs is a
//                        ready-to-interpolate MATLAB literal (number now;
//                        type-aware string in Phase B)
//   onEscape()         — Esc when not editing (close modal / pop breadcrumb)
//   onSave(format)     — save-as handler; null → no save-as button
//   saveDisabled       — gray out save-as (e.g. tile-mode huge matrix)
//   fontScale
export function MatrixPanel({
  rows, cols, name, type,
  getCellValue, getSlice, stats,
  dims, page = 0, setPage,
  readOnly = false,
  onCommit, onEscape, onSave,
  saveDisabled = false,
}) {
  const [precision, setPrecision] = useState(4);
  const [notation, setNotation]   = useState('fixed');
  const [heatmap, setHeatmap]     = useState(false);
  const [statsVisible, setStatsVisible] = useStatChooser();
  const [showPlot, setShowPlot]   = useState(false);
  const [saveOpen, setSaveOpen]   = useState(false);
  const [plotWidth, setPlotWidth] = useState(() => {
    const stored = parseInt(localStorage.getItem('numkit.ve.plotWidth') || '', 10);
    return Number.isFinite(stored) && stored >= 200 && stored <= 1600 ? stored : 520;
  });
  useEffect(() => { localStorage.setItem('numkit.ve.plotWidth', String(plotWidth)); }, [plotWidth]);

  const [activeCell, setActiveCell] = useState({ r: 0, c: 0 });
  const [editing, setEditing]       = useState(null);
  const [editVal, setEditVal]       = useState('');
  const veBodyRef = useRef(null);
  const tableRef  = useRef(null);
  const inputRef  = useRef(null);

  // Reset the active cell when the data source changes shape (e.g. the
  // inspector drilled into a different field).
  useEffect(() => { setActiveCell({ r: 0, c: 0 }); setEditing(null); }, [rows, cols, name]);

  function startDragDivider(e) {
    e.preventDefault();
    const body = veBodyRef.current;
    if (!body) return;
    const onMove = (ev) => {
      const rect = body.getBoundingClientRect();
      const desired = rect.right - ev.clientX;
      const maxW = Math.max(220, rect.width - 200);
      setPlotWidth(Math.max(200, Math.min(maxW, desired)));
    };
    const onUp = () => {
      window.removeEventListener('mousemove', onMove);
      window.removeEventListener('mouseup', onUp);
      document.body.style.cursor = '';
      document.body.style.userSelect = '';
    };
    window.addEventListener('mousemove', onMove);
    window.addEventListener('mouseup', onUp);
    document.body.style.cursor = 'col-resize';
    document.body.style.userSelect = 'none';
  }

  function formatNum(n) {
    if (!Number.isFinite(n)) return String(n);
    if (notation === 'exp')   return n.toExponential(precision);
    if (notation === 'fixed') return n.toFixed(precision);
    return fmt(n, { fix: precision, exp: precision });
  }
  const COMPLEX_RE = /^\s*(-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)\s*([+-])\s*(\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)i\s*$/;
  function format(v) {
    if (v === null || v === undefined) return '—';
    if (typeof v === 'number') return formatNum(v);
    if (typeof v === 'string') {
      const m = v.match(COMPLEX_RE);
      if (m) {
        const re = parseFloat(m[1]);
        const im = parseFloat(m[3]) * (m[2] === '-' ? -1 : 1);
        return `${formatNum(re)}${im >= 0 ? '+' : '-'}${formatNum(Math.abs(im))}i`;
      }
    }
    return String(v);
  }

  // Collect the edit, hand a type-aware MATLAB literal + JS mirror value
  // to the owner. valueToMatlabRHS escapes/validates per type (numeric /
  // logical / char / string); null → invalid input, abort the edit.
  const commitEdit = useCallback(() => {
    if (!editing) return;
    const out = valueToMatlabRHS(editVal, type);
    if (!out) { setEditing(null); return; }
    onCommit?.(editing.r, editing.c, out.rhs, out.value);
    setEditing(null);
  }, [editing, editVal, onCommit, type]);

  useEffect(() => {
    if (editing && inputRef.current) { inputRef.current.focus(); inputRef.current.select(); }
  }, [editing]);

  const handleKey = useCallback((e) => {
    if (editing) {
      if (e.key === 'Enter')  { e.preventDefault(); commitEdit(); }
      if (e.key === 'Escape') { e.preventDefault(); setEditing(null); }
      return;
    }
    if (e.key === 'Escape') { onEscape?.(); return; }
    if (!readOnly && (e.key === 'Enter' || e.key === 'F2')) {
      const v = getCellValue(activeCell.r, activeCell.c);
      if (v === null || v === undefined) return;
      setEditing({ ...activeCell });
      setEditVal(typeof v === 'number' ? String(v) : '');
      e.preventDefault();
      return;
    }
    let { r, c } = activeCell;
    if (e.key === 'ArrowUp')    r = Math.max(0, r - 1);
    if (e.key === 'ArrowDown')  r = Math.min(rows - 1, r + 1);
    if (e.key === 'ArrowLeft')  c = Math.max(0, c - 1);
    if (e.key === 'ArrowRight') c = Math.min(cols - 1, c + 1);
    if (e.key === 'Home')       c = 0;
    if (e.key === 'End')        c = cols - 1;
    if (e.key === 'PageUp')     r = Math.max(0, r - 10);
    if (e.key === 'PageDown')   r = Math.min(rows - 1, r + 10);
    if (r !== activeCell.r || c !== activeCell.c) { setActiveCell({ r, c }); e.preventDefault(); }
  }, [activeCell, rows, cols, editing, getCellValue, onEscape, readOnly, commitEdit]);

  useEffect(() => {
    window.addEventListener('keydown', handleKey);
    return () => window.removeEventListener('keydown', handleKey);
  }, [handleKey]);

  return (
    <>
      <div className="ve-toolbar">
        {dims && pageCount(dims) > 1 && (
          <div className="ve-tools-group ve-slice-nav">
            <span className="ve-label">slice</span>
            <span className="ve-slice-expr">{name}(:,:,</span>
            {dims.slice(2).map((dlen, j) => {
              const subs = pageToSubs(page, dims);
              const k = subs[j] || 0;
              const setK = (nk) => {
                const s = pageToSubs(page, dims);
                s[j] = Math.min(dlen - 1, Math.max(0, nk));
                setPage?.(subsToPage(s, dims));
              };
              return (
                <span key={j} className="ve-slice-dim">
                  {j > 0 && <span className="ve-slice-comma">,</span>}
                  <button className="ve-slice-arrow" disabled={k <= 0}
                    onClick={() => setK(k - 1)} title="Previous slice">‹</button>
                  <input className="ve-slice-input" type="number" min={1} max={dlen}
                    value={k + 1}
                    onChange={(e) => { const nv = parseInt(e.target.value, 10);
                                       if (Number.isFinite(nv)) setK(nv - 1); }} />
                  <span className="ve-slice-of">/{dlen}</span>
                  <button className="ve-slice-arrow" disabled={k >= dlen - 1}
                    onClick={() => setK(k + 1)} title="Next slice">›</button>
                </span>
              );
            })}
            <span className="ve-slice-expr">)</span>
          </div>
        )}
        <div className="ve-tools-group">
          <span className="ve-label">notation</span>
          <div className="ve-segmented">
            {['fixed', 'exp', 'auto'].map((n) => (
              <button key={n} className={notation === n ? 'is-active' : ''} onClick={() => setNotation(n)}>{n}</button>
            ))}
          </div>
        </div>
        <div className="ve-tools-group">
          <span className="ve-label">precision</span>
          <input type="range" min={0} max={8} step={1}
            value={precision} onChange={(e) => setPrecision(parseInt(e.target.value, 10))} />
          <span className="ve-precision-num">{precision}</span>
        </div>
        <div className="ve-tools-group">
          {stats && <StatChooserButton visible={statsVisible} setVisible={setStatsVisible} />}
          <button className={`ve-btn ${heatmap ? 'is-active' : ''}`}
            onClick={() => setHeatmap((h) => !h)} title="Toggle value heatmap">
            <svg width="11" height="11" viewBox="0 0 12 12">
              <rect x="0.5" y="0.5" width="3" height="11" fill="oklch(0.6 0.1 240)"/>
              <rect x="3.5" y="0.5" width="3" height="11" fill="oklch(0.6 0.1 180)"/>
              <rect x="6.5" y="0.5" width="3" height="11" fill="oklch(0.6 0.1 60)"/>
              <rect x="9.5" y="0.5" width="2" height="11" fill="oklch(0.6 0.1 30)"/>
            </svg>
            heatmap
          </button>
          <button className={`ve-btn ${showPlot ? 'is-active' : ''}`}
            title="Toggle inline plot" onClick={() => setShowPlot((p) => !p)}>
            <svg width="11" height="11" viewBox="0 0 12 12">
              <polyline points="1,9 4,5 7,7 11,2" stroke="currentColor" fill="none" strokeWidth="1.4"/>
            </svg>
            plot
          </button>
          <button className="ve-btn" title="Copy as CSV" onClick={() => {
            const lines = [];
            for (let r = 0; r < rows; r++) {
              const row = [];
              for (let c = 0; c < cols; c++) {
                const v = getCellValue(r, c);
                row.push(typeof v === 'number' ? v : `"${v ?? ''}"`);
              }
              lines.push(row.join(','));
            }
            navigator.clipboard?.writeText(lines.join('\n'));
          }}>
            <svg width="11" height="11" viewBox="0 0 12 12">
              <rect x="2" y="2" width="7" height="8" rx="1" stroke="currentColor" fill="none"/>
              <rect x="3.5" y="0.5" width="7" height="8" rx="1" stroke="currentColor" fill="none"/>
            </svg>
            copy csv
          </button>
          {onSave && (
            <div className="ve-saveas-wrap">
              <button className="ve-btn ve-saveas-trigger"
                title={saveDisabled ? 'save disabled for huge matrices' : 'Save variable to file'}
                disabled={saveDisabled}
                onClick={() => setSaveOpen((s) => !s)}>
                <svg width="11" height="11" viewBox="0 0 12 12">
                  <path d="M2 2h6l2 2v6H2z M4 2v3h4V2 M4 8h4v2H4z" stroke="currentColor" fill="none"/>
                </svg>
                save as
                <svg width="8" height="8" viewBox="0 0 8 8" style={{ marginLeft: 2 }}>
                  <path d="M1 2.5 L4 5.5 L7 2.5" stroke="currentColor" fill="none" strokeWidth="1.2" strokeLinecap="round"/>
                </svg>
              </button>
              {saveOpen && (
                <SaveAsMenu onClose={() => setSaveOpen(false)}
                  onPick={(f) => { onSave(f); setSaveOpen(false); }} />
              )}
            </div>
          )}
        </div>
        <div className="ve-tools-spacer" />
      </div>

      {/* Aggregate statistics over the whole matrix, on their own row. The
          chooser (Σ ▾) lives in the toolbar above; this row collapses to
          nothing for non-numeric values OR when no statistic is selected. */}
      <StatsBar stats={stats} visible={statsVisible} />

      <div className="ve-address">
        <span className="ve-cell-ref">{name}({activeCell.r + 1}, {activeCell.c + 1}{dims && dims.length > 2 ? pageToSubs(page, dims).map((k) => `, ${k + 1}`).join('') : ''})</span>
        <span className="ve-eq">=</span>
        <span className="ve-cell-val">{format(getCellValue(activeCell.r, activeCell.c))}</span>
      </div>

      <div className={`ve-body ${showPlot ? 'has-plot' : ''}`}
           ref={veBodyRef}
           style={showPlot ? { gridTemplateColumns: `1fr 6px ${plotWidth}px` } : undefined}>
        <VirtualTable
          tableRef={tableRef}
          rows={rows} cols={cols}
          getCellValue={getCellValue}
          activeCell={activeCell} setActiveCell={setActiveCell}
          editing={editing} setEditing={setEditing}
          editVal={editVal} setEditVal={setEditVal}
          commitEdit={commitEdit}
          inputRef={inputRef}
          heatmap={heatmap} stats={stats}
          format={format}
          readOnly={readOnly}
        />
        {showPlot && (
          <>
            <div className="ve-divider" role="separator" aria-orientation="vertical"
              aria-label="Resize plot pane"
              onMouseDown={startDragDivider}
              onDoubleClick={() => setPlotWidth(520)}
              title="Drag to resize · double-click to reset" />
            <InlinePlot getSlice={getSlice} rows={rows} cols={cols}
              onClose={() => setShowPlot(false)} />
          </>
        )}
      </div>

      <div className="ve-status">
        <span>cell ({activeCell.r + 1},{activeCell.c + 1}{dims && dims.length > 2 ? pageToSubs(page, dims).map((k) => `,${k + 1}`).join('') : ''})</span>
        <span className="ve-sep" />
        <span>↑↓←→ navigate</span>
        <span className="ve-sep" />
        <span>{readOnly ? 'read-only' : '↵ / F2 edit'}</span>
        <span className="ve-sep" />
        <span>Esc {onEscape ? 'back' : 'close'}</span>
        <span className="ve-spacer" />
        <span>UTF-8</span>
        <span className="ve-sep" />
        <span>{readOnly ? 'read-only' : 'read/write'}</span>
      </div>
    </>
  );
}

/* ======================================================================== */
/* Variable Editor — modal table with notation/precision/heatmap/plot       */
/* ======================================================================== */
// Switch to tile-mode for matrices with more cells than this. A 500×500
// matrix is the rough boundary where full-fetch JSON becomes expensive
// (~250k values, several MB of JSON, sluggish parsing); above it we
// only fetch what's visible.
