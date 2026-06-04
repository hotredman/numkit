// EntityBrowser.jsx — the unified tabular browser (cards / list view with
// filter · sort · column chooser) shared by the Workspace panel and the
// struct/cell inspector's field list, plus EntityCard and the type-metadata
// / tone palette it renders with.
import { useState, useMemo, useEffect } from 'react';
import { useTheme } from '../../theme';
import ValueTable from './ValueTable';
import { VALUE_COLUMNS } from './valueColumns';
import { ChooserButton } from './chooser';

export const KIND_META = {
  scalar: { label: 'scalar', glyph: '∙', tone: 'cyan'   },
  vector: { label: 'vector', glyph: '⟶', tone: 'green'  },
  matrix: { label: 'matrix', glyph: '▦', tone: 'amber'  },
  string: { label: 'string', glyph: '"', tone: 'violet' },
  struct: { label: 'struct', glyph: '⊞', tone: 'pink'   },
  cell:   { label: 'cell',   glyph: '{}', tone: 'pink'   },
};

export const TONE = {
  cyan:   { fg: '#6ec3b8', bg: 'rgba(124,224,211,0.07)', border: 'rgba(124,224,211,0.20)',
            fgL: '#0969da', bgL: '#ddf4ff', borderL: '#54aeff66' },
  green:  { fg: '#6fc28a', bg: 'rgba(127,217,154,0.07)', border: 'rgba(127,217,154,0.20)',
            fgL: '#1a7f37', bgL: '#dafbe1', borderL: '#4ac26b66' },
  amber:  { fg: '#d0a360', bg: 'rgba(233,184,112,0.07)', border: 'rgba(233,184,112,0.20)',
            fgL: '#9a6700', bgL: '#fff8c5', borderL: '#d4a72c66' },
  violet: { fg: '#9d89db', bg: 'rgba(182,156,242,0.07)', border: 'rgba(182,156,242,0.20)',
            fgL: '#6639ba', bgL: '#fbefff', borderL: '#c297ff66' },
  pink:   { fg: '#d877b8', bg: 'rgba(224,112,192,0.07)', border: 'rgba(224,112,192,0.20)',
            fgL: '#a040a0', bgL: '#ffeffb', borderL: '#e070c066' },
};

export function pickTone(t, themeName) {
  const isLight = themeName === 'light';
  return isLight
    ? { fg: t.fgL, bg: t.bgL, border: t.borderL }
    : { fg: t.fg,  bg: t.bg,  border: t.border  };
}


/* ======================================================================== */
/* Card / row                                                               */
/* ======================================================================== */
//
// One mouse click opens the Variable Editor — the same gesture that opens
// a figure card in the Figures pane. There's no persistent selection
// state on purpose: a "selected" workspace variable used to swallow Enter
// keystrokes meant for the editor / console (the pane held a window-level
// keydown listener), so a click in the workspace would silently break
// newline insertion elsewhere. Hover highlight only.

// One card for both contexts — a workspace variable or a struct field.
// `row` is the normalized shape EntityBrowser uses:
//   { key, name, value, size, klass, kind, stats, drill }
function EntityCard({ row, nameCell, onOpen, onContextMenu }) {
  const { themeName } = useTheme();
  const meta = KIND_META[row.kind] || KIND_META.matrix;
  const tone = pickTone(TONE[meta.tone] || TONE.amber, themeName);
  return (
    <div
      className="var-card"
      onClick={row.drill !== false ? () => onOpen?.(row) : undefined}
      onContextMenu={onContextMenu ? (e) => { e.preventDefault(); e.stopPropagation(); onContextMenu(row, e); } : undefined}
      role="button"
      aria-label={`Open ${row.name}`}
    >
      <div className="var-card-head">
        <span className="var-name">{nameCell ? nameCell(row) : row.name}</span>
        <span className="var-size">{row.size}</span>
        <span className="var-type-pill" style={{ color: tone.fg, background: tone.bg, borderColor: tone.border }}>
          <span className="var-glyph">{meta.glyph}</span>{row.klass}
        </span>
      </div>
      <div className="var-card-body">
        <span className="var-preview">{row.value}</span>
      </div>
    </div>
  );
}

/* ======================================================================== */
/* Workspace toolbar                                                        */
/* ======================================================================== */
// Numeric size for sorting: bytes when known (variables), else element
// count parsed from the "R×C" string (struct fields).
function sizeMetric(row) {
  if (Number.isFinite(row.bytes)) return row.bytes;
  const m = String(row.size || '').match(/(\d+)\s*[x×]\s*(\d+)/);
  return m ? (+m[1]) * (+m[2]) : 0;
}

const VIEW_OPTS = ['cards', 'list'];
const SORT_OPTS = ['name', 'size', 'type'];

