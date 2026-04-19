import { useState, useEffect, useRef, useCallback, useMemo } from "react";
import FileBrowser from "./FileBrowser";
import Console from "./Console";
import Workspace from "./Workspace";
import Reference from "./Reference";
import Figures from "./Figures";
import SyntaxEditor from "./SyntaxEditor";
import tempFS from "../temporary";
import localFS from "../fs/local";
import { pickRunOrigin } from "../fs/run-origin";
import { loadUiState, saveUiState } from "../ui-state";
import { useTheme, FONT, FONT_UI } from "../theme";

// Stable reference for "no breakpoints" — keeps useMemo output
// reference-equal across renders when the active tab has none.
const EMPTY_BPS = Object.freeze([]);

function TabBar({ tabs, activeTab, onSelect, onClose, onNew, onRename, onCloseAll, onCloseExcept }) {
  const C = useTheme();
  const [editingId, setEditingId] = useState(null);
  const [editName, setEditName] = useState("");
  const [ctxMenu, setCtxMenu] = useState(null);
  const [canScrollL, setCanScrollL] = useState(false);
  const [canScrollR, setCanScrollR] = useState(false);
  const scrollRef = useRef(null);

  const checkScroll = useCallback(() => {
    const el = scrollRef.current; if (!el) return;
    setCanScrollL(el.scrollLeft > 2);
    setCanScrollR(el.scrollLeft + el.clientWidth < el.scrollWidth - 2);
  }, []);

  useEffect(() => {
    const el = scrollRef.current; if (!el) return;
    checkScroll();
    el.addEventListener('scroll', checkScroll);
    const ro = new ResizeObserver(checkScroll); ro.observe(el);
    return () => { el.removeEventListener('scroll', checkScroll); ro.disconnect(); };
  }, [checkScroll, tabs]);

  useEffect(() => {
    if (!ctxMenu) return;
    const h = () => setCtxMenu(null);
    window.addEventListener('mousedown', h);
    return () => window.removeEventListener('mousedown', h);
  }, [ctxMenu]);

  const scroll = dir => { const el = scrollRef.current; if (el) el.scrollBy({ left: dir * 120, behavior: 'smooth' }); };

  const arrowStyle = (enabled) => ({
    display: 'flex', alignItems: 'center', justifyContent: 'center',
    width: 22, height: 26, flexShrink: 0,
    background: C.bg0, border: 'none', cursor: enabled ? 'pointer' : 'default',
    color: enabled ? C.textDim : `${C.textMuted}44`, fontSize: 13, transition: 'color 0.15s',
  });

  return (
    <div style={{ display: 'flex', alignItems: 'center', background: C.bg0, borderBottom: `1px solid ${C.border}`, minHeight: 32, flexShrink: 0, position: 'relative' }}>
      <button onClick={() => scroll(-1)} style={arrowStyle(canScrollL)} disabled={!canScrollL}>◀</button>
      <div ref={scrollRef} className="tab-scroll" style={{ flex: 1, display: 'flex', alignItems: 'center', gap: 1, padding: '3px 2px', overflowX: 'auto', scrollbarWidth: 'none', msOverflowStyle: 'none' }}
        onWheel={e => { if (scrollRef.current) scrollRef.current.scrollLeft += e.deltaY; }}>
        <style>{`.tab-scroll::-webkit-scrollbar{display:none}`}</style>
        {tabs.map(tab => (
          <div key={tab.id}
            onClick={() => onSelect(tab.id)}
            onContextMenu={e => { e.preventDefault(); setCtxMenu({ x: e.clientX, y: e.clientY, tabId: tab.id }); }}
            style={{ display: 'flex', alignItems: 'center', gap: 4, padding: '3px 10px', borderRadius: 5, cursor: 'pointer', fontSize: 11, background: tab.id === activeTab ? C.bg3 : 'transparent', color: tab.id === activeTab ? C.text : C.textMuted, border: `1px solid ${tab.id === activeTab ? C.borderHi : 'transparent'}`, whiteSpace: 'nowrap', transition: 'all 0.15s', flexShrink: 0 }}
            onMouseEnter={e => { if (tab.id !== activeTab) e.currentTarget.style.background = C.bg2; }}
            onMouseLeave={e => { if (tab.id !== activeTab) e.currentTarget.style.background = 'transparent'; }}>
            {editingId === tab.id
              ? <input value={editName} autoFocus onChange={e => setEditName(e.target.value)} onBlur={() => { onRename(tab.id, editName); setEditingId(null); }} onKeyDown={e => { if (e.key === 'Enter') { onRename(tab.id, editName); setEditingId(null); } }} style={{ background: 'transparent', border: 'none', color: C.text, fontSize: 11, width: 80, outline: 'none', fontFamily: FONT }} onClick={e => e.stopPropagation()} />
              : <span onDoubleClick={e => { e.stopPropagation(); setEditingId(tab.id); setEditName(tab.name); }}>{tab.name}{tab.modified ? ' •' : ''}</span>}
            {tabs.length > 1 && <span onClick={e => { e.stopPropagation(); onClose(tab.id); }} style={{ color: C.textMuted, fontSize: 13, lineHeight: 1, marginLeft: 2, opacity: 0.5 }} onMouseEnter={e => e.currentTarget.style.opacity = 1} onMouseLeave={e => e.currentTarget.style.opacity = 0.5}>×</span>}
          </div>
        ))}
      </div>
      <button onClick={() => scroll(1)} style={arrowStyle(canScrollR)} disabled={!canScrollR}>▶</button>
      <button onClick={onNew} style={{ background: 'none', border: `1px dashed ${C.border}`, borderRadius: 5, color: C.textMuted, fontSize: 13, padding: '1px 8px', cursor: 'pointer', lineHeight: 1, marginRight: 8, flexShrink: 0 }}>+</button>
      {ctxMenu && (
        <div onMouseDown={e => e.stopPropagation()} style={{ position: 'fixed', left: ctxMenu.x, top: ctxMenu.y, zIndex: 1000, background: C.bg3, border: `1px solid ${C.borderHi}`, borderRadius: 5, boxShadow: '0 4px 16px rgba(0,0,0,0.35)', minWidth: 160, padding: '4px 0' }}>
          {[
            { label: '📄 New Script', action: () => onNew() },
            { sep: true },
            { label: '✏️ Rename', action: () => { setEditingId(ctxMenu.tabId); setEditName(tabs.find(t => t.id === ctxMenu.tabId)?.name || ''); } },
            { sep: true },
            { label: '✕ Close', action: () => onClose(ctxMenu.tabId), disabled: tabs.length <= 1 },
            { label: '✕ Close All', action: () => onCloseAll() },
            { label: '✕ Close Others', action: () => onCloseExcept(ctxMenu.tabId), disabled: tabs.length <= 1 },
          ].map((item, i) => item.sep
            ? <div key={i} style={{ height: 1, background: C.border, margin: '3px 8px' }} />
            : <div key={i} onClick={() => { if (!item.disabled) { item.action(); setCtxMenu(null); } }}
                style={{ padding: '5px 12px', fontSize: 11, color: item.disabled ? C.textMuted : C.text, cursor: item.disabled ? 'default' : 'pointer', opacity: item.disabled ? 0.4 : 1 }}
                onMouseEnter={e => { if (!item.disabled) e.currentTarget.style.background = C.bg4; }}
                onMouseLeave={e => e.currentTarget.style.background = 'transparent'}>
                {item.label}
              </div>
          )}
        </div>
      )}
    </div>
  );
}

