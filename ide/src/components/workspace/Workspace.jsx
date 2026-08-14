import { useState, useMemo, useEffect, useRef, useCallback } from 'react';
import { useTheme } from '../../theme';
import { aggregateStats, loadVisibleColumns, saveVisibleColumns } from './valueColumns';
import { pageToSubs } from './sliceNav';
import { useChooser } from './chooser';
import { EntityBrowser, KIND_META, TONE, pickTone, WS_VIEW_KEY, WS_SORT_KEY } from './EntityBrowser';
import { MatrixPanel, TILE, TILE_MODE_THRESHOLD } from './MatrixPanel';
import { StructInspector } from './StructInspector';

// Re-exported so existing importers (IDE, render tests) keep their
// `{ MatrixPanel } from './Workspace'` imports working after the viewer
// clusters moved to their own files.
export { MatrixPanel };

/* ======================================================================== */
/* Type metadata + tone palette                                             */
/* ======================================================================== */

export function WorkspacePanel({ variables, onOpen }) {
  // Column visibility — shared key with the struct inspector's table.
  const [cols, setCols] = useChooser('numkit.ide.valuecols', loadVisibleColumns, saveVisibleColumns);
  const byName = useMemo(() => {
    const m = new Map();
    for (const v of variables) m.set(v.name, v);
    return m;
  }, [variables]);
  const rows = useMemo(() => variables.map((v) => ({
    key: v.name, name: v.name, value: v.preview, size: v.size,
    klass: v.type, kind: v.kind, bytes: v.bytes, stats: v.stats || null, drill: true,
  })), [variables]);

  return (
    <div className="workspace">
      <EntityBrowser
        rows={rows}
        nameHeader="Name"
        countNoun="variable"
        viewKey={WS_VIEW_KEY} sortKey={WS_SORT_KEY}
        cols={cols} setCols={setCols}
        onOpen={(row) => { const v = byName.get(row.name); if (v) onOpen(v); }}
        footer={(
          <div className="ws-hint">
            <kbd>click</kbd> open · <kbd>Esc</kbd> close editor
          </div>
        )}
      />
    </div>
  );
}

/* ======================================================================== */
/* Save-as menu (used inside Variable Editor)                               */
/* ======================================================================== */
function downloadBlob(filename, blob) {
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url; a.download = filename;
  document.body.appendChild(a); a.click();
  setTimeout(() => { URL.revokeObjectURL(url); a.remove(); }, 100);
}

function exportData(variable, data, format) {
  const name = variable.name;
  if (format === 'csv') {
    const csv = data.map((row) => row.map((v) => typeof v === 'number' ? v : `"${v}"`).join(',')).join('\n');
    downloadBlob(`${name}.csv`, new Blob([csv], { type: 'text/csv' }));
  } else if (format === 'tsv') {
    const tsv = data.map((row) => row.map((v) => typeof v === 'number' ? v : `"${v}"`).join('\t')).join('\n');
    downloadBlob(`${name}.tsv`, new Blob([tsv], { type: 'text/tab-separated-values' }));
  } else if (format === 'json') {
    const obj = { name, size: variable.size, type: variable.type, data };
    downloadBlob(`${name}.json`, new Blob([JSON.stringify(obj, null, 2)], { type: 'application/json' }));
  } else if (format === 'mat') {
    const text = `% Numkit save\n${name} = [\n${data.map((r) => '  ' + r.join(' ')).join(';\n')}\n];\n`;
    downloadBlob(`${name}.n`, new Blob([text], { type: 'text/plain' }));
  } else if (format === 'npy') {
    const rows = data.length, cols = data[0]?.length || 0;
    const flat = new Float64Array(rows * cols);
    for (let i = 0; i < rows; i++) for (let j = 0; j < cols; j++) flat[i * cols + j] = +data[i][j] || 0;
    const header = `{'descr': '<f8', 'fortran_order': False, 'shape': (${rows}, ${cols}), }`;
    const padded = header + ' '.repeat(63 - ((10 + header.length) % 64)) + '\n';
    const headerBytes = new TextEncoder().encode(padded);
    const magic = new Uint8Array([0x93, 0x4E, 0x55, 0x4D, 0x50, 0x59, 1, 0]);
    const lenBytes = new Uint8Array(2);
    new DataView(lenBytes.buffer).setUint16(0, headerBytes.length, true);
    const buf = new Uint8Array(magic.length + lenBytes.length + headerBytes.length + flat.byteLength);
    buf.set(magic, 0);
    buf.set(lenBytes, magic.length);
    buf.set(headerBytes, magic.length + lenBytes.length);
    buf.set(new Uint8Array(flat.buffer), magic.length + lenBytes.length + headerBytes.length);
    downloadBlob(`${name}.npy`, new Blob([buf], { type: 'application/octet-stream' }));
  }
}



