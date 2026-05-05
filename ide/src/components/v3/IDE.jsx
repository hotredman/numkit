import { useCallback, useEffect, useMemo, useRef, useState } from 'react';

import SyntaxEditor from '../SyntaxEditor';
import OldFigures from '../Figures';
import Sidebar from './Sidebar';
import ConsolePane from './ConsolePane';

import Toolbar from './Toolbar';
import StatusBar from './StatusBar';
import ResizeHandle from './ResizeHandle';
import { WorkspacePanel, VariableEditor } from './Workspace';
import ReferencePanel from './Reference';
import { ALL_DOCS } from './refData';
import FiguresPane from './FiguresPane';
import FigureWindow from './FigureWindow';
import { adaptVariables, adaptFigures } from './adapters';

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

  return (
    <div className="editor-tabs">
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
      <div className="editor-actions">
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
function EditorBody({ activeTab, errorLine, debugLine, breakpoints, onToggleBp, onChange }) {
  const editorRef = useRef(null);
  const gutterRef = useRef(null);

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
          errorLine={errorLine}
          debugLine={debugLine}
        />
      </div>
    </div>
  );
}

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
        <div style={{ display: active === 'console' ? 'flex' : 'none', flex: 1, overflow: 'hidden', flexDirection: 'column' }}>
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
  const [consoleNotify, setConsoleNotify] = useState(false);
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
  const [showLegacyFigures, setShowLegacyFigures] = useState(false);

  const mountedRef = useRef(true);
  useEffect(() => () => { mountedRef.current = false; }, []);
  const warnedFallbackRef = useRef(false);
  useEffect(() => { if (vfsAdapters?.local) warnedFallbackRef.current = false; }, [vfsAdapters?.local]);

  // Engine version banner
  const [engineVersion, setEngineVersion] = useState(null);
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
  const addOutput = useCallback((items) => {
    setOutput((prev) => {
      for (const i of items) if (i.text === '__CLEAR__') return [];
      return [...prev, ...items.filter((i) => i.text !== '__CLEAR__')];
    });
  }, []);

  const runCode = useCallback(async (code) => {
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
    if (origin) engine.pushScriptOrigin(origin, scriptDir);
    let result;
    const t0 = performance.now();
    try { result = engine.execute(code); }
    finally { if (origin) engine.popScriptOrigin(); }
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
        return Array.from(map.values());
      });
      setPanels((p) => ({ ...p, figures: true }));
    }
    if (result.errorLine) setErrorLine(result.errorLine);
    setVariables(engine.getVars());

    if (adapter) adapter.flush().then(() => {
      if (mountedRef.current) setVfsRefreshKey((k) => k + 1);
    });
  }, [engine, addOutput, tabs, activeTab, vfsAdapters]);

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
        return Array.from(map.values());
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
      setVariables(engine.getVars());
    } else if (result.status === 'error') {
      setDebugLine(null);
      setDebugState(null);
      if (result.line) setErrorLine(result.line);
      addOutput([{ type: 'error', text: `Error: ${result.message}` }]);
      setConsoleNotify(true);
      setVariables(engine.getVars());
    }
  }, [engine, addOutput]);

  const debugStart = useCallback(() => {
    const tab = tabs.find((t) => t.id === activeTab);
    if (!tab || !tab.code.trim()) return;
    setPanels((p) => ({ ...p, terminal: true }));
    setErrorLine(null);
    addOutput([{ type: 'system', text: `── Debug ${tab.name} ──` }]);
    setConsoleNotify(true);
    const t0 = performance.now();
    const result = engine.debugStart(tab.code);
    setExecTimeMs(performance.now() - t0);
    handleDebugResult(result);
  }, [tabs, activeTab, engine, addOutput, handleDebugResult]);

  const debugResume = useCallback((action = 0) => {
    if (!debugState || debugState.status !== 'paused') return;
    const result = engine.debugResume(action);
    handleDebugResult(result);
  }, [debugState, engine, handleDebugResult]);

  const debugStop = useCallback(() => {
    engine.debugStop?.();
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
    addOutput([
      { type: 'system', text: `── Running ${tab.name} ──` },
      { type: 'input',  text: tab.code },
    ]);
    setConsoleNotify(true);
    runCode(tab.code);
    setTabs((p) => p.map((t) => (t.id === activeTab ? { ...t, modified: false } : t)));
  }, [tabs, activeTab, addOutput, runCode]);

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

  // Ctrl+S
  useEffect(() => {
    const h = (e) => { if ((e.ctrlKey || e.metaKey) && e.key === 's') { e.preventDefault(); handleSave(); } };
    window.addEventListener('keydown', h);
    return () => window.removeEventListener('keydown', h);
  }, [handleSave]);

  /* ─────────────── adapted data ─────────────── */
  const workspaceVars = useMemo(() => adaptVariables(variables), [variables]);
  const adaptedFigures = useMemo(() => adaptFigures(figures), [figures]);

  // Notify the user once (non-blocking) that figures with non-line plots
  // exist but the new viewer can only show line/scatter — they should use
  // the right pane's classic renderer for those. Cheap: only fires when
  // count drops between adaptation passes.
  const droppedFigures = figures.length - adaptedFigures.length;

  const handleCloseFigure = useCallback((id) => {
    engine.execute(`close(${id})`);
  }, [engine]);
  const handleCloseAllFigures = useCallback(() => {
    engine.execute("close('all')");
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
  return (
    <div className="ide">
      <Toolbar
        panels={panels}
        togglePanel={togglePanel}
        theme={themeName}
        onToggleTheme={toggleTheme}
        onRun={runActiveTab}
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
        isDebugging={isDebugging}
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
                <EditorBody
                  activeTab={activeTabData}
                  errorLine={errorLine}
                  debugLine={debugLine}
                  breakpoints={activeBreakpoints}
                  onToggleBp={toggleBreakpoint}
                  onChange={updateTabCode}
                />
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
            onOpenLegacy={() => setShowLegacyFigures(true)}
            onExpand={(fig) => setOpenFigure(fig)}
            onCloseFigure={handleCloseFigure}
            onCloseAll={handleCloseAllFigures}
          />
        )}
      </div>

      <StatusBar
        engineStatus={status}
        activeTabName={activeTabData?.name}
        activeTabSource={activeTabData?.vfsPath ? activeTabData?.source : null}
        figureCount={figures.length}
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
        <FigureWindow figure={openFigure} onClose={() => setOpenFigure(null)} />
      )}

      {/* Legacy d3 renderer — full-screen modal for unsupported plot types */}
      {showLegacyFigures && (
        <div onClick={(e) => { if (e.target === e.currentTarget) setShowLegacyFigures(false); }}
          style={{
            position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.55)',
            display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 900,
          }}>
          <div style={{
            background: 'var(--bg-1)', border: '1px solid var(--line)',
            borderRadius: 10, width: '85vw', height: '80vh',
            display: 'flex', flexDirection: 'column', overflow: 'hidden',
            boxShadow: '0 20px 60px rgba(0,0,0,0.5)',
          }}>
            <div style={{
              display: 'flex', alignItems: 'center', justifyContent: 'space-between',
              padding: '8px 14px', borderBottom: '1px solid var(--line)',
              fontSize: 12, color: 'var(--fg-1)',
            }}>
              <span>Legacy figures (heatmap / surface / 3-D)</span>
              <button onClick={() => setShowLegacyFigures(false)}
                style={{ background: 'transparent', border: 'none', color: 'var(--fg-2)', cursor: 'pointer', fontSize: 18, lineHeight: 1 }}>×</button>
            </div>
            <div style={{ flex: 1, minHeight: 0 }}>
              <OldFigures
                figures={figures.filter((f) => !adaptedFigures.some((a) => a.id === f.id))}
                onSetFigures={setFigures}
                onCloseFigure={handleCloseFigure}
                onCloseAll={handleCloseAllFigures}
                onClose={() => setShowLegacyFigures(false)}
              />
            </div>
          </div>
        </div>
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