export default function REPL({ engine: engineProp, status: statusProp, vfsAdapters, onLocalMount }) {
  const C = useTheme();
  const { themeName, toggleTheme } = C;

  // Rehydrate once from localStorage. Kept in useState so React's
  // lazy-init guarantee prevents re-reads on every render.
  const [savedState] = useState(() => loadUiState());

  const [showLeft, setShowLeft] = useState(() => savedState?.layout?.showLeft ?? true);
  const [showCenter, setShowCenter] = useState(() => savedState?.layout?.showCenter ?? true);
  const [showRight, setShowRight] = useState(() => savedState?.layout?.showRight ?? false);
  const [showBottom, setShowBottom] = useState(() => savedState?.layout?.showBottom ?? true);
  const [bottomTab, setBottomTab] = useState("console");
  const [bottomHeight, setBottomHeight] = useState(() => savedState?.layout?.bottomHeight ?? 300);
  const [output, setOutput] = useState([]);
  const [figures, setFigures] = useState([]);
  const [helpTopic, setHelpTopic] = useState(null);
  const [execTimeMs, setExecTimeMs] = useState(null);
  const [variables, setVariables] = useState({});
  const [errorLine, setErrorLine] = useState(null);
  // Tabs own their breakpoints (sorted number[]). Single source of
  // truth — persistence, rendering and engine-sync all read from the
  // tab object. One-time migration from the previous schema
  // (breakpointsByPath) merges its entries into the matching tabs so
  // users upgrading across this commit keep their red dots.
  const [tabs, setTabs] = useState(() => {
    const t = savedState?.tabs;
    const legacyByPath = savedState?.breakpointsByPath;
    const normalize = x => {
      let bps = Array.isArray(x.breakpoints) ? x.breakpoints.slice() : [];
      if (legacyByPath && x.vfsPath && Array.isArray(legacyByPath[x.vfsPath])) {
        bps = Array.from(new Set([...bps, ...legacyByPath[x.vfsPath]]));
      }
      bps.sort((a, b) => a - b);
      return { ...x, breakpoints: bps };
    };
    if (Array.isArray(t) && t.length > 0) return t.map(normalize);
    return [{id:"1",name:"untitled.m",code:"",modified:false,vfsPath:null,source:null,breakpoints:[]}];
  });
  const [activeTab, setActiveTab] = useState(() => {
    const t = savedState?.tabs;
    const a = savedState?.activeTab;
    if (Array.isArray(t) && a && t.some(x => x.id === a)) return a;
    if (Array.isArray(t) && t.length > 0) return t[0].id;
    return "1";
  });
  const [vfsRefreshKey, setVfsRefreshKey] = useState(0);
  const [showSaveDialog, setShowSaveDialog] = useState(false);
  const [saveFileName, setSaveFileName] = useState("");
  const [consoleNotify, setConsoleNotify] = useState(false);
  const [figuresWidth, setFiguresWidth] = useState(() => savedState?.layout?.figuresWidth ?? 360);

  // ── Debug state ──
  const [debugState, setDebugState] = useState(null);  // null | { status, line, variables }
  const [debugLine, setDebugLine] = useState(null);     // current paused line

  const tabCountRef=useRef(
    Array.isArray(savedState?.tabs) && savedState.tabs.length > 0
      ? Math.max(...savedState.tabs.map(x => parseInt(x.id, 10) || 0), 1)
      : 1
  );
  const editorRef=useRef(null); const gutterRef=useRef(null); const consoleRef=useRef(null); const resizingRef=useRef(false); const resizingRightRef=useRef(false);
  // Lives for the component lifetime. Used to guard async setState calls
  // from adapter.flush().then(...) after an unmount.
  const mountedRef = useRef(true);
  useEffect(() => () => { mountedRef.current = false; }, []);
  // Fires once per "local-folder is unavailable" streak — avoids stacking
  // identical warnings when the user re-Runs before mounting. Reset when
  // local becomes available so a later unmount re-arms the warning.
  const warnedFallbackRef = useRef(false);
  useEffect(() => { if (vfsAdapters?.local) warnedFallbackRef.current = false; }, [vfsAdapters?.local]);
  const engine = engineProp;

  useEffect(()=>{setOutput([{type:"system",text:"numkit mIDE v2.5"},{type:"system",text:'Type commands below. "help <topic>" for function info.'}]);},[]);

  const addOutput=useCallback(items=>{setOutput(prev=>{for(const i of items)if(i.text==="__CLEAR__")return[];return[...prev,...items.filter(i=>i.text!=="__CLEAR__")];});},[]);

  const runCode=useCallback(code=>{
    const activeTabObj = tabs.find(t => t.id === activeTab);
    const { adapter, origin, fallbackUsed } = pickRunOrigin(activeTabObj?.source, vfsAdapters);

    // Tell the user (once per "local-unavailable" streak) when a run
    // from a Local-Folder tab is silently routed to Temporary, so they
    // know why their file landed in the wrong tree.
    if (fallbackUsed && !warnedFallbackRef.current) {
      addOutput([{
        type: 'warning',
        text: 'Warning: Local Folder not mounted — file I/O for this run will go to Temporary. Mount a folder from the File Browser to persist writes to disk.',
      }]);
      setConsoleNotify(true);
      warnedFallbackRef.current = true;
    }

    if (origin) engine.pushScriptOrigin(origin);
    let result;
    const t0=performance.now();
    try { result = engine.execute(code); }
    finally { if (origin) engine.popScriptOrigin(); }
    setExecTimeMs(performance.now()-t0);setErrorLine(null);
    const items=[];if(result.output)for(const line of result.output.split("\n")){if(line==="__CLEAR__"){setOutput([]);continue;}items.push({type:/^Error/.test(line)?"error":/^Warning:/.test(line)?"warning":"result",text:line});}
    if(items.length){addOutput(items);setConsoleNotify(true);}
    if(result.closeAllFigures)setFigures([]);else if(result.closedFigureIds?.length){const closed=new Set(result.closedFigureIds);setFigures(prev=>prev.filter(f=>!closed.has(f.id)));}
    if(result.figures?.length){setFigures(prev=>{const map=new Map(prev.map(f=>[f.id,f]));for(const fig of result.figures)map.set(fig.id,fig);return Array.from(map.values());});setShowRight(true);}
    if(result.errorLine)setErrorLine(result.errorLine);
    setVariables(engine.getVars());
    // If the script wrote to tempFS/local via the sync mirror, the disk
    // persist is still in flight; wait for it before refreshing the File
    // Browser so newly-created files show up on the first render. Guarded
    // by mountedRef so an unmount mid-flush doesn't setState on a dead
    // component.
    if (adapter) adapter.flush().then(() => {
      if (mountedRef.current) setVfsRefreshKey(k => k + 1);
    });
  },[engine,addOutput,tabs,activeTab,vfsAdapters]);

  // ── Debug actions ──
  // Toggle a red dot on the active tab. Breakpoints are stored sorted
  // inside the tab object — same array reference survives keystrokes
  // (updateTabCode spreads but doesn't clone breakpoints), so the
  // engine-sync effect below doesn't refire on every character typed.
  const toggleBreakpoint = useCallback((line) => {
    setTabs(prev => prev.map(t => {
      if (t.id !== activeTab) return t;
      const has = t.breakpoints.includes(line);
      const next = has
        ? t.breakpoints.filter(l => l !== line)
        : [...t.breakpoints, line].sort((a, b) => a - b);
      return { ...t, breakpoints: next };
    }));
  }, [activeTab]);

  // Active tab's breakpoint list as a stable reference. Becomes a new
  // array only when the user actually toggles one, so the engine-sync
  // effect fires exactly when it needs to.
  const activeBreakpoints = useMemo(() => {
    const tab = tabs.find(t => t.id === activeTab);
    return tab?.breakpoints || EMPTY_BPS;
  }, [tabs, activeTab]);

  // Keep the engine's breakpoint manager in lockstep with the active
  // tab. Without this sync, a breakpoint removed while paused would
  // keep firing until the next Continue.
  useEffect(() => {
    engine.debugSetBreakpoints(activeBreakpoints);
  }, [activeBreakpoints, engine]);

  // Persist UI state (tabs, active tab, layout) to localStorage —
  // debounced inside saveUiState so typing/resizing doesn't hammer
  // the main thread. Breakpoints ride along inside each tab object.
  //
  // The first invocation is a no-op: React fires this effect right
  // after mount with no user-visible change vs. what we just loaded,
  // so writing would just echo disk back to disk.
  const initialSaveSkippedRef = useRef(false);
  useEffect(() => {
    if (!initialSaveSkippedRef.current) { initialSaveSkippedRef.current = true; return; }
    saveUiState({
      layout: { showLeft, showCenter, showRight, showBottom, figuresWidth, bottomHeight },
      tabs,
      activeTab,
    });
  }, [tabs, activeTab, showLeft, showCenter, showRight, showBottom, figuresWidth, bottomHeight]);

  // Handle debug result from start or resume
  const handleDebugResult = useCallback((result) => {
    if (result.output) {
      const items = result.output.split("\n").map(line => ({
        type: /^Error/.test(line) ? "error" : "result",
        text: line,
      }));
      addOutput(items);
      setConsoleNotify(true);
    }

    // Process figures from debug output (same as normal execute path)
    if (result.closeAllFigures) setFigures([]);
    else if (result.closedFigureIds?.length) {
      const closed = new Set(result.closedFigureIds);
      setFigures(prev => prev.filter(f => !closed.has(f.id)));
    }
    if (result.figures?.length) {
      setFigures(prev => {
        const map = new Map(prev.map(f => [f.id, f]));
        for (const fig of result.figures) map.set(fig.id, fig);
        return Array.from(map.values());
      });
      setShowRight(true);
    }

    if (result.status === 'paused' && result.pauseState) {
      const ps = result.pauseState;
      setDebugLine(ps.line);
      setDebugState({
        status: 'paused',
        line: ps.line,
        variables: ps.variables || {},
        reason: ps.reason,
      });
      addOutput([{
        type: "system",
        text: `\u23F8 Paused at line ${ps.line} (${ps.reason})`,
      }]);
      if (ps.variables && typeof ps.variables === 'object') {
        const debugVars = {};
        for (const [name, preview] of Object.entries(ps.variables))
          debugVars[name] = preview;
        setVariables(debugVars);
      }
    } else if (result.status === 'completed') {
      setDebugLine(null);
      setDebugState(null);
      addOutput([{ type: "system", text: "\u2713 Debug completed" }]);
      setConsoleNotify(true);
      setVariables(engine.getVars());
    } else if (result.status === 'error') {
      setDebugLine(null);
      setDebugState(null);
      if (result.line) setErrorLine(result.line);
      addOutput([{ type: "error", text: `Error: ${result.message}` }]);
      setConsoleNotify(true);
      setVariables(engine.getVars());
    }
  }, [engine, addOutput]);

  const debugStart = useCallback(() => {
    const tab = tabs.find(t => t.id === activeTab);
    if (!tab || !tab.code.trim()) return;

    setShowBottom(true);
    setErrorLine(null);
    addOutput([{ type: "system", text: `\u2500\u2500 Debug ${tab.name} \u2500\u2500` }]);
    setConsoleNotify(true);

    // Breakpoints are kept in sync via the useEffect above — no need to
    // push them here too.
    const t0 = performance.now();
    const result = engine.debugStart(tab.code);
    setExecTimeMs(performance.now() - t0);
    handleDebugResult(result);
  }, [tabs, activeTab, engine, addOutput, handleDebugResult]);

  // Resume with a DebugAction: 0=Continue, 1=StepOver, 2=StepInto, 3=StepOut
  const debugResume = useCallback((action = 0) => {
    if (!debugState || debugState.status !== 'paused') return;
    const result = engine.debugResume(action);
    handleDebugResult(result);
  }, [debugState, engine, handleDebugResult]);

  const debugStop = useCallback(() => {
    engine.debugStop?.();
    setDebugLine(null);
    setDebugState(null);
    addOutput([{ type: "system", text: "\u25A0 Debug stopped" }]);
    setConsoleNotify(true);
  }, [engine, addOutput]);

  const handleBottomTabChange=useCallback(id=>{setBottomTab(id);if(id==="console")setConsoleNotify(false);},[]);
  const handleCloseFigure=useCallback(id=>{engine.execute(`close(${id})`);},[engine]);
  const handleCloseAllFigures=useCallback(()=>{engine.execute("close('all')");},[engine]);

  const newTab=useCallback(()=>{tabCountRef.current++;const id=String(tabCountRef.current);setTabs(p=>[...p,{id,name:`script${tabCountRef.current}.m`,code:"",modified:false,vfsPath:null,source:null,breakpoints:[]}]);setActiveTab(id);},[]);
  const closeTab=useCallback(id=>{setTabs(p=>{const n=p.filter(t=>t.id!==id);if(!n.length)return p;if(activeTab===id)setActiveTab(n[n.length-1].id);return n;});},[activeTab]);
  const closeAllTabs=useCallback(()=>{tabCountRef.current++;const id=String(tabCountRef.current);setTabs([{id,name:'untitled.m',code:'',modified:false,vfsPath:null,source:null,breakpoints:[]}]);setActiveTab(id);},[]);
  const closeOtherTabs=useCallback(id=>{setTabs(p=>{const keep=p.find(t=>t.id===id);return keep?[keep]:p;});setActiveTab(id);},[]);
  const renameTab=useCallback((id,name)=>{if(!name.trim())return;setTabs(p=>p.map(t=>t.id===id?{...t,name:name.trim()}:t));},[]);
  const activeTabData=tabs.find(t=>t.id===activeTab)||tabs[0];
  const updateTabCode=useCallback(code=>{setTabs(p=>p.map(t=>t.id===activeTab?{...t,code,modified:true}:t));},[activeTab]);

  const runActiveTab=useCallback(()=>{const tab=tabs.find(t=>t.id===activeTab);if(!tab||!tab.code.trim())return;setShowBottom(true);setDebugLine(null);setDebugState(null);addOutput([{type:"system",text:`── Running ${tab.name} ──`},{type:"input",text:tab.code}]);setConsoleNotify(true);runCode(tab.code);setTabs(p=>p.map(t=>t.id===activeTab?{...t,modified:false}:t));},[tabs,activeTab,addOutput,runCode]);

  const handleOpenFile=useCallback((filename,content,vfsPath,source)=>{const existing=tabs.find(t=>t.vfsPath&&t.vfsPath===vfsPath);if(existing){setActiveTab(existing.id);return;}tabCountRef.current++;const id=String(tabCountRef.current);setTabs(p=>[...p,{id,name:filename,code:content,modified:false,vfsPath:vfsPath||null,source:source||null,breakpoints:[]}]);setActiveTab(id);setShowCenter(true);},[tabs]);
  // Save routes the write to the same filesystem the file came from.
  // Tabs opened from Local Folder write back to disk; everything else
  // (new tabs, tabs from Temporary, etc.) defaults to Temporary FS.
  const handleSaveToFS=useCallback(async(path,name)=>{const tab=tabs.find(t=>t.id===activeTab);if(!tab)return;const fullPath=path||tab.vfsPath;if(!fullPath)return;const targetSource=tab.source==='localFolder'?'localFolder':'temporary';try{if(targetSource==='localFolder')await localFS.writeFile(fullPath,tab.code);else await tempFS.writeFile(fullPath,tab.code);}catch(e){addOutput([{type:"error",text:`Save failed: ${e?.message||e}`}]);setConsoleNotify(true);return;}setTabs(p=>p.map(t=>t.id===activeTab?{...t,modified:false,vfsPath:fullPath,name:name||t.name,source:targetSource}:t));setVfsRefreshKey(k=>k+1);addOutput([{type:"system",text:`Saved ${name||tab.name}`}]);setConsoleNotify(true);},[tabs,activeTab,addOutput]);
  const handleSave=useCallback(()=>{const tab=tabs.find(t=>t.id===activeTab);if(!tab)return;if(tab.vfsPath)handleSaveToFS(tab.vfsPath,tab.name);else{setSaveFileName(tab.name);setShowSaveDialog(true);}},[tabs,activeTab,handleSaveToFS]);
  const handleSaveDialogSubmit=useCallback(async()=>{if(!saveFileName.trim())return;let name=saveFileName.trim();if(!name.includes('.'))name+='.m';await handleSaveToFS(`/${name}`,name);setShowSaveDialog(false);},[saveFileName,handleSaveToFS]);
  // Narrow predicate the FileBrowser uses to warn about unsaved tab content
  // when the user asks to download a file. Keeps FileBrowser free of
  // tab-state knowledge beyond "is the (source,path) pair currently dirty?".
  // Both Temporary and Local Folder can have a file at "/foo.m"; the
  // source discriminator keeps their warnings independent.
  const isTabUnsaved=useCallback((path,source)=>tabs.some(t=>t.vfsPath===path&&t.source===source&&t.modified),[tabs]);

  useEffect(()=>{const h=e=>{if((e.ctrlKey||e.metaKey)&&e.key==='s'){e.preventDefault();handleSave();}};window.addEventListener('keydown',h);return()=>window.removeEventListener('keydown',h);},[handleSave]);
  const handleResizeStart=useCallback(e=>{e.preventDefault();resizingRef.current=true;const sY=e.clientY,sH=bottomHeight;const onM=ev=>{if(!resizingRef.current)return;setBottomHeight(Math.max(100,Math.min(window.innerHeight*0.7,sH+sY-ev.clientY)));};const onU=()=>{resizingRef.current=false;document.removeEventListener('mousemove',onM);document.removeEventListener('mouseup',onU);};document.addEventListener('mousemove',onM);document.addEventListener('mouseup',onU);},[bottomHeight]);
  const handleRightResizeStart=useCallback(e=>{e.preventDefault();resizingRightRef.current=true;const sX=e.clientX,sW=figuresWidth;const onM=ev=>{if(!resizingRightRef.current)return;setFiguresWidth(Math.max(200,Math.min(window.innerWidth*0.5,sW+sX-ev.clientX)));};const onU=()=>{resizingRightRef.current=false;document.removeEventListener('mousemove',onM);document.removeEventListener('mouseup',onU);};document.addEventListener('mousemove',onM);document.addEventListener('mouseup',onU);},[figuresWidth]);
  const handleEditorScroll=useCallback(scrollTop=>{if(gutterRef.current)gutterRef.current.scrollTop=scrollTop;},[]);

  const isDebugging = debugState?.status === 'paused';

  const PanelBtn=({active,onClick,icon,label,title,notify})=>(<button onClick={onClick} title={title} style={{display:"flex",alignItems:"center",gap:4,padding:"4px 8px",border:"none",borderRadius:4,background:active?`${C.accent}25`:"transparent",color:active?C.accent:C.textMuted,fontFamily:FONT_UI,fontSize:11,fontWeight:500,cursor:"pointer",transition:"all 0.15s",whiteSpace:"nowrap",position:"relative"}} onMouseEnter={e=>{if(!active)e.currentTarget.style.background=C.bg3;}} onMouseLeave={e=>{if(!active)e.currentTarget.style.background="transparent";}}><span style={{fontSize:13}}>{icon}</span>{label}{notify&&<span style={{width:6,height:6,borderRadius:"50%",background:C.green,position:"absolute",top:2,right:2}}/>}</button>);
  const ActBtn=({onClick,icon,label,color,title,disabled})=>(<button onClick={onClick} title={title} disabled={disabled} style={{display:"flex",alignItems:"center",gap:3,padding:"4px 8px",border:`1px solid ${C.border}`,borderRadius:4,background:C.bg2,color:color||C.textDim,fontFamily:FONT_UI,fontSize:11,fontWeight:500,cursor:disabled?"default":"pointer",transition:"all 0.15s",whiteSpace:"nowrap",opacity:disabled?0.4:1}} onMouseEnter={e=>{if(!disabled)e.currentTarget.style.borderColor=C.borderHi;}} onMouseLeave={e=>e.currentTarget.style.borderColor=C.border}><span style={{fontSize:12}}>{icon}</span>{label}</button>);
  const bottomTabBtn=(id,label,notify)=>(<button onClick={()=>handleBottomTabChange(id)} style={{padding:"5px 12px",border:"none",borderBottom:bottomTab===id?`2px solid ${C.accent}`:"2px solid transparent",background:"transparent",color:notify&&bottomTab!==id?C.green:bottomTab===id?C.text:C.textMuted,fontFamily:FONT_UI,fontSize:11,fontWeight:(notify&&bottomTab!==id)||bottomTab===id?600:400,cursor:"pointer",transition:"all 0.15s"}}>{label}</button>);

  return (
    <div style={{display:"flex",flexDirection:"column",height:"100vh",width:"100%",background:C.bg0,color:C.text,fontFamily:FONT,fontSize:13,overflow:"hidden"}}>
      <div style={{display:"flex",alignItems:"center",justifyContent:"space-between",padding:"5px 12px",background:C.bg1,borderBottom:`1px solid ${C.border}`,flexShrink:0,zIndex:30,gap:8}}>
        <div style={{display:"flex",alignItems:"baseline",gap:6,flexShrink:0}}><span style={{fontSize:17,fontWeight:600,letterSpacing:-0.5,fontFamily:"'Fraunces', 'Times New Roman', serif"}}>[n<sub style={{fontSize:"0.7em",verticalAlign:"sub"}}>k</sub>]</span><span style={{fontSize:15,fontWeight:700,letterSpacing:-0.5,fontFamily:FONT_UI,color:C.accent}}>mIDE</span><span style={{fontSize:9,color:C.textMuted}}>v2.5</span></div>
        <div style={{display:"flex",gap:2,alignItems:"center",background:C.bg0,borderRadius:6,padding:"2px 3px"}}>
          <PanelBtn active={showLeft} onClick={()=>setShowLeft(!showLeft)} icon="📂" label="Explorer" title="File Browser"/>
          <PanelBtn active={showCenter} onClick={()=>setShowCenter(!showCenter)} icon="📝" label="Editor" title="Code Editor"/>
          <PanelBtn active={showRight} onClick={()=>setShowRight(!showRight)} icon="📊" label="Figures" title="Plot Figures" notify={figures.length>0&&!showRight}/>
          <PanelBtn active={showBottom} onClick={()=>setShowBottom(!showBottom)} icon="💻" label="Terminal" title="Bottom Panel"/>
        </div>
        <div style={{display:"flex",gap:3,flexShrink:0}}>
          {showCenter&&<ActBtn onClick={runActiveTab} icon="▶" label="Run" color={C.green} title="Run current script" disabled={isDebugging}/>}
          {showCenter&&!isDebugging&&<ActBtn onClick={debugStart} icon="🐛" label="Debug" color={C.orange} title="Run to breakpoint"/>}
          {showCenter&&isDebugging&&<ActBtn onClick={debugStop} icon="⏹" label="Stop" color={C.red} title="Stop debugging"/>}
          {showCenter&&<ActBtn onClick={handleSave} icon="💾" label="Save" title="Ctrl+S"/>}
          <ActBtn onClick={()=>setOutput([])} icon="🗑" label="Clear" title="Clear console"/>
          <ActBtn onClick={()=>{engine.reset();setVariables({});setDebugLine(null);setDebugState(null);addOutput([{type:"system",text:"Workspace cleared."}]);setConsoleNotify(true);}} icon="🔄" label="Reset" title="Reset workspace"/>
          <button onClick={toggleTheme} title={`Switch to ${themeName==='dark'?'light':'dark'} theme`}
            style={{display:"flex",alignItems:"center",gap:3,padding:"4px 8px",border:`1px solid ${C.border}`,borderRadius:4,background:C.bg2,color:C.textDim,fontFamily:FONT_UI,fontSize:11,fontWeight:500,cursor:"pointer",whiteSpace:"nowrap"}}>
            <span style={{fontSize:12}}>{themeName==='dark'?'☀️':'🌙'}</span>{themeName==='dark'?'Light':'Dark'}
          </button>
        </div>
      </div>

      {/* Debug toolbar - shown when paused */}
      {isDebugging && (
        <div style={{display:"flex",alignItems:"center",gap:8,padding:"4px 12px",background:`${C.orange}15`,borderBottom:`1px solid ${C.orange}40`,flexShrink:0}}>
          <span style={{fontSize:11,color:C.orange,fontWeight:600,fontFamily:FONT_UI}}>⏸ Paused at line {debugState.line}</span>
          <span style={{fontSize:10,color:C.textMuted}}>({debugState.reason})</span>
          <div style={{flex:1}}/>
          <button onClick={()=>debugResume(2)} style={{padding:"3px 10px",borderRadius:4,fontSize:10,fontWeight:600,background:C.blue||"#5c9fd6",color:"#fff",border:"none",cursor:"pointer",fontFamily:FONT_UI}} title="Step Into (F11)">↓ Into</button>
          <button onClick={()=>debugResume(1)} style={{padding:"3px 10px",borderRadius:4,fontSize:10,fontWeight:600,background:C.blue||"#5c9fd6",color:"#fff",border:"none",cursor:"pointer",fontFamily:FONT_UI}} title="Step Over (F10)">→ Over</button>
          <button onClick={()=>debugResume(3)} style={{padding:"3px 10px",borderRadius:4,fontSize:10,fontWeight:600,background:C.blue||"#5c9fd6",color:"#fff",border:"none",cursor:"pointer",fontFamily:FONT_UI}} title="Step Out (Shift+F11)">↑ Out</button>
          <button onClick={()=>debugResume(0)} style={{padding:"3px 10px",borderRadius:4,fontSize:10,fontWeight:600,background:C.cyan,color:"#fff",border:"none",cursor:"pointer",fontFamily:FONT_UI}} title="Continue (F5)">▶ Continue</button>
        </div>
      )}

      <div style={{flex:1,display:"flex",flexDirection:"column",overflow:"hidden"}}>
        <div style={{flex:1,display:"flex",overflow:"hidden",minHeight:0}}>
          {showLeft&&<div style={{width:280,minWidth:220,flexShrink:0,background:C.bg1,borderRight:`1px solid ${C.border}`,display:"flex",flexDirection:"column",overflow:"hidden"}}><FileBrowser onOpenFile={handleOpenFile} defaultGitHubRepo="skynetjudgmentday/mlab-demo" vfsRefreshKey={vfsRefreshKey} isTabUnsaved={isTabUnsaved} onLocalMount={onLocalMount}/></div>}
          {showCenter&&<div style={{flex:1,display:"flex",flexDirection:"column",overflow:"hidden",minWidth:0}}>
            <TabBar tabs={tabs} activeTab={activeTab} onSelect={setActiveTab} onClose={closeTab} onNew={newTab} onRename={renameTab} onCloseAll={closeAllTabs} onCloseExcept={closeOtherTabs}/>
            <div style={{flex:1,display:"flex",overflow:"hidden",position:"relative"}}>
              {/* Gutter with breakpoints */}
              <div ref={gutterRef} style={{padding:"8px 0",background:C.bg0,borderRight:`1px solid ${C.border}`,userSelect:"none",minWidth:48,overflowY:"hidden",flexShrink:0}}>
                {(activeTabData?.code||"").split("\n").map((_,i)=>{
                  const ln=i+1;
                  const isErr=errorLine===ln;
                  const isDbg=debugLine===ln;
                  const hasBp=activeBreakpoints.includes(ln);
                  return (
                    <div key={i}
                      onClick={()=>toggleBreakpoint(ln)}
                      style={{
                        display:"flex",alignItems:"center",
                        fontSize:10,lineHeight:"20px",height:20,
                        cursor:"pointer",
                        color:isErr?C.red:isDbg?C.orange:C.textMuted,
                        background:isDbg?`${C.orange}20`:isErr?`${C.red}18`:"transparent",
                        fontWeight:isErr||isDbg?700:400,
                        position:"relative",
                      }}
                      onMouseEnter={e=>{if(!hasBp&&!isDbg)e.currentTarget.querySelector('.bp-dot').style.opacity='0.3';}}
                      onMouseLeave={e=>{if(!hasBp&&!isDbg)e.currentTarget.querySelector('.bp-dot').style.opacity='0';}}
                    >
                      {/* Breakpoint dot area */}
                      <span className="bp-dot" style={{
                        width:14,display:"flex",alignItems:"center",justifyContent:"center",flexShrink:0,
                        opacity:hasBp?1:0,transition:"opacity 0.1s",
                      }}>
                        <span style={{
                          width:8,height:8,borderRadius:"50%",
                          background:hasBp?C.red:`${C.red}80`,
                          display:"block",
                        }}/>
                      </span>
                      {/* Debug arrow */}
                      {isDbg && <span style={{fontSize:11,color:C.orange,marginRight:1}}>▶</span>}
                      {/* Line number */}
                      <span style={{flex:1,textAlign:"right",paddingRight:6}}>{ln}</span>
                    </div>
                  );
                })}
              </div>
              <div style={{flex:1,overflow:"hidden",background:C.bg1}}>
                <SyntaxEditor ref={editorRef} value={activeTabData?.code||""} onChange={val=>{updateTabCode(val);setErrorLine(null);}} onScroll={handleEditorScroll} errorLine={errorLine} debugLine={debugLine}/>
              </div>
            </div>
          </div>}
          {showRight&&<><div onMouseDown={handleRightResizeStart} style={{width:4,cursor:"ew-resize",background:C.border,flexShrink:0,transition:"background 0.15s"}} onMouseEnter={e=>e.currentTarget.style.background=C.accent} onMouseLeave={e=>{if(!resizingRightRef.current)e.currentTarget.style.background=C.border;}}/>
          <div style={{width:figuresWidth,minWidth:200,flexShrink:0,background:C.bg1,display:"flex",flexDirection:"column",overflow:"hidden"}}><Figures figures={figures} onSetFigures={setFigures} onCloseFigure={handleCloseFigure} onCloseAll={handleCloseAllFigures} onClose={()=>setShowRight(false)}/></div></>}
          {!showLeft&&!showCenter&&!showRight&&<div style={{flex:1,display:"flex",alignItems:"center",justifyContent:"center",color:C.textMuted,fontSize:12,fontFamily:FONT_UI}}>Toggle panels from the toolbar</div>}
        </div>

        {showBottom&&<div style={{height:bottomHeight,minHeight:100,display:"flex",flexDirection:"column",overflow:"hidden",flexShrink:0}}>
          <div onMouseDown={handleResizeStart} style={{height:4,cursor:"ns-resize",background:C.border,flexShrink:0,transition:"background 0.15s"}} onMouseEnter={e=>e.currentTarget.style.background=C.accent} onMouseLeave={e=>{if(!resizingRef.current)e.currentTarget.style.background=C.border;}}/>
          <div style={{display:"flex",alignItems:"center",background:C.bg0,borderBottom:`1px solid ${C.border}`,flexShrink:0,justifyContent:"space-between"}}>
            <div style={{display:"flex"}}>{bottomTabBtn("console","💻 Console",consoleNotify)}{bottomTabBtn("workspace","🔍 Workspace")}{bottomTabBtn("cheatsheet","📖 Reference")}</div>
            <button onClick={()=>setShowBottom(false)} style={{background:"none",border:"none",color:C.textMuted,cursor:"pointer",fontSize:16,padding:"0 10px",lineHeight:1}}>×</button>
          </div>
          <div style={{flex:1,overflow:"hidden",display:"flex",flexDirection:"column"}}>
            {bottomTab==="console"&&<Console ref={consoleRef} engine={engine} output={output} onAddOutput={addOutput} onRunCode={runCode} helpTopic={helpTopic} onSetHelpTopic={setHelpTopic}/>}
            {bottomTab==="workspace"&&<Workspace variables={variables}/>}
            {bottomTab==="cheatsheet"&&<Reference/>}
          </div>
        </div>}
      </div>

      {showSaveDialog&&<div style={{position:"fixed",inset:0,background:"rgba(0,0,0,0.6)",display:"flex",alignItems:"center",justifyContent:"center",zIndex:1000}} onClick={()=>setShowSaveDialog(false)}>
        <div onClick={e=>e.stopPropagation()} style={{background:C.bg2,border:`1px solid ${C.borderHi}`,borderRadius:8,padding:20,width:320,boxShadow:"0 8px 32px rgba(0,0,0,0.5)"}}>
          <div style={{fontSize:13,fontWeight:600,color:C.text,marginBottom:12}}>Save to Local Files</div>
          <input value={saveFileName} onChange={e=>setSaveFileName(e.target.value)} autoFocus onKeyDown={e=>{if(e.key==="Enter")handleSaveDialogSubmit();if(e.key==="Escape")setShowSaveDialog(false);}} placeholder="filename" style={{width:"100%",padding:"8px 10px",borderRadius:5,fontSize:12,background:C.bg0,border:`1px solid ${C.border}`,color:C.text,outline:"none",fontFamily:FONT,marginBottom:12,boxSizing:"border-box"}}/>
          <div style={{display:"flex",gap:8,justifyContent:"flex-end"}}>
            <button onClick={()=>setShowSaveDialog(false)} style={{padding:"6px 14px",borderRadius:5,fontSize:11,background:C.bg3,border:`1px solid ${C.border}`,color:C.textDim,cursor:"pointer",fontFamily:FONT_UI}}>Cancel</button>
            <button onClick={handleSaveDialogSubmit} style={{padding:"6px 14px",borderRadius:5,fontSize:11,fontWeight:600,background:C.accent,border:"none",color:"#fff",cursor:"pointer",fontFamily:FONT_UI}}>Save</button>
          </div>
        </div>
      </div>}

      <div style={{display:"flex",justifyContent:"space-between",alignItems:"center",padding:"3px 12px",background:C.bg1,borderTop:`1px solid ${C.border}`,fontSize:9,color:C.textMuted,flexShrink:0}}>
        <div style={{display:"flex",alignItems:"center",gap:5}}>
          <span style={{width:6,height:6,borderRadius:"50%",background:statusProp==="ready"?C.green:C.yellow,display:"inline-block"}}/><span>{statusProp==="ready"?"WASM":"Demo"}</span>
          <span style={{color:C.border}}>|</span><span>{activeTabData?.name}</span>
          {activeTabData?.vfsPath&&<><span style={{color:C.border}}>|</span><span style={{color:C.green}}>{activeTabData.source==='localFolder'?'💾 local folder':'📌 temporary'}</span></>}
          {figures.length>0&&<><span style={{color:C.border}}>|</span><span>{figures.length} figure{figures.length>1?"s":""}</span></>}
          {activeBreakpoints.length>0&&<><span style={{color:C.border}}>|</span><span style={{color:C.red}}>● {activeBreakpoints.length} bp</span></>}
          {isDebugging&&<><span style={{color:C.border}}>|</span><span style={{color:C.orange}}>⏸ line {debugState.line}</span></>}
        </div>
        <div style={{display:"flex",alignItems:"center",gap:6}}>
          {execTimeMs!==null&&<span>{execTimeMs.toFixed(1)}ms</span>}
          <span style={{color:C.border}}>|</span><span>Ctrl+S: save</span><span style={{color:C.border}}>|</span><span>Tab: autocomplete</span><span style={{color:C.border}}>|</span><span>↑↓: history</span>
        </div>
      </div>
    </div>
  );
}