// Unified tabular browser — the one widget behind BOTH the Workspace
// panel and the struct/cell inspector's field list. Toolbar (filter ·
// sort · Σ▾ column chooser · cards/list toggle) over a cards grid or a
// ValueTable. The caller supplies normalized rows + open / context-menu
// handlers + an optional footer (e.g. the struct "+ new field" row);
// view & sort persist per `viewKey`/`sortKey`, columns via the shared
// `cols`/`setCols` chooser state. Filter & sort are display-only.
export function EntityBrowser({
  rows, nameHeader = 'Name', countNoun = 'item', defaultView = 'cards',
  viewKey, sortKey, cols, setCols,
  nameCell, onOpen, onRowContextMenu, onAreaContextMenu, footer,
}) {
  const [query, setQuery] = useState('');
  const [sort, setSort] = useState(() => loadPref(sortKey, SORT_OPTS, 'name'));
  const [view, setView] = useState(() => loadPref(viewKey, VIEW_OPTS, defaultView));
  useEffect(() => { try { localStorage.setItem(viewKey, view); } catch { /* ignore */ } }, [viewKey, view]);
  useEffect(() => { try { localStorage.setItem(sortKey, sort); } catch { /* ignore */ } }, [sortKey, sort]);

  const filtered = useMemo(() => {
    const q = query.toLowerCase();
    const list = rows.filter((r) => r.name.toLowerCase().includes(q));
    list.sort((a, b) => {
      if (sort === 'size') return sizeMetric(b) - sizeMetric(a);
      if (sort === 'type') return String(a.klass || '').localeCompare(String(b.klass || ''));
      return a.name.localeCompare(b.name);
    });
    return list;
  }, [rows, query, sort]);

  const plural = (n) => `${n} ${countNoun}${n === 1 ? '' : 's'}`;

  return (
    <div className="entity-browser">
      <div className="ws-toolbar">
        <div className="ws-toolbar-left">
          <span className="ws-count">{plural(filtered.length)}</span>
          <span className="ws-sep" />
          <div className="ws-search">
            <svg width="11" height="11" viewBox="0 0 12 12" aria-hidden="true">
              <circle cx="5" cy="5" r="3.2" stroke="currentColor" strokeWidth="1.2" fill="none" />
              <path d="M7.4 7.4L10 10" stroke="currentColor" strokeWidth="1.2" strokeLinecap="round" />
            </svg>
            <input value={query} onChange={(e) => setQuery(e.target.value)}
              placeholder={`filter ${countNoun}s…`} spellCheck={false} />
          </div>
        </div>
        <div className="ws-toolbar-right">
          {view === 'list' && cols && (
            <ChooserButton className="ws-cols-btn" title="choose columns"
              label={<>Σ <span className="ve-caret">▾</span></>}
              defs={VALUE_COLUMNS} lockedLabel={nameHeader}
              visible={cols} setVisible={setCols} />
          )}
          <div className="ws-segmented" role="tablist" aria-label="Sort">
            {SORT_OPTS.map((k) => (
              <button key={k} role="tab" aria-selected={sort === k}
                className={sort === k ? 'is-active' : ''}
                onClick={() => setSort(k)}>sort: {k}</button>
            ))}
          </div>
          <div className="ws-segmented" role="tablist" aria-label="Layout">
            <button aria-selected={view === 'cards'} className={view === 'cards' ? 'is-active' : ''}
              onClick={() => setView('cards')} title="Cards view">
              <svg width="12" height="12" viewBox="0 0 12 12">
                <rect x="1" y="1"   width="4.5" height="4.5" rx="0.5" fill="currentColor"/>
                <rect x="6.5" y="1" width="4.5" height="4.5" rx="0.5" fill="currentColor"/>
                <rect x="1" y="6.5" width="4.5" height="4.5" rx="0.5" fill="currentColor"/>
                <rect x="6.5" y="6.5" width="4.5" height="4.5" rx="0.5" fill="currentColor"/>
              </svg>
            </button>
            <button aria-selected={view === 'list'} className={view === 'list' ? 'is-active' : ''}
              onClick={() => setView('list')} title="List view">
              <svg width="12" height="12" viewBox="0 0 12 12">
                <rect x="1" y="2"   width="10" height="1.4" rx="0.5" fill="currentColor"/>
                <rect x="1" y="5.3" width="10" height="1.4" rx="0.5" fill="currentColor"/>
                <rect x="1" y="8.6" width="10" height="1.4" rx="0.5" fill="currentColor"/>
              </svg>
            </button>
          </div>
        </div>
      </div>

      {view === 'cards' ? (
        <div className="ws-grid" onContextMenu={onAreaContextMenu}>
          {filtered.map((r) => (
            <EntityCard key={r.key} row={r} nameCell={nameCell}
              onOpen={onOpen} onContextMenu={onRowContextMenu} />
          ))}
          {filtered.length === 0 && <div className="ws-empty">nothing matches “{query}”</div>}
        </div>
      ) : (
        <div className="ws-list" onContextMenu={onAreaContextMenu}>
          <ValueTable
            rows={filtered}
            nameHeader={nameHeader}
            nameCell={nameCell}
            visible={cols} setVisible={setCols}
            onRowClick={onOpen}
            onRowContextMenu={onRowContextMenu}
            emptyLabel={`nothing matches “${query}”`}
          />
        </div>
      )}
      {footer}
    </div>
  );
}

/* ======================================================================== */
/* Workspace panel (the main exported component for the bottom-dock tab)    */
/* ======================================================================== */
// localStorage keys for the Workspace display preferences. Same
// `numkit.ide.*` namespace + lazy-init / write-on-change pattern as
// the editor-pane layout in IDE.jsx, so the user's chosen view (cards
// vs table) and sort order survive restarts. The search `query` is
// intentionally NOT persisted — a stale filter on restart would hide
// variables for no visible reason.
export const WS_VIEW_KEY = 'numkit.ide.workspace.view';
export const WS_SORT_KEY = 'numkit.ide.workspace.sort';

function loadPref(key, allowed, fallback) {
  try {
    const v = localStorage.getItem(key);
    if (v && allowed.includes(v)) return v;
  } catch { /* private mode / unavailable */ }
  return fallback;
}
