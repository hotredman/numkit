import { Fragment, forwardRef, useCallback, useEffect, useImperativeHandle, useMemo, useRef, useState } from 'react';

import SyntaxEditor from '../editor/SyntaxEditor';
import Sidebar from './Sidebar';
import ConsolePane from './ConsolePane';

import Toolbar from './Toolbar';
import StatusBar from './StatusBar';
import ResizeHandle from './ResizeHandle';
import PreferencesModal from './PreferencesModal';
import { WorkspacePanel, VariableEditor } from '../workspace/Workspace';
import ReferencePanel from '../reference/Reference';
import { ALL_DOCS } from '../reference/refData';
import FiguresPane from '../plot/FiguresPane';
import FigureWindow from '../plot/FigureWindow';
import NumkitGraphView from '../lang/NumkitGraphView';
import NumkitASTView from '../lang/NumkitASTView';
import NumkitASTTreeView from '../lang/NumkitASTTreeView';
import { adaptVariables, adaptFigures } from '../plot/adapters';

import tempFS from '../../temporary';
import localFS from '../../fs/local';
import { pickRunOrigin } from '../../fs/run-origin';
import { loadUiState, saveUiState } from '../../ui-state';
import { useTheme } from '../../theme';

const EMPTY_BPS = Object.freeze([]);

/* ─────────────── tab strip (mockup .editor-tabs chrome) ─────────────── */
function TabStrip({ tabs, activeTab, onSelect, onClose, onNew, onRename, onCloseAll, onCloseExcept }) {
  const [editingId, setEditingId] = useState(null);
  const [editName, setEditName]   = useState('');
  const [ctxMenu, setCtxMenu]     = useState(null);

  useEffect(() => {
    if (!ctxMenu) return;
    const h = () => setCtxMenu(null);
    window.addEventListener('mousedown', h);
    return () => window.removeEventListener('mousedown', h);
  }, [ctxMenu]);

  const stripRef = useRef(null);
  const [overflow, setOverflow] = useState(false);

  const scrollTabs = (dir) => {
    const el = stripRef.current;
    // Instant scroll — native smooth scrollBy is a no-op in the Electron/Chromium
    // shell here, so `behavior: 'smooth'` would leave the buttons doing nothing.
    // scrollLeft clamps at the ends, so clicking past an edge is a harmless no-op.
    if (el) el.scrollLeft += dir * Math.max(120, el.clientWidth * 0.8);
  };

  // Mouse wheel → horizontal scroll (the 30px strip has no room for a
  // scrollbar). Native non-passive listener so preventDefault sticks.
  useEffect(() => {
    const el = stripRef.current;
    if (!el) return undefined;
    const onWheel = (e) => {
      if (el.scrollWidth <= el.clientWidth || e.deltaX !== 0) return;
      el.scrollLeft += e.deltaY;
      e.preventDefault();
    };
    el.addEventListener('wheel', onWheel, { passive: false });
    return () => el.removeEventListener('wheel', onWheel);
  }, []);

  // Keep the active tab in view when it changes or tabs are added/removed, so
  // opening a file or clicking an off-screen tab scrolls it into reach.
  useEffect(() => {
    const strip = stripRef.current;
    const active = strip && strip.querySelector('.editor-tab.is-active');
    if (!strip || !active) return;
    const s = strip.getBoundingClientRect();
    const a = active.getBoundingClientRect();
    if (a.left < s.left) strip.scrollLeft -= s.left - a.left;
    else if (a.right > s.right) strip.scrollLeft += a.right - s.right;
  }, [activeTab, tabs.length]);

  // Show the ‹ › buttons only when the strip actually overflows.
  useEffect(() => {
    const el = stripRef.current;
    if (!el) return undefined;
    const update = () => setOverflow(el.scrollWidth > el.clientWidth + 1);
    update();
    const ro = new ResizeObserver(update);
    ro.observe(el);
    return () => ro.disconnect();
  }, [tabs.length]);

  return (
    <div className="editor-tabs">
      <div className="editor-tabs-scroll" ref={stripRef}>
      {tabs.map((tab) => {
        const isActive = tab.id === activeTab;
        return (
          <div key={tab.id}
            className={`editor-tab ${isActive ? 'is-active' : ''}`}
            onClick={() => onSelect(tab.id)}
            onContextMenu={(e) => { e.preventDefault(); setCtxMenu({ x: e.clientX, y: e.clientY, tabId: tab.id }); }}>
            <svg width="10" height="10" viewBox="0 0 12 12">
              <rect x="1" y="2" width="10" height="8" rx="1" stroke="currentColor" fill="none"/>
            </svg>
            {editingId === tab.id ? (
              <input
                value={editName}
                autoFocus
                onChange={(e) => setEditName(e.target.value)}
                onBlur={() => { onRename(tab.id, editName); setEditingId(null); }}
                onKeyDown={(e) => {
                  if (e.key === 'Enter') { onRename(tab.id, editName); setEditingId(null); }
                  if (e.key === 'Escape') setEditingId(null);
                }}
                onClick={(e) => e.stopPropagation()}
                style={{ background: 'transparent', border: 'none', outline: 'none', color: 'inherit', font: 'inherit', width: 90 }}
              />
            ) : (
              <span onDoubleClick={(e) => { e.stopPropagation(); setEditingId(tab.id); setEditName(tab.name); }}>
                {tab.name}{tab.modified ? ' •' : ''}
              </span>
            )}
            {tabs.length > 1 && (
              <button className="tab-close" aria-label="Close"
                onClick={(e) => { e.stopPropagation(); onClose(tab.id); }}>×</button>
            )}
          </div>
        );
      })}
      </div>
      <div className="editor-actions">
        {overflow && (
          <>
            <button title="Scroll tabs left" onClick={() => scrollTabs(-1)}>
              <svg width="12" height="12" viewBox="0 0 12 12">
                <path d="M7.5 2.5 4 6l3.5 3.5" stroke="currentColor" strokeWidth="1.4"
                  fill="none" strokeLinecap="round" strokeLinejoin="round"/>
              </svg>
            </button>
            <button title="Scroll tabs right" onClick={() => scrollTabs(1)}>
              <svg width="12" height="12" viewBox="0 0 12 12">
                <path d="M4.5 2.5 8 6l-3.5 3.5" stroke="currentColor" strokeWidth="1.4"
                  fill="none" strokeLinecap="round" strokeLinejoin="round"/>
              </svg>
            </button>
          </>
        )}
        <button title="New tab" onClick={onNew}>
          <svg width="12" height="12" viewBox="0 0 12 12">
            <path d="M6 2v8M2 6h8" stroke="currentColor" strokeWidth="1.3" strokeLinecap="round"/>
          </svg>
        </button>
      </div>
      {ctxMenu && (
        <div onMouseDown={(e) => e.stopPropagation()}
          style={{
            position: 'fixed', left: ctxMenu.x, top: ctxMenu.y, zIndex: 1000,
            background: 'var(--bg-3)', border: '1px solid var(--line)', borderRadius: 5,
            boxShadow: '0 4px 16px rgba(0,0,0,0.35)', minWidth: 160, padding: '4px 0',
            color: 'var(--fg-1)',
          }}>
          {[
            { label: '＋ New tab', action: () => onNew() },
            { sep: true },
            { label: '✏ Rename', action: () => { setEditingId(ctxMenu.tabId); setEditName(tabs.find((t) => t.id === ctxMenu.tabId)?.name || ''); } },
            { sep: true },
            { label: '× Close',        action: () => onClose(ctxMenu.tabId), disabled: tabs.length <= 1 },
            { label: '× Close all',    action: () => onCloseAll() },
            { label: '× Close others', action: () => onCloseExcept(ctxMenu.tabId), disabled: tabs.length <= 1 },
          ].map((item, i) => item.sep
            ? <div key={i} style={{ height: 1, background: 'var(--line)', margin: '3px 8px' }} />
            : (
              <div key={i}
                onClick={() => { if (!item.disabled) { item.action(); setCtxMenu(null); } }}
                style={{
                  padding: '5px 12px', fontSize: 11,
                  color: item.disabled ? 'var(--fg-3)' : 'var(--fg-1)',
                  cursor: item.disabled ? 'default' : 'pointer',
                  opacity: item.disabled ? 0.4 : 1,
                }}>
                {item.label}
              </div>
            )
          )}
        </div>
      )}
    </div>
  );
}