export function VariableEditor({ variable, onClose, engine }) {
  // Struct / cell variables get the drill-in inspector instead of the
  // numeric table — the matrix toolbar (notation / precision / heatmap
  // / plot / cell-address) doesn't apply to them. Gated on kind, which
  // adapters.classify() now reports as 'struct' / 'cell'. StructInspector
  // owns its own path state + engine fetch.
  const isStructLike = variable.kind === 'struct' || variable.kind === 'cell';

  // Display state (notation / precision / heatmap / plot / activeCell /
  // editing) lives in MatrixPanel now. VariableEditor keeps only the
  // window chrome + the name-addressed DATA layer below.
  const [maximized, setMaximized] = useState(false);

  // dimensions: { rows, cols, tileMode } — populated from variable.size & getVarShape
  const initialShape = (() => {
    let r = variable.data?.length || 1;
    let c = variable.data?.[0]?.length || 1;
    if (variable.size && typeof variable.size === 'string') {
      const m = variable.size.match(/(\d+)\s*[x×]\s*(\d+)/);
      if (m) {
        r = parseInt(m[1], 10) || r;
        c = parseInt(m[2], 10) || c;
      }
    }
    const tileMode = (r * c > TILE_MODE_THRESHOLD);
    return { rows: r, cols: c, tileMode };
  })();
  const [shape, setShape] = useState(initialShape);
  // Linear page index (0-based) of the displayed 2-D slice for 3-D / N-D
  // arrays (0 for 2-D). loadedPageRef tracks which page's full-mode data is
  // currently in `data`, so a page change refetches but mount / variable
  // swap don't double-fetch page 0.
  const [page, setPage] = useState(0);
  const loadedPageRef = useRef(0);
  // Full-mode data (small matrices). For tile-mode we don't use this.
  const [data, setData]           = useState(variable.data);
  // Tile cache + pending set (tile-mode only). Map<"tR,tC", number[][] | 'pending' | 'error'>
  const tileCache = useRef(new Map());
  const [, setTileBump] = useState(0);  // bump to re-render after tile arrives
  const [loading, setLoading]     = useState(false);
  const [loadError, setLoadError] = useState(null);

  // Struct / cell fetching is owned by StructInspector (it manages its
  // own path state + engine.inspectPath calls). VariableEditor just
  // skips the matrix fetch below when isStructLike.

  // On open (and on variable swap), pick a fetch strategy:
  //   - small matrix → full fetch (existing path) — responsive precision/heatmap
  //   - huge matrix  → tile mode — only the viewport is read from the engine
  useEffect(() => {
    if (isStructLike) return;   // struct path handled by the effect above
    setData(variable.data);
    setPage(0);                 // new variable → back to slice 1
    loadedPageRef.current = 0;
    // activeCell now lives in MatrixPanel (it resets itself on a
    // rows/cols/name change), so VariableEditor no longer touches it.
    setLoadError(null);
    tileCache.current = new Map();
    sliceCache.current = new Map();
    if (!engine || typeof engine.getVarData !== 'function') {
      return;
    }
    let active = true;
    setLoading(true);
    (async () => {
      try {
        // Cheap dimension probe first.
        const sh = (typeof engine.getVarShape === 'function')
          ? await engine.getVarShape(variable.name)   // may be Promise (native) or value (WASM)
          : null;
        if (!active) return;
        if (sh && !sh.error) {
          const numel = sh.rows * sh.cols;
          const tileMode = numel > TILE_MODE_THRESHOLD
                        && typeof engine.getVarTile === 'function';
          setShape({ rows: sh.rows, cols: sh.cols, tileMode,
                     pages: sh.pages ?? 1, ndim: sh.ndim ?? 2, dims: sh.dims });
          if (tileMode) {
            // Don't full-fetch. The virtual table will request tiles on demand.
            setLoading(false);
            return;
          }
        }
        // Small enough: full fetch.
        const r = await engine.getVarData(variable.name);   // may be Promise (native) or value (WASM)
        if (!active) return;
        if (!r) { setLoading(false); return; }
        if (r.error) { setLoadError(r.error); setLoading(false); return; }
        if (Array.isArray(r.data) && r.data.length > 0) {
          setData(r.data);
          setShape({
            rows: r.rows ?? r.data.length,
            cols: r.cols ?? (r.data[0]?.length || 0),
            tileMode: false,
            pages: sh?.pages ?? 1, ndim: sh?.ndim ?? 2, dims: sh?.dims,
          });
        }
        setLoading(false);
      } catch (e) {
        if (active) {
          setLoadError(e?.message || String(e));
          setLoading(false);
        }
      }
    })();
    return () => { active = false; };
  }, [variable.name, engine, isStructLike]);

  // Page change → refetch the full-mode slice. Tile-mode pages are keyed by
  // `page` in the tile / slice caches below, so they refetch lazily without
  // an effect. loadedPageRef guards the mount / variable-swap double (the
  // main effect already loaded slice 0).
  useEffect(() => {
    if (isStructLike || shape.tileMode) return;
    if (page === loadedPageRef.current) return;
    loadedPageRef.current = page;
    if (!engine || typeof engine.getVarPage !== 'function') return;
    (async () => {
      try {
        const r = await engine.getVarPage(variable.name, page);   // may be Promise (native)
        if (r && !r.error && Array.isArray(r.data)) setData(r.data);
      } catch (e) {
        setLoadError(e?.message || String(e));
      }
    })();
  }, [page, variable.name, engine, shape.tileMode, isStructLike]);

  /* ─── slice cache (for InlinePlot in both modes) ─── */
  // Keyed by `${axis}:${idx}`. In tile-mode each miss triggers a single
  // 10000×1 (or 1×10000) tile fetch — fast because numkit storage is
  // column-major, so a column slice is a single contiguous read.
  const sliceCache = useRef(new Map());
  const getSlice = useCallback((axis, idx) => {
    if (!shape.tileMode) {
      return axis === 'row' ? (data[idx] || []) : data.map((r) => r[idx]);
    }
    const key = `${page}:${axis}:${idx}`;
    const cached = sliceCache.current.get(key);
    if (cached && cached !== 'pending' && cached !== 'error') return cached;
    if (cached === undefined) {
      sliceCache.current.set(key, 'pending');
      Promise.resolve().then(async () => {
        try {
          let res;
          if (axis === 'row') {
            res = await engine.getVarTile(variable.name, idx, 0, 1, Math.min(shape.cols, 10000), page);
          } else {
            res = await engine.getVarTile(variable.name, 0, idx, Math.min(shape.rows, 10000), 1, page);
          }
          if (!res || res.error || !Array.isArray(res.data)) {
            sliceCache.current.set(key, 'error');
          } else {
            const out = (axis === 'row')
              ? (res.data[0] || [])
              : res.data.map((r) => r[0]);
            sliceCache.current.set(key, out);
          }
        } catch {
          sliceCache.current.set(key, 'error');
        }
        setTileBump((n) => n + 1);
      });
    }
    return [];
  }, [shape.tileMode, shape.rows, shape.cols, data, engine, variable.name, page]);

  /* ─── tile-mode cell accessor ─── */
  // Returns the value at (r, c). For full-mode this is just data[r][c].
  // For tile-mode it consults the tile cache, kicking off a fetch if the
  // tile is missing. While the tile is in flight we return null and the
  // cell renders "—".
  const getCellValue = useCallback((r, c) => {
    if (!shape.tileMode) return data[r]?.[c];
    const tR = Math.floor(r / TILE);
    const tC = Math.floor(c / TILE);
    const key = `${page},${tR},${tC}`;
    const tile = tileCache.current.get(key);
    if (tile && tile !== 'pending' && tile !== 'error') {
      return tile[r - tR * TILE]?.[c - tC * TILE];
    }
    if (tile === undefined) {
      // First time we ask for this tile — kick off fetch. Mark pending so
      // we don't re-trigger on every cell render.
      tileCache.current.set(key, 'pending');
      const r0 = tR * TILE, c0 = tC * TILE;
      // Defer to a microtask so React's render pass isn't blocked.
      // `await` handles both WASM (sync value) and native (Promise).
      Promise.resolve().then(async () => {
        try {
          const res = await engine.getVarTile(variable.name, r0, c0, TILE, TILE, page);
          if (!res || res.error || !Array.isArray(res.data)) {
            tileCache.current.set(key, 'error');
          } else {
            tileCache.current.set(key, res.data);
          }
        } catch {
          tileCache.current.set(key, 'error');
        }
        // Bump state to trigger re-render.
        setTileBump((n) => n + 1);
      });
    }
    return null;
  }, [shape.tileMode, data, engine, variable.name, page]);

  // Dimensions come from `shape` (set on open via getVarShape) so tile-mode
  // matrices size their grid correctly even before any tile arrives.
  const rows = shape.rows;
  const cols = shape.cols;

  // Tile-mode stats are computed natively in the engine via getVarStats.
  // Stored in state so the heatmap can light up as soon as the result
  // arrives (typically <100 ms even for 100M cells).
  const [tileStats, setTileStats] = useState(null);
  useEffect(() => {
    if (!shape.tileMode || !engine || typeof engine.getVarStats !== 'function') {
      setTileStats(null);
      return;
    }
    let cancelled = false;
    setTimeout(async () => {
      const s = await engine.getVarStats(variable.name, page);   // may be Promise (native)
      if (cancelled) return;
      if (s && !s.error) setTileStats(s);
    }, 0);
    return () => { cancelled = true; };
  }, [shape.tileMode, engine, variable.name, page]);

  // Full-mode stats: the complete set over the in-memory data (same helper
  // as the drilled-matrix path). Tile-mode (huge matrices) uses the
  // engine's getVarStats, which now returns the full set too.
  const stats = useMemo(() => {
    if (shape.tileMode) return tileStats;
    return aggregateStats(data.flat());
  }, [data, shape.tileMode, tileStats]);

  // Write-back for a committed cell edit. MatrixPanel hands us (r, c,
  // rhs) where rhs is a ready-to-interpolate MATLAB literal. The engine's
  // parser handles 1-based indexing, type coercion, and persistence; we
  // then invalidate the affected cache so the cell repaints fresh.
  const onCommit = useCallback((r, c, rhs, value) => {
    if (engine && typeof engine.execute === 'function') {
      try {
        // Page subscripts (k3,k4,…) so the edit lands in the displayed slice
        // of a 3-D / N-D array, not slice 1.
        const subs = pageToSubs(page, shape.dims || []);
        const idx = [r + 1, c + 1, ...subs.map((k) => k + 1)].join(', ');
        engine.execute(`${variable.name}(${idx}) = ${rhs};`);
      } catch (e) {
        console.warn('[VariableEditor] write-back failed:', e);
      }
    }
    if (shape.tileMode) {
      const tR = Math.floor(r / TILE), tC = Math.floor(c / TILE);
      tileCache.current.delete(`${page},${tR},${tC}`);
      sliceCache.current.delete(`${page}:col:${c}`);
      sliceCache.current.delete(`${page}:row:${r}`);
      setTileBump((n) => n + 1);
    } else {
      // Full mode: mirror the JS value locally so the cell repaints
      // immediately without a refetch.
      setData((d) => {
        const copy = d.map((row) => row.slice());
        if (copy[r]) copy[r][c] = value;
        return copy;
      });
    }
  }, [engine, variable.name, shape.tileMode, shape.dims, page]);

  const { themeName: veThemeName } = useTheme();
  const meta = KIND_META[variable.kind] || KIND_META.matrix;
  const tone = pickTone(TONE[meta.tone] || TONE.amber, veThemeName);

  // Shared title-right (maximise / close) — identical for the matrix
  // and struct layouts, so it lives in one const.
  const titleButtons = (
    <div className="ve-title-right">
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
  );

  // ── Struct / cell layout — drill-in inspector ──
  if (isStructLike) {
    return (
      <div className="ve-overlay" onClick={(e) => { if (e.target === e.currentTarget) onClose(); }}>
        <div className={`ve-window ve-window-struct ${maximized ? 'is-max' : ''}`}
          role="dialog" aria-label={`Variable Editor: ${variable.name}`}>
          <div className="ve-titlebar">
            <div className="ve-title-left">
              <span className="ve-tag" style={{ color: tone.fg, background: tone.bg, borderColor: tone.border }}>
                {meta.glyph} {variable.type}
              </span>
              <span className="ve-name">{variable.name}</span>
              <span className="ve-dim">{variable.size}</span>
              <span className="ve-meta" title={`${variable.bytes} B`}>
                {variable.bytes} B · {variable.size}
              </span>
            </div>
            {titleButtons}
          </div>
          <StructInspector key={variable.name} variable={variable} engine={engine} onEscape={onClose} />
        </div>
      </div>
    );
  }

  return (
    <div className="ve-overlay" onClick={(e) => { if (e.target === e.currentTarget) onClose(); }}>
      <div className={`ve-window ${maximized ? 'is-max' : ''}`}
        role="dialog" aria-label={`Variable Editor: ${variable.name}`}>
        <div className="ve-titlebar">
          <div className="ve-title-left">
            <span className="ve-tag" style={{ color: tone.fg, background: tone.bg, borderColor: tone.border }}>
              {meta.glyph} {variable.type}
            </span>
            <span className="ve-name">{variable.name}</span>
            <span className="ve-dim">{variable.size}</span>
            <span className="ve-meta" title={`${variable.bytes} B · ${rows * cols} elements`}>
              {variable.bytes} B · {rows * cols} elements
            </span>
            {loading && (
              <span className="ve-meta" style={{ color: 'var(--accent)' }}>
                loading…
              </span>
            )}
            {loadError && (
              <span className="ve-meta" style={{ color: 'var(--danger)' }}
                title={loadError}>
                preview only
              </span>
            )}
          </div>
          {titleButtons}
        </div>

        <MatrixPanel
          engine={engine}
          rows={rows} cols={cols} name={variable.name} type={variable.type}
          getCellValue={getCellValue} getSlice={getSlice} stats={stats}
          dims={shape.dims} page={page} setPage={setPage}
          readOnly={false}
          onCommit={onCommit}
          onEscape={onClose}
          onSave={shape.tileMode ? null : (f) => exportData(variable, data, f)}
          saveDisabled={shape.tileMode}
        />
      </div>
    </div>
  );
}