/* ─────────────── editor body (gutter + SyntaxEditor) ─────────────── */
// Wrapped in forwardRef so parents (the multi-pane layout in IDE)
// can call imperative methods on the inner SyntaxEditor — chiefly
// `setCaret(line, col)` from the AST → editor click handoff.
const EditorBody = forwardRef(function EditorBody(
  { activeTab, errorLine, debugLine, breakpoints, onToggleBp, onChange, onCursor, engine },
  ref,
) {
  const editorRef = useRef(null);
  const gutterRef = useRef(null);
  useImperativeHandle(ref, () => ({
    setCaret: (line, col) => editorRef.current?.setCaret(line, col),
    focus:    () => editorRef.current?.focus(),
  }));

  const handleScroll = useCallback((scrollTop) => {
    if (gutterRef.current) gutterRef.current.scrollTop = scrollTop;
  }, []);

  const lineCount = (activeTab?.code || '').split('\n').length;

  return (
    <div className="editor-body" style={{ display: 'flex', position: 'relative' }}>
      <div ref={gutterRef}
        style={{
          padding: '8px 0', background: 'var(--bg-0)',
          borderRight: '1px solid var(--line)', userSelect: 'none',
          minWidth: 48, overflow: 'hidden', flexShrink: 0,
        }}>
        {Array.from({ length: lineCount }, (_, i) => {
          const ln = i + 1;
          const isErr = errorLine === ln;
          const isDbg = debugLine === ln;
          const hasBp = breakpoints.includes(ln);
          return (
            <div key={i}
              onClick={() => onToggleBp(ln)}
              className="gutter-line"
              style={{
                display: 'flex', alignItems: 'center',
                fontSize: 10, lineHeight: '20px', height: 20,
                cursor: 'pointer',
                color: isErr ? 'var(--danger)' : isDbg ? 'var(--warn)' : 'var(--fg-3)',
                background: isDbg ? 'rgba(239, 168, 80, 0.15)'
                          : isErr ? 'rgba(220, 80, 80, 0.12)' : 'transparent',
                fontWeight: isErr || isDbg ? 700 : 400,
              }}>
              <span style={{
                width: 14, display: 'flex', alignItems: 'center', justifyContent: 'center',
                flexShrink: 0, opacity: hasBp ? 1 : 0,
              }}>
                <span style={{
                  width: 8, height: 8, borderRadius: '50%',
                  background: 'var(--danger)', display: 'block',
                }}/>
              </span>
              {isDbg && <span style={{ fontSize: 11, color: 'var(--warn)', marginRight: 1 }}>▶</span>}
              <span style={{ flex: 1, textAlign: 'right', paddingRight: 6 }}>{ln}</span>
            </div>
          );
        })}
      </div>
      <div style={{ flex: 1, overflow: 'hidden', background: 'var(--bg-1)' }}>
        <SyntaxEditor
          ref={editorRef}
          value={activeTab?.code || ''}
          onChange={onChange}
          onScroll={handleScroll}
          onCursor={onCursor}
          errorLine={errorLine}
          debugLine={debugLine}
          engine={engine}
          // EditorBody owns its own line-number gutter (the one with
          // breakpoint click handling). Hide SyntaxEditor's built-in
          // gutter so we don't get two columns of line numbers.
          showGutter={false}
        />
      </div>
    </div>
  );
});

/* ─────────────── bottom dock ─────────────── */
function BottomDock({
  active, setActive,
  consoleNode,
  workspaceVars, onOpenVar,
  onClose,
}) {
  return (
    <div className="dock">
      <div className="dock-tabs">
        <button className={`dock-tab ${active === 'console' ? 'is-active' : ''}`}
          onClick={() => setActive('console')}>
          <svg width="11" height="11" viewBox="0 0 12 12">
            <path d="M2 3l3 3-3 3M6 9h4" stroke="currentColor" fill="none" strokeWidth="1.3" strokeLinecap="round"/>
          </svg>
          Console
        </button>
        <button className={`dock-tab ${active === 'workspace' ? 'is-active' : ''}`}
          onClick={() => setActive('workspace')}>
          <svg width="11" height="11" viewBox="0 0 12 12">
            <circle cx="6" cy="6" r="4" stroke="currentColor" fill="none" strokeWidth="1.3"/>
            <circle cx="6" cy="6" r="1.4" fill="currentColor"/>
          </svg>
          Workspace <span className="dock-count">{workspaceVars.length}</span>
        </button>
        <button className={`dock-tab ${active === 'reference' ? 'is-active' : ''}`}
          onClick={() => setActive('reference')}>
          <svg width="11" height="11" viewBox="0 0 12 12">
            <rect x="2" y="1.5" width="8" height="9" rx="1" stroke="currentColor" fill="none"/>
            <line x1="4" y1="4" x2="8" y2="4" stroke="currentColor"/>
            <line x1="4" y1="6" x2="8" y2="6" stroke="currentColor"/>
            <line x1="4" y1="8" x2="6.5" y2="8" stroke="currentColor"/>
          </svg>
          Reference
        </button>
        <div className="dock-spacer" />
        <button className="dock-iconbtn" title="Close panel" onClick={onClose}>×</button>
      </div>
      <div className="dock-body">
        {/* Console is mounted at all times so its history/state survives
            tab switches; visibility is toggled via display + dimensions
            so the inner `.console` (flex column) gets a real height to
            distribute between its scrollable output and the input row. */}
        <div style={{
          display:        active === 'console' ? 'block' : 'none',
          height:         '100%',
          minHeight:      0,
          overflow:       'hidden',
        }}>
          {consoleNode}
        </div>
        {active === 'workspace' && <WorkspacePanel variables={workspaceVars} onOpen={onOpenVar} />}
        {active === 'reference' && <ReferencePanel docs={ALL_DOCS} />}
      </div>
    </div>
  );
}

/* ─────────────── main IDE shell ─────────────── */
export default function IDE({ engine, status, vfsAdapters, onLocalMount }) {
  const C = useTheme();
  const { themeName, toggleTheme } = C;

  const [savedState] = useState(() => loadUiState());

  // Layout / panel visibility
  const [panels, setPanels] = useState(() => {
    const l = savedState?.layout || {};
    return {
      explorer: l.showLeft   ?? true,
      editor:   l.showCenter ?? true,
      figures:  l.showRight  ?? false,
      terminal: l.showBottom ?? true,
    };
  });
  const togglePanel = (k) => setPanels((p) => ({ ...p, [k]: !p[k] }));

  const [sidebarW, setSidebarW] = useState(220);
  const [figuresW, setFiguresW] = useState(() => savedState?.layout?.figuresWidth ?? 440);
  const [dockH,    setDockH]    = useState(() => savedState?.layout?.bottomHeight ?? 280);

  // Drag-resize of the right pane / dock height does not trigger window.resize,
  // so plot canvases inside the pane wouldn't redraw at the new size in browsers
  // where ResizeObserver behaves oddly. Dispatch a synthetic resize whenever any
  // pane dimension changes so subscribers re-measure on the next layout pass.
  useEffect(() => {
    window.dispatchEvent(new Event('resize'));
  }, [sidebarW, figuresW, dockH, panels.explorer, panels.figures, panels.terminal, panels.editor]);

  // Engine/output state
  const [output, setOutput]       = useState([]);
  const [figures, setFigures]     = useState([]);
  const [variables, setVariables] = useState({});
  const [helpTopic, setHelpTopic] = useState(null);
  const [execTimeMs, setExecTimeMs] = useState(null);
  const [errorLine, setErrorLine] = useState(null);
  const [, setConsoleNotify] = useState(false);
  const [bottomTab, setBottomTab] = useState('console');

  // Tabs
  const [tabs, setTabs] = useState(() => {
    const t = savedState?.tabs;
    const legacyByPath = savedState?.breakpointsByPath;
    const normalize = (x) => {
      let bps = Array.isArray(x.breakpoints) ? x.breakpoints.slice() : [];
      if (legacyByPath && x.vfsPath && Array.isArray(legacyByPath[x.vfsPath])) {
        bps = Array.from(new Set([...bps, ...legacyByPath[x.vfsPath]]));
      }
      bps.sort((a, b) => a - b);
      return { ...x, breakpoints: bps };
    };
    if (Array.isArray(t) && t.length > 0) return t.map(normalize);
    return [{ id: '1', name: 'untitled.m', code: '', modified: false, vfsPath: null, source: null, breakpoints: [] }];
  });
  const [activeTab, setActiveTab] = useState(() => {
    const t = savedState?.tabs;
    const a = savedState?.activeTab;
    if (Array.isArray(t) && a && t.some((x) => x.id === a)) return a;
    if (Array.isArray(t) && t.length > 0) return t[0].id;
    return '1';
  });
  const tabCountRef = useRef(
    Array.isArray(savedState?.tabs) && savedState.tabs.length > 0
      ? Math.max(...savedState.tabs.map((x) => parseInt(x.id, 10) || 0), 1)
      : 1
  );

  // Debug
  const [debugState, setDebugState] = useState(null);
  const [debugLine, setDebugLine]   = useState(null);

  // VFS refresh
  const [vfsRefreshKey, setVfsRefreshKey] = useState(0);

  // Save dialog
  const [showSaveDialog, setShowSaveDialog] = useState(false);
  const [saveFileName, setSaveFileName] = useState('');

  // New modals from mockup
  const [openVar, setOpenVar]         = useState(null);
  const [openFigure, setOpenFigure]   = useState(null);
  // Preferences modal
  const [prefsOpen, setPrefsOpen]     = useState(false);
  // Build & Run state — true while codegen IPC is in flight
  const [isBuildRunning, setIsBuildRunning] = useState(false);
  // Run state — true while runCode is in flight (prevents concurrent runs)
  const [isRunning, setIsRunning] = useState(false);

  const mountedRef = useRef(true);
  useEffect(() => () => { mountedRef.current = false; }, []);
  const warnedFallbackRef = useRef(false);
  useEffect(() => { if (vfsAdapters?.local) warnedFallbackRef.current = false; }, [vfsAdapters?.local]);

  // Engine version banner
  const [engineVersion, setEngineVersion] = useState(null);
  // Editor view mode: 'text' = standard code editor, 'graph' = data-flow
  // node graph (NumkitGraphView). Single-shared state across tabs for
  // MVP — switching tabs keeps whatever view the user picked last.
  // Editor area is a multi-pane split: any subset of {text, graph,
  // ast, tree} renders side-by-side as horizontal-flex panes. The
  // user picks which to see via independent toggle buttons; at
  // least one must stay active (the toggle handler refuses to
  // turn off the last). Pane order on screen matches EDITOR_PANE_ORDER
  // (text → graph → AST → tree). Both the on/off state and the
  // per-pane flex-grow ratios are persisted to localStorage so the
  // user's layout survives restarts.
  const STORAGE_PANES = 'numkit.ide.editor.panes';
  const STORAGE_FRACS = 'numkit.ide.editor.paneFracs';
  const DEFAULT_PANES = { text: true, graph: false, ast: false, tree: false };
  const DEFAULT_FRACS = { text: 1,    graph: 1,     ast: 1,     tree: 1     };
  const [editorPanes, setEditorPanes] = useState(() => {
    try {
      const raw = localStorage.getItem(STORAGE_PANES);
      if (raw) return { ...DEFAULT_PANES, ...JSON.parse(raw) };
    } catch { /* ignore */ }
    return DEFAULT_PANES;
  });
  const [paneFracs, setPaneFracs] = useState(() => {
    try {
      const raw = localStorage.getItem(STORAGE_FRACS);
      if (raw) return { ...DEFAULT_FRACS, ...JSON.parse(raw) };
    } catch { /* ignore */ }
    return DEFAULT_FRACS;
  });
  useEffect(() => {
    try { localStorage.setItem(STORAGE_PANES, JSON.stringify(editorPanes)); }
    catch { /* ignore */ }
  }, [editorPanes]);
  useEffect(() => {
    try { localStorage.setItem(STORAGE_FRACS, JSON.stringify(paneFracs)); }
    catch { /* ignore */ }
  }, [paneFracs]);
  const toggleEditorPane = useCallback((key) => {
    setEditorPanes((prev) => {
      const next = { ...prev, [key]: !prev[key] };
      if (!Object.values(next).some(Boolean)) return prev;
      return next;
    });
  }, []);
  const ensureEditorPane = useCallback((key) => {
    setEditorPanes((prev) => prev[key] ? prev : { ...prev, [key]: true });
  }, []);
  const multiPaneRef = useRef(null);
  // Drag-resize handler factory — returns an onResize(dx) closure
  // tied to the left+right neighbour pair around a specific
  // ResizeHandle. The handle shifts flex-grow units between the
  // two; total stays roughly constant so other panes keep their
  // share. Min frac 0.1 stops a pane from collapsing to invisible.
  const onDragPaneBorder = useCallback((leftKey, rightKey) => (dx /*, dy */) => {
    if (!dx) return;
    setPaneFracs((prev) => {
      const containerW = multiPaneRef.current?.clientWidth || 1000;
      const active = ['text', 'graph', 'ast', 'tree']
        .filter((k) => editorPanes[k]);
      const totalFrac = active.reduce((s, k) => s + (prev[k] || 1), 0);
      const dFrac = (dx / containerW) * totalFrac;
      const lf = Math.max(0.1, (prev[leftKey]  || 1) + dFrac);
      const rf = Math.max(0.1, (prev[rightKey] || 1) - dFrac);
      return { ...prev, [leftKey]: lf, [rightKey]: rf };
    });
  }, [editorPanes]);
  const resetPaneFracs = useCallback(() => setPaneFracs(DEFAULT_FRACS), []);
  // Caret position in the active editor tab, tracked for bidirectional
  // sync with the AST view (cursor on line N → AST highlights the
  // deepest node whose source range contains line N).
  const [editorCursor, setEditorCursor] = useState({ line: 1, col: 1 });
  // Imperative handle to the editor body — lets onNavigate (AST →
  // editor) place the real caret instead of just highlighting the
  // line via setErrorLine.
  const editorBodyRef = useRef(null);
  useEffect(() => {
    const v = engine?.version?.();
    if (v) setEngineVersion(v);
  }, [engine, status]);
  useEffect(() => {
    const banner = engineVersion ? `Numkit IDE v3 — build ${engineVersion}` : 'Numkit IDE v3';
    setOutput([
      { type: 'system', text: banner },
      { type: 'system', text: 'Type commands below. "help <topic>" for function info.' },
    ]);
  }, [engineVersion]);

  /* ─────────────── helpers ─────────────── */
  // Hard caps. Without these, two state arrays in IDE grew unbounded and
  // were the proven path to V8 OOM in long Electron sessions:
  //   - `output` — ConsolePane renders one <div> per entry, so loops with
  //     disp/fprintf push tens of thousands of nodes into the React tree.
  //   - `figures` — each figure can hold a Z matrix or long x/y arrays;
  //     scripts that forget `close all` accumulate them forever.
  const OUTPUT_CAP = 5000;
  const FIGURE_CAP = 50;
  const addOutput = useCallback((items) => {
    setOutput((prev) => {
      for (const i of items) if (i.text === '__CLEAR__') return [];
      const filtered = items.filter((i) => i.text !== '__CLEAR__');
      const next = prev.length + filtered.length > OUTPUT_CAP
        ? [...prev, ...filtered].slice(-OUTPUT_CAP)
        : [...prev, ...filtered];
      return next;
    });
  }, []);

  const runCode = useCallback(async (code) => {
    if (isRunning) return; // guard against concurrent runs
    setIsRunning(true);
    try {
    const activeTabObj = tabs.find((t) => t.id === activeTab);
    const { adapter, origin, fallbackUsed } = pickRunOrigin(activeTabObj?.source, vfsAdapters);

    if (adapter?.refresh) {
      try { await adapter.refresh(); } catch (e) { console.warn('[runCode] refresh failed', e); }
    }
    if (fallbackUsed && !warnedFallbackRef.current) {
      addOutput([{
        type: 'warning',
        text: 'Warning: Local Folder not mounted — file I/O for this run will go to Temporary. Mount a folder from the File Browser to persist writes to disk.',
      }]);
      setConsoleNotify(true);
      warnedFallbackRef.current = true;
    }

    let scriptDir = null;
    if (activeTabObj?.vfsPath) {
      const idx = Math.max(activeTabObj.vfsPath.lastIndexOf('/'), activeTabObj.vfsPath.lastIndexOf('\\'));
      if (idx > 0) scriptDir = activeTabObj.vfsPath.slice(0, idx);
      else if (idx === 0) scriptDir = '/';
    }

    const t0 = performance.now();
    let result;

    // ── Electron: route through persistent native REPL session ──
    if (typeof window.nativeFS !== 'undefined' && window.nativeFS.runRepl) {
      const r = await window.nativeFS.runRepl(code);
      setExecTimeMs(performance.now() - t0);
      setErrorLine(r.errorLine ?? null);   // highlight failing line; null = clear

      if (r.notFound) {
        addOutput([{
          type: 'warning',
          text: '[Run] numkit_repl.exe not found. Configure the path in Settings (gear icon).',
        }]);
        setConsoleNotify(true);
        return;
      }
      if (r.sessionRestarted) {
        addOutput([{
          type: 'system',
          text: '[!] REPL session crashed and was automatically restarted. Workspace has been reset.',
        }]);
        setConsoleNotify(true);
      }

      // stdout is already stripped of figure markers by _flush(); display as-is.
      const rawOutput = (r.stdout || '') + (r.stderr ? '\n' + r.stderr : '');
      const items = [];
      for (const line of rawOutput.split('\n')) {
        if (!line && items.length === 0) continue; // skip leading blank
        if (line === '__CLEAR__') { setOutput([]); continue; }
        items.push({
          type: /^Error/.test(line) ? 'error'
              : /^Warning:/.test(line) ? 'warning'
              : 'result',
          text: line,
        });
      }
      // Trim trailing empty lines.
      while (items.length && items[items.length - 1].text.trim() === '') items.pop();
      if (items.length) { addOutput(items); setConsoleNotify(true); }

      // Update workspace panel from native session state.
      if (r.vars && typeof r.vars === 'object') setVariables(r.vars);

      // Update figures from native REPL output (extracted by _flush()).
      if (r.closeAll) setFigures([]);
      else if (r.closedFigureIds?.length) {
        const closed = new Set(r.closedFigureIds);
        setFigures((prev) => prev.filter((f) => !closed.has(f.id)));
      }
      if (r.figures?.length) {
        setFigures((prev) => {
          const map = new Map(prev.map((f) => [f.id, f]));
          for (const fig of r.figures) map.set(fig.id, fig);
          const list = Array.from(map.values());
          if (list.length > FIGURE_CAP) {
            console.warn(`[IDE] Capped figures at ${FIGURE_CAP} (native run); dropped ${list.length - FIGURE_CAP} oldest.`);
            return list.slice(-FIGURE_CAP);
          }
          return list;
        });
        setPanels((p) => ({ ...p, figures: true }));
      }

      if (adapter) adapter.flush().then((wasDirty) => {
        if (mountedRef.current && wasDirty) setVfsRefreshKey((k) => k + 1);
      });
      return;
    }

    // ── Browser: WASM engine ──────────────────────────────────────
    if (origin) engine.pushScriptOrigin(origin, scriptDir);
    try {
      result = engine.execute(code);
    } finally {
      if (origin) engine.popScriptOrigin();
    }
    setExecTimeMs(performance.now() - t0);
    setErrorLine(null);

    const items = [];
    if (result.output) {
      for (const line of result.output.split('\n')) {
        if (line === '__CLEAR__') { setOutput([]); continue; }
        items.push({
          type: /^Error/.test(line) ? 'error'
              : /^Warning:/.test(line) ? 'warning'
              : 'result',
          text: line,
        });
      }
    }
    if (items.length) { addOutput(items); setConsoleNotify(true); }

    if (result.closeAllFigures) setFigures([]);
    else if (result.closedFigureIds?.length) {
      const closed = new Set(result.closedFigureIds);
      setFigures((prev) => prev.filter((f) => !closed.has(f.id)));
    }
    if (result.figures?.length) {
      setFigures((prev) => {
        const map = new Map(prev.map((f) => [f.id, f]));
        for (const fig of result.figures) map.set(fig.id, fig);
        const list = Array.from(map.values());
        if (list.length > FIGURE_CAP) {
          const dropped = list.length - FIGURE_CAP;
          console.warn(`[IDE] Capped figures at ${FIGURE_CAP}; dropped ${dropped} oldest.`
                     + ' Add `close all` between runs to avoid this.');
          return list.slice(-FIGURE_CAP);
        }
        return list;
      });
      setPanels((p) => ({ ...p, figures: true }));
    }
    if (result.errorLine) setErrorLine(result.errorLine);
    setVariables(engine.getVars());

    if (adapter) adapter.flush().then((wasDirty) => {
      if (mountedRef.current && wasDirty) setVfsRefreshKey((k) => k + 1);
    });
    } finally {
      setIsRunning(false);
    }
  }, [engine, addOutput, tabs, activeTab, vfsAdapters, isRunning]);





  /* ─────────────── debug ─────────────── */
  const toggleBreakpoint = useCallback((line) => {
    setTabs((prev) => prev.map((t) => {
      if (t.id !== activeTab) return t;
      const has = t.breakpoints.includes(line);
      const next = has
        ? t.breakpoints.filter((l) => l !== line)
        : [...t.breakpoints, line].sort((a, b) => a - b);
      return { ...t, breakpoints: next };
    }));
  }, [activeTab]);

  const activeBreakpoints = useMemo(() => {
    const tab = tabs.find((t) => t.id === activeTab);
    return tab?.breakpoints || EMPTY_BPS;
  }, [tabs, activeTab]);

  useEffect(() => {
    engine.debugSetBreakpoints(activeBreakpoints);
  }, [activeBreakpoints, engine]);

  const initialSaveSkippedRef = useRef(false);
  useEffect(() => {
    if (!initialSaveSkippedRef.current) { initialSaveSkippedRef.current = true; return; }
    saveUiState({
      layout: {
        showLeft: panels.explorer, showCenter: panels.editor,
        showRight: panels.figures, showBottom: panels.terminal,
        figuresWidth: figuresW, bottomHeight: dockH,
      },
      tabs, activeTab,
    });
  }, [tabs, activeTab, panels, figuresW, dockH]);

  const handleDebugResult = useCallback((result) => {
    if (result.output) {
      const items = result.output.split('\n').map((line) => ({
        type: /^Error/.test(line) ? 'error' : 'result',
        text: line,
      }));
      addOutput(items);
      setConsoleNotify(true);
    }
    if (result.closeAllFigures) setFigures([]);
    else if (result.closedFigureIds?.length) {
      const closed = new Set(result.closedFigureIds);
      setFigures((prev) => prev.filter((f) => !closed.has(f.id)));
    }
    if (result.figures?.length) {
      setFigures((prev) => {
        const map = new Map(prev.map((f) => [f.id, f]));
        for (const fig of result.figures) map.set(fig.id, fig);
        const list = Array.from(map.values());
        if (list.length > FIGURE_CAP) {
          console.warn(`[IDE] Capped figures at ${FIGURE_CAP} (debug); dropped ${list.length - FIGURE_CAP} oldest.`);
          return list.slice(-FIGURE_CAP);
        }
        return list;
      });
      setPanels((p) => ({ ...p, figures: true }));
    }
    if (result.status === 'paused' && result.pauseState) {
      const ps = result.pauseState;
      setDebugLine(ps.line);
      setDebugState({ status: 'paused', line: ps.line, variables: ps.variables || {}, reason: ps.reason });
      addOutput([{ type: 'system', text: `⏸ Paused at line ${ps.line} (${ps.reason})` }]);
      if (ps.variables && typeof ps.variables === 'object') {
        const debugVars = {};
        for (const [name, preview] of Object.entries(ps.variables)) debugVars[name] = preview;
        setVariables(debugVars);
      }
    } else if (result.status === 'completed') {
      setDebugLine(null);
      setDebugState(null);
      addOutput([{ type: 'system', text: '✓ Debug completed' }]);
      setConsoleNotify(true);
      // Prefer vars from the debug result (native path sends __VARS__: on completion).
      if (result.vars && typeof result.vars === 'object') setVariables(result.vars);
      else setVariables(engine.getVars());
    } else if (result.status === 'error') {
      setDebugLine(null);
      setDebugState(null);
      if (result.line) setErrorLine(result.line);
      addOutput([{ type: 'error', text: `Error: ${result.message}` }]);
      setConsoleNotify(true);
      if (result.vars && typeof result.vars === 'object') setVariables(result.vars);
      else setVariables(engine.getVars());
    }
  }, [engine, addOutput]);

  const debugStart = useCallback(async () => {
    const tab = tabs.find((t) => t.id === activeTab);
    if (!tab || !tab.code.trim()) return;
    setPanels((p) => ({ ...p, terminal: true }));
    setErrorLine(null);
    addOutput([{ type: 'system', text: `── Debug ${tab.name} ──` }]);
    setConsoleNotify(true);
    const t0 = performance.now();
    const result = await engine.debugStart(tab.code);
    setExecTimeMs(performance.now() - t0);
    handleDebugResult(result);
  }, [tabs, activeTab, engine, addOutput, handleDebugResult]);

  const debugResume = useCallback(async (action = 0) => {
    if (!debugState || debugState.status !== 'paused') return;
    const result = await engine.debugResume(action);
    handleDebugResult(result);
  }, [debugState, engine, handleDebugResult]);

  const debugStop = useCallback(async () => {
    await engine.debugStop?.();
    setDebugLine(null);
    setDebugState(null);
    addOutput([{ type: 'system', text: '■ Debug stopped' }]);
    setConsoleNotify(true);
  }, [engine, addOutput]);

  /* ─────────────── tabs / files ─────────────── */
  const newTab = useCallback(() => {
    tabCountRef.current++;
    const id = String(tabCountRef.current);
    setTabs((p) => [...p, {
      id, name: `script${tabCountRef.current}.m`,
      code: '', modified: false, vfsPath: null, source: null, breakpoints: [],
    }]);
    setActiveTab(id);
  }, []);
  const closeTab = useCallback((id) => {
    setTabs((p) => {
      const n = p.filter((t) => t.id !== id);
      if (!n.length) return p;
      if (activeTab === id) setActiveTab(n[n.length - 1].id);
      return n;
    });
  }, [activeTab]);
  const closeAllTabs = useCallback(() => {
    tabCountRef.current++;
    const id = String(tabCountRef.current);
    setTabs([{ id, name: 'untitled.m', code: '', modified: false, vfsPath: null, source: null, breakpoints: [] }]);
    setActiveTab(id);
  }, []);
  const closeOtherTabs = useCallback((id) => {
    setTabs((p) => {
      const keep = p.find((t) => t.id === id);
      return keep ? [keep] : p;
    });
    setActiveTab(id);
  }, []);
  const renameTab = useCallback((id, name) => {
    if (!name.trim()) return;
    setTabs((p) => p.map((t) => (t.id === id ? { ...t, name: name.trim() } : t)));
  }, []);
  const updateTabCode = useCallback((code) => {
    setTabs((p) => p.map((t) => (t.id === activeTab ? { ...t, code, modified: true } : t)));
    setErrorLine(null);
  }, [activeTab]);
  const activeTabData = tabs.find((t) => t.id === activeTab) || tabs[0];

  const runActiveTab = useCallback(() => {
    const tab = tabs.find((t) => t.id === activeTab);
    if (!tab || !tab.code.trim()) return;
    setPanels((p) => ({ ...p, terminal: true }));
    setDebugLine(null);
    setDebugState(null);
    // Just announce the run — the source is already in the editor pane,
    // dumping it into the console is noise that pushes useful output off
    // the visible area.
    addOutput([{ type: 'system', text: `── Running ${tab.name} ──` }]);
    setConsoleNotify(true);
    runCode(tab.code);
    setTabs((p) => p.map((t) => (t.id === activeTab ? { ...t, modified: false } : t)));
  }, [tabs, activeTab, addOutput, runCode]);

  // Build & Run — transpile + AOT-compile + run the active tab via
  // numkit_codegen --run. Requires the Electron IPC bridge; gracefully
  // degrades in browser mode with an explanatory console message.
  const handleBuildRun = useCallback(async () => {
    const tab = tabs.find((t) => t.id === activeTab);
    if (!tab || !tab.code.trim()) return;

    setPanels((p) => ({ ...p, terminal: true }));
    setBottomTab('console');
    setConsoleNotify(true);

    if (!window.nativeFS?.runCodegen) {
      addOutput([{
        type: 'warning',
        text: '[Build & Run] Not available in browser mode — requires the Electron desktop app.',
      }]);
      return;
    }

    setIsBuildRunning(true);
    addOutput([{ type: 'system', text: `── Build & Run ${tab.name} ──` }]);

    let result;
    try {
      result = await window.nativeFS.runCodegen(tab.code);
    } catch (err) {
      addOutput([{ type: 'error', text: `[Build & Run] IPC error: ${err?.message || err}` }]);
      setIsBuildRunning(false);
      return;
    }

    if (result.notFound) {
      addOutput([{
        type: 'warning',
        text: '[Build & Run] numkit_codegen not found. Set the path in Settings (⚙ Settings → Code Generator).',
      }]);
    } else {
      // stderr contains the compiler log ("compiled → /tmp/…") and any
      // codegen diagnostics; stdout is the program's own output.
      if (result.stderr) {
        for (const line of result.stderr.trimEnd().split('\n')) {
          if (!line) continue;
          addOutput([{ type: 'system', text: line }]);
        }
      }
      if (result.stdout) {
        for (const line of result.stdout.trimEnd().split('\n')) {
          addOutput([{ type: 'result', text: line }]);
        }
      }
      if (result.exitCode !== 0) {
        addOutput([{ type: 'error', text: `[Build & Run] exited with code ${result.exitCode}` }]);
      }
    }

    setIsBuildRunning(false);
    setConsoleNotify(true);
  }, [tabs, activeTab, addOutput]);

  const handleOpenFile = useCallback((filename, content, vfsPath, source) => {
    const existing = tabs.find((t) => t.vfsPath && t.vfsPath === vfsPath);
    if (existing) { setActiveTab(existing.id); return; }
    tabCountRef.current++;
    const id = String(tabCountRef.current);
    setTabs((p) => [...p, {
      id, name: filename, code: content, modified: false,
      vfsPath: vfsPath || null, source: source || null, breakpoints: [],
    }]);
    setActiveTab(id);
    setPanels((p) => ({ ...p, editor: true }));
  }, [tabs]);

  const handleSaveToFS = useCallback(async (path, name) => {
    const tab = tabs.find((t) => t.id === activeTab);
    if (!tab) return;
    const fullPath = path || tab.vfsPath;
    if (!fullPath) return;
    const targetSource = tab.source === 'localFolder' ? 'localFolder' : 'temporary';
    try {
      if (targetSource === 'localFolder') await localFS.writeFile(fullPath, tab.code);
      else                                 await tempFS.writeFile(fullPath, tab.code);
    } catch (e) {
      addOutput([{ type: 'error', text: `Save failed: ${e?.message || e}` }]);
      setConsoleNotify(true);
      return;
    }
    setTabs((p) => p.map((t) => (t.id === activeTab
      ? { ...t, modified: false, vfsPath: fullPath, name: name || t.name, source: targetSource }
      : t)));
    setVfsRefreshKey((k) => k + 1);
    addOutput([{ type: 'system', text: `Saved ${name || tab.name}` }]);
    setConsoleNotify(true);
  }, [tabs, activeTab, addOutput]);

  const handleSave = useCallback(() => {
    const tab = tabs.find((t) => t.id === activeTab);
    if (!tab) return;
    if (tab.vfsPath) handleSaveToFS(tab.vfsPath, tab.name);
    else { setSaveFileName(tab.name); setShowSaveDialog(true); }
  }, [tabs, activeTab, handleSaveToFS]);

  const handleSaveDialogSubmit = useCallback(async () => {
    if (!saveFileName.trim()) return;
    let name = saveFileName.trim();
    if (!name.includes('.')) name += '.m';
    await handleSaveToFS(`/${name}`, name);
    setShowSaveDialog(false);
  }, [saveFileName, handleSaveToFS]);

  const isTabUnsaved = useCallback(
    (path, source) => tabs.some((t) => t.vfsPath === path && t.source === source && t.modified),
    [tabs]
  );

  // Keyboard shortcuts (Ctrl+S, F5, F10, F11, Shift+F11, Shift+F5)
  useEffect(() => {
    const h = (e) => {
      if ((e.ctrlKey || e.metaKey) && e.key === 's') {
        e.preventDefault();
        handleSave();
        return;
      }
      if (e.key === 'F5') {
        e.preventDefault();
        if (e.shiftKey) {
          debugStop();
        } else if (debugState?.status === 'paused') {
          debugResume(0); // Continue
        } else if (!isRunning) {
          debugStart();
        }
        return;
      }
      if (debugState?.status === 'paused') {
        if (e.key === 'F10') {
          e.preventDefault();
          debugResume(1); // Step Over
        } else if (e.key === 'F11') {
          e.preventDefault();
          if (e.shiftKey) debugResume(3); // Step Out
          else debugResume(2); // Step Into
        }
      }
    };
    window.addEventListener('keydown', h);
    return () => window.removeEventListener('keydown', h);
  }, [handleSave, debugState, isRunning, debugResume, debugStart, debugStop]);

  /* ─────────────── adapted data ─────────────── */
  const workspaceVars = useMemo(() => adaptVariables(variables), [variables]);
  const adaptedFigures = useMemo(() => adaptFigures(figures), [figures]);

  // Notify the user once (non-blocking) that figures with non-line plots
  // exist but the new viewer can only show line/scatter — they should use
  // the right pane's classic renderer for those. Cheap: only fires when
  // count drops between adaptation passes.
  const droppedFigures = figures.length - adaptedFigures.length;

  // Both close paths used to fire-and-forget engine.execute and ignore
  // the result. The engine emits __FIGURE_CLOSE__:id (or
  // __FIGURE_CLOSE_ALL__) in the output stream, which extractMarkers
  // turns into result.closedFigureIds / result.closeAllFigures — the
  // ONLY signal the IDE has that the figure is gone on the C++ side.
  // Without consuming it, the figures state stayed stale and the X
  // button on the preview cards looked broken.
  const handleCloseFigure = useCallback((id) => {
    const result = engine.execute(`close(${id})`);
    if (result?.closeAllFigures) setFigures([]);
    else if (result?.closedFigureIds?.length) {
      const closed = new Set(result.closedFigureIds);
      setFigures((prev) => prev.filter((f) => !closed.has(f.id)));
    } else {
      // Fallback: if the engine didn't emit a marker (older WASM
      // without close() instrumentation), drop the id locally so the
      // UI at least reflects the user's intent.
      setFigures((prev) => prev.filter((f) => f.id !== id));
    }
  }, [engine]);
  const handleCloseAllFigures = useCallback(() => {
    const result = engine.execute("close('all')");
    if (result?.closeAllFigures) setFigures([]);
    else if (result?.closedFigureIds?.length) {
      const closed = new Set(result.closedFigureIds);
      setFigures((prev) => prev.filter((f) => !closed.has(f.id)));
    } else {
      // Fallback as above — clear locally so the UI reflects intent.
      setFigures([]);
    }
  }, [engine]);

  const handleBottomTabChange = useCallback((id) => {
    setBottomTab(id);
    if (id === 'console') setConsoleNotify(false);
  }, []);

  const isDebugging = debugState?.status === 'paused';

  /* ─────────────── grid cols / rows ─────────────── */
  const cols = [];
  if (panels.explorer) cols.push(`${sidebarW}px`, '4px');
  cols.push('minmax(0, 1fr)');
  if (panels.figures) cols.push('4px', `${figuresW}px`);

  const rows = [];
  if (panels.editor) rows.push('minmax(0, 1fr)');
  if (panels.editor && panels.terminal) rows.push('4px');
  if (panels.terminal) rows.push(`${dockH}px`);

  const centerVisible = panels.editor || panels.terminal;

  /* ─────────────── render ─────────────── */
  // .ide is a CSS grid with 3 rows (toolbar / main / statusbar). When the
  // debug session pauses we render a 4th element (the debug toolbar) — push
  // an extra explicit row into the template so it doesn't auto-place into
  // the 22px statusbar row and squash main to nothing.
  const gridRows = isDebugging
    ? '36px 28px 1fr 22px'
    : '36px 1fr 22px';
  return (
    <div className="ide" style={{ gridTemplateRows: gridRows }}>
      <Toolbar
        panels={panels}
        togglePanel={togglePanel}
        theme={themeName}
        onToggleTheme={toggleTheme}
        onRun={runActiveTab}
        onBuildRun={handleBuildRun}
        onDebug={debugStart}
        onStop={debugStop}
        onSave={handleSave}
        onClear={() => setOutput([])}
        onReset={() => {
          engine.reset();
          setVariables({});
          setDebugLine(null);
          setDebugState(null);
          addOutput([{ type: 'system', text: 'Workspace cleared.' }]);
          setConsoleNotify(true);
        }}
        onOpenPreferences={() => setPrefsOpen(true)}
        isDebugging={isDebugging}
        isBuildRunning={isBuildRunning}
        isRunning={isRunning}
        canRun={Boolean(activeTabData?.code?.trim())}
      />

      {/* Debug toolbar — shown when paused */}
      {isDebugging && (
        <div style={{
          display: 'flex', alignItems: 'center', gap: 8,
          padding: '4px 12px',
          background: 'rgba(239, 168, 80, 0.12)',
          borderBottom: '1px solid rgba(239, 168, 80, 0.30)',
          fontSize: 11, color: 'var(--warn)',
          gridColumn: '1 / -1',
        }}>
          <span style={{ fontWeight: 600 }}>⏸ Paused at line {debugState.line}</span>
          <span style={{ color: 'var(--fg-3)' }}>({debugState.reason})</span>
          <span style={{ flex: 1 }}/>
          {[
            { lbl: '↓ Into', act: 2, title: 'Step Into (F11)' },
            { lbl: '→ Over', act: 1, title: 'Step Over (F10)' },
            { lbl: '↑ Out',  act: 3, title: 'Step Out (Shift+F11)' },
            { lbl: '▶ Continue', act: 0, title: 'Continue (F5)', accent: true },
          ].map(({ lbl, act, title, accent }) => (
            <button key={act} title={title} onClick={() => debugResume(act)}
              style={{
                padding: '3px 10px', borderRadius: 4, fontSize: 10, fontWeight: 600,
                background: accent ? 'var(--accent-2)' : 'var(--bg-3)',
                color: accent ? '#fff' : 'var(--fg-1)',
                border: '1px solid var(--line)', cursor: 'pointer',
              }}>{lbl}</button>
          ))}
        </div>
      )}

      <div className="ide-main" style={{ gridTemplateColumns: cols.join(' ') }}>
        {panels.explorer && (
          <Sidebar
            onOpenFile={handleOpenFile}
            vfsRefreshKey={vfsRefreshKey}
            isTabUnsaved={isTabUnsaved}
            onLocalMount={onLocalMount}
            vfsAdapters={vfsAdapters}
          />
        )}
        {panels.explorer && (
          <ResizeHandle orientation="vertical"
            onResize={(dx) => setSidebarW((w) => Math.max(140, Math.min(520, w + dx)))}
            onDoubleClick={() => setSidebarW(220)} />
        )}

        {centerVisible ? (
          <div className="ide-center" style={{ gridTemplateRows: rows.join(' ') }}>
            {panels.editor && (
              <div className="editor">
                <TabStrip
                  tabs={tabs}
                  activeTab={activeTab}
                  onSelect={setActiveTab}
                  onClose={closeTab}
                  onNew={newTab}
                  onRename={renameTab}
                  onCloseAll={closeAllTabs}
                  onCloseExcept={closeOtherTabs}
                />
                {/* Editor view toggle — INDEPENDENT checkboxes for
                    text / graph / AST. Any combination renders side-
                    by-side with equal horizontal flex. Click on an
                    already-active toggle hides that pane (unless
                    it's the last one). This is also where the
                    bidirectional cursor sync becomes useful in
                    real time: turn on text + AST together and the
                    editor caret highlights the corresponding AST
                    node as you move it. */}
                <div className="editor-view-toggle">
                  <button
                    className={`evt-btn${editorPanes.text ? ' is-active' : ''}`}
                    onClick={() => toggleEditorPane('text')}
                    title="Text editor pane (toggle)"
                  >text</button>
                  <button
                    className={`evt-btn${editorPanes.graph ? ' is-active' : ''}`}
                    onClick={() => toggleEditorPane('graph')}
                    title="Data-flow graph pane (toggle)"
                  >graph</button>
                  <button
                    className={`evt-btn${editorPanes.ast ? ' is-active' : ''}`}
                    onClick={() => toggleEditorPane('ast')}
                    title="Parse-tree AST — graph layout (toggle)"
                  >AST</button>
                  <button
                    className={`evt-btn${editorPanes.tree ? ' is-active' : ''}`}
                    onClick={() => toggleEditorPane('tree')}
                    title="Parse-tree AST — indented tree (astexplorer-style)"
                  >tree</button>
                </div>
                {(() => {
                  // Render the active panes in a fixed left-to-right
                  // order, with a ResizeHandle between each adjacent
                  // pair. flex-grow per pane is read from paneFracs
                  // so the user's drag positions persist across
                  // toggles + restarts.
                  const ORDER = ['text', 'graph', 'ast', 'tree'];
                  const active = ORDER.filter((k) => editorPanes[k]);
                  const renderPane = (k) => {
                    switch (k) {
                      case 'text':
                        return (
                          <EditorBody
                            ref={editorBodyRef}
                            activeTab={activeTabData}
                            errorLine={errorLine}
                            debugLine={debugLine}
                            breakpoints={activeBreakpoints}
                            onToggleBp={toggleBreakpoint}
                            onChange={updateTabCode}
                            onCursor={(line, col) => setEditorCursor({ line, col })}
                            engine={engine}
                          />
                        );
                      case 'graph':
                        return (
                          <NumkitGraphView
                            source={activeTabData?.code || ''}
                            engine={engine}
                          />
                        );
                      case 'ast':
                        return (
                          <NumkitASTView
                            source={activeTabData?.code || ''}
                            engine={engine}
                            cursorLine={editorCursor.line}
                            onNavigate={(line, col) => {
                              ensureEditorPane('text');
                              queueMicrotask(() => {
                                editorBodyRef.current?.setCaret(line, col || 1);
                              });
                            }}
                          />
                        );
                      case 'tree':
                        return (
                          <NumkitASTTreeView
                            source={activeTabData?.code || ''}
                            engine={engine}
                            cursorLine={editorCursor.line}
                            onNavigate={(line, col) => {
                              ensureEditorPane('text');
                              queueMicrotask(() => {
                                editorBodyRef.current?.setCaret(line, col || 1);
                              });
                            }}
                          />
                        );
                      default: return null;
                    }
                  };
                  // Normalise the per-pane flex-grow fractions so they always
                  // sum to the pane count (>= 1). Without this a single pane left
                  // with a persisted fraction < 1 — e.g. 0.69 after dragging a
                  // split divider then closing the other pane — fails to fill the
                  // row: when the grow values sum to < 1, CSS distributes only
                  // that fraction of the free space and leaves the rest as dead
                  // space on the right. Scaling by paneCount/total keeps the
                  // user's drag ratios intact for real splits.
                  const fracTotal = active.reduce((s, k) => s + (paneFracs[k] || 1), 0) || 1;
                  const growFor = (key) => ((paneFracs[key] || 1) * active.length) / fracTotal;
                  return (
                    <div className="editor-multi-pane" ref={multiPaneRef}>
                      {active.map((key, i) => (
                        <Fragment key={key}>
                          {i > 0 && (
                            <ResizeHandle orientation="vertical"
                              onResize={onDragPaneBorder(active[i - 1], key)}
                              onDoubleClick={resetPaneFracs} />
                          )}
                          <div className="editor-pane"
                               style={{ flex: `${growFor(key)} 1 0` }}>
                            {renderPane(key)}
                          </div>
                        </Fragment>
                      ))}
                    </div>
                  );
                })()}
              </div>
            )}
            {panels.editor && panels.terminal && (
              <ResizeHandle orientation="horizontal"
                onResize={(_dx, dy) => setDockH((h) => Math.max(80, Math.min(700, h - dy)))}
                onDoubleClick={() => setDockH(280)} />
            )}
            {panels.terminal && (
              <BottomDock
                active={bottomTab}
                setActive={handleBottomTabChange}
                consoleNode={
                  <ConsolePane
                    engine={engine}
                    output={output}
                    onAddOutput={addOutput}
                    onRunCode={runCode}
                    helpTopic={helpTopic}
                    onSetHelpTopic={setHelpTopic}
                  />
                }
                workspaceVars={workspaceVars}
                onOpenVar={setOpenVar}
                onClose={() => setPanels((p) => ({ ...p, terminal: false }))}
              />
            )}
          </div>
        ) : <div className="ide-center is-empty" />}

        {panels.figures && (
          <ResizeHandle orientation="vertical"
            onResize={(dx) => setFiguresW((w) => Math.max(220, Math.min(800, w - dx)))}
            onDoubleClick={() => setFiguresW(440)} />
        )}
        {panels.figures && (
          <FiguresPane
            figures={adaptedFigures}
            unsupportedCount={droppedFigures}
            onExpand={(fig) => setOpenFigure(fig)}
            onCloseFigure={handleCloseFigure}
            onCloseAll={handleCloseAllFigures}
            engine={engine}
          />
        )}
      </div>

      <StatusBar
        engineStatus={status}
        activeTabName={activeTabData?.name}
        activeTabSource={activeTabData?.vfsPath ? activeTabData?.source : null}
        figureCount={figures.length}
        outputCount={output.length}
        execTimeMs={execTimeMs}
        buildVersion={engineVersion}
        breakpointCount={activeBreakpoints.length}
        debugLine={debugState?.line ?? null}
      />

      {openVar && (
        <VariableEditor variable={openVar} engine={engine}
          onClose={() => setOpenVar(null)} />
      )}
      {openFigure && (
        <FigureWindow figure={openFigure} engine={engine}
          onClose={() => setOpenFigure(null)} />
      )}
      {prefsOpen && (
        <PreferencesModal onClose={() => setPrefsOpen(false)} />
      )}

      {showSaveDialog && (
        <div onClick={() => setShowSaveDialog(false)}
          style={{
            position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.6)',
            display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 1000,
          }}>
          <div onClick={(e) => e.stopPropagation()}
            style={{
              background: 'var(--bg-2)', border: '1px solid var(--line)',
              borderRadius: 8, padding: 20, width: 320,
              boxShadow: '0 8px 32px rgba(0,0,0,0.5)', color: 'var(--fg-1)',
            }}>
            <div style={{ fontSize: 13, fontWeight: 600, marginBottom: 12 }}>Save to Local Files</div>
            <input value={saveFileName}
              onChange={(e) => setSaveFileName(e.target.value)}
              autoFocus
              onKeyDown={(e) => {
                if (e.key === 'Enter') handleSaveDialogSubmit();
                if (e.key === 'Escape') setShowSaveDialog(false);
              }}
              placeholder="filename"
              style={{
                width: '100%', padding: '8px 10px', borderRadius: 5, fontSize: 12,
                background: 'var(--bg-0)', border: '1px solid var(--line)',
                color: 'var(--fg-1)', outline: 'none', marginBottom: 12, boxSizing: 'border-box',
              }} />
            <div style={{ display: 'flex', gap: 8, justifyContent: 'flex-end' }}>
              <button onClick={() => setShowSaveDialog(false)}
                style={{
                  padding: '6px 14px', borderRadius: 5, fontSize: 11,
                  background: 'var(--bg-3)', border: '1px solid var(--line)',
                  color: 'var(--fg-2)', cursor: 'pointer',
                }}>Cancel</button>
              <button onClick={handleSaveDialogSubmit}
                style={{
                  padding: '6px 14px', borderRadius: 5, fontSize: 11, fontWeight: 600,
                  background: 'var(--accent)', border: 'none', color: '#fff', cursor: 'pointer',
                }}>Save</button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
