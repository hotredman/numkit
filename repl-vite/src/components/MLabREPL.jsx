import { useState, useEffect, useRef, useCallback } from "react";
import * as d3 from "d3";
import HELP_DB from "../data/help";
import EXAMPLES from "../data/examples";
import CHEAT_SHEET from "../data/cheatsheet";
import FileBrowser from "./FileBrowser";
import vfs from "../vfs";
import C, { FONT, FONT_UI } from "../theme";

// ── PlotPanel ──
function PlotPanel({ data, onClose }) {
  const svgRef = useRef(null);
  const containerRef = useRef(null);
  useEffect(() => {
    if (!data || !svgRef.current || !containerRef.current) return;
    const svg = d3.select(svgRef.current); svg.selectAll("*").remove();
    const cw = containerRef.current.clientWidth;
    const width = Math.min(cw - 8, 560), height = 240;
    const margin = { top: 28, right: 16, bottom: 36, left: 46 };
    const iw = width - margin.left - margin.right, ih = height - margin.top - margin.bottom;
    svg.attr("width", width).attr("height", height);
    const g = svg.append("g").attr("transform", `translate(${margin.left},${margin.top})`);
    const colors = [C.accent, C.cyan, C.green, C.orange, C.pink, C.yellow];
    const allX = data.datasets.flatMap(d => d.x), allY = data.datasets.flatMap(d => d.y);
    const xScale = d3.scaleLinear().domain([Math.min(...allX), Math.max(...allX)]).range([0, iw]).nice();
    const yScale = d3.scaleLinear().domain([Math.min(...allY)*0.95, Math.max(...allY)*1.05]).range([ih, 0]).nice();
    g.append("g").selectAll("line").data(yScale.ticks(4)).enter().append("line").attr("x1",0).attr("x2",iw).attr("y1",d=>yScale(d)).attr("y2",d=>yScale(d)).attr("stroke",C.border).attr("stroke-dasharray","2,4");
    g.append("g").attr("transform",`translate(0,${ih})`).call(d3.axisBottom(xScale).ticks(5)).selectAll("text,line,path").attr("fill",C.textMuted).attr("stroke",C.textMuted);
    g.append("g").call(d3.axisLeft(yScale).ticks(4)).selectAll("text,line,path").attr("fill",C.textMuted).attr("stroke",C.textMuted);
    data.datasets.forEach((ds, idx) => {
      const color = colors[idx % colors.length];
      if (data.config.type === "line") g.append("path").datum(ds.y).attr("d",d3.line().x((_,i)=>xScale(ds.x[i])).y((_,i)=>yScale(ds.y[i])).curve(d3.curveMonotoneX)).attr("fill","none").attr("stroke",color).attr("stroke-width",2);
      else if (data.config.type === "scatter") g.selectAll(`.dot-${idx}`).data(ds.x.map((x,i)=>({x,y:ds.y[i]}))).enter().append("circle").attr("cx",d=>xScale(d.x)).attr("cy",d=>yScale(d.y)).attr("r",4).attr("fill",color).attr("opacity",0.8);
      else if (data.config.type === "bar") { const bw = Math.max(2,iw/ds.x.length*0.7); g.selectAll(`.bar-${idx}`).data(ds.x.map((x,i)=>({x,y:ds.y[i]}))).enter().append("rect").attr("x",d=>xScale(d.x)-bw/2).attr("y",d=>yScale(d.y)).attr("width",bw).attr("height",d=>ih-yScale(d.y)).attr("fill",color).attr("opacity",0.85).attr("rx",2); }
    });
    if (data.config.title) svg.append("text").attr("x",width/2).attr("y",16).attr("text-anchor","middle").attr("fill",C.text).attr("font-size",12).attr("font-weight",600).text(data.config.title);
    if (data.config.xlabel) svg.append("text").attr("x",width/2).attr("y",height-4).attr("text-anchor","middle").attr("fill",C.textMuted).attr("font-size",10).text(data.config.xlabel);
    if (data.config.ylabel) svg.append("text").attr("transform",`translate(12,${height/2}) rotate(-90)`).attr("text-anchor","middle").attr("fill",C.textMuted).attr("font-size",10).text(data.config.ylabel);
  }, [data]);
  if (!data) return null;
  return (
    <div ref={containerRef} style={{ background: C.bg1, border: `1px solid ${C.border}`, borderRadius: 6, margin: "4px 0", padding: 6, position: "relative" }}>
      <button onClick={onClose} style={{ position: "absolute", top: 4, right: 6, background: "none", border: "none", color: C.textMuted, cursor: "pointer", fontSize: 14, lineHeight: 1 }}>×</button>
      <svg ref={svgRef} style={{ display: "block", margin: "0 auto" }} />
    </div>
  );
}

// ── Tab Bar ──
function TabBar({ tabs, activeTab, onSelect, onClose, onNew, onRename }) {
  const [editingId, setEditingId] = useState(null);
  const [editName, setEditName] = useState("");
  return (
    <div style={{ display: "flex", alignItems: "center", gap: 1, padding: "3px 8px", background: C.bg0, borderBottom: `1px solid ${C.border}`, overflowX: "auto", minHeight: 32, flexShrink: 0 }}>
      {tabs.map(tab => (
        <div key={tab.id} onClick={() => onSelect(tab.id)} style={{
          display: "flex", alignItems: "center", gap: 4, padding: "3px 10px", borderRadius: 5, cursor: "pointer", fontSize: 11,
          background: tab.id === activeTab ? C.bg3 : "transparent", color: tab.id === activeTab ? C.text : C.textMuted,
          border: `1px solid ${tab.id === activeTab ? C.borderHi : "transparent"}`, whiteSpace: "nowrap", transition: "all 0.15s",
        }}>
          {editingId === tab.id ? (
            <input value={editName} autoFocus onChange={e => setEditName(e.target.value)}
              onBlur={() => { onRename(tab.id, editName); setEditingId(null); }}
              onKeyDown={e => { if (e.key === "Enter") { onRename(tab.id, editName); setEditingId(null); } }}
              style={{ background: "transparent", border: "none", color: C.text, fontSize: 11, width: 80, outline: "none", fontFamily: FONT }}
              onClick={e => e.stopPropagation()} />
          ) : (
            <span onDoubleClick={e => { e.stopPropagation(); setEditingId(tab.id); setEditName(tab.name); }}>
              {tab.name}{tab.modified ? " •" : ""}
            </span>
          )}
          {tabs.length > 1 && <span onClick={e => { e.stopPropagation(); onClose(tab.id); }} style={{ color: C.textMuted, fontSize: 13, lineHeight: 1, marginLeft: 2, opacity: 0.5 }}>×</span>}
        </div>
      ))}
      <button onClick={onNew} style={{ background: "none", border: `1px dashed ${C.border}`, borderRadius: 5, color: C.textMuted, fontSize: 13, padding: "1px 8px", cursor: "pointer", lineHeight: 1, marginLeft: 4 }}>+</button>
    </div>
  );
}

// ── Variable Inspector ──
function VarInspector({ variables }) {
  const entries = Object.entries(variables);
  const getType = v => { if (Array.isArray(v)) return v.length && Array.isArray(v[0]) ? "matrix" : "vector"; if (typeof v === "string") return "char"; if (typeof v === "object" && v !== null) return "struct"; return "double"; };
  const getSize = v => { if (Array.isArray(v)) return v.length && Array.isArray(v[0]) ? `${v.length}×${v[0].length}` : `1×${v.length}`; if (typeof v === "string") return `1×${v.length}`; if (typeof v === "object" && v !== null) return `1×${Object.keys(v).length}`; return "1×1"; };
  const getPreview = v => { if (Array.isArray(v)) { const f=v.flat(); return f.length<=6 ? `[${f.map(x=>typeof x==="number"?(Number.isInteger(x)?x:x.toFixed(3)):x).join(", ")}]` : `[${f.slice(0,4).map(x=>typeof x==="number"?(Number.isInteger(x)?x:x.toFixed(3)):x).join(", ")}, …]`; } if (typeof v==="string") return `'${v}'`; if (typeof v==="object"&&v!==null) return `{${Object.keys(v).join(", ")}}`; if (typeof v==="number") return Number.isInteger(v)?String(v):v.toFixed(6); return String(v); };
  const tc = { double: C.cyan, vector: C.green, matrix: C.accent, char: C.yellow, struct: C.orange };
  return (
    <div style={{ flex: 1, overflowY: "auto", padding: 8 }}>
      {!entries.length ? <div style={{ color: C.textMuted, fontSize: 11, padding: 16, textAlign: "center", lineHeight: 1.6 }}>No variables.<br/>Run code to see them here.</div>
      : entries.map(([name, val]) => { const t=getType(val); return (
        <div key={name} style={{ padding: "7px 10px", marginBottom: 3, borderRadius: 5, background: C.bg2, border: `1px solid ${C.border}` }}>
          <div style={{ display:"flex", justifyContent:"space-between", alignItems:"center", marginBottom: 3 }}>
            <span style={{ fontSize: 12, fontWeight: 600, color: C.text }}>{name}</span>
            <div style={{ display:"flex", gap: 5, alignItems:"center" }}>
              <span style={{ fontSize: 9, color: C.textMuted }}>{getSize(val)}</span>
              <span style={{ fontSize: 9, padding: "1px 5px", borderRadius: 3, background: `${tc[t]||C.textDim}22`, color: tc[t]||C.textDim }}>{t}</span>
            </div>
          </div>
          <div style={{ fontSize: 10, color: C.textDim, fontFamily: FONT, whiteSpace: "nowrap", overflow: "hidden", textOverflow: "ellipsis" }}>{getPreview(val)}</div>
        </div>
      ); })}
    </div>
  );
}

// ── Cheat Sheet ──
function CheatSheetContent() {
  return (
    <div style={{ padding: 10, display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(200px, 1fr))", gap: 8, alignContent: "start", overflowY: "auto" }}>
      {CHEAT_SHEET.map(s => (
        <div key={s.title} style={{ background: C.bg2, borderRadius: 6, padding: 10, border: `1px solid ${C.border}` }}>
          <div style={{ fontSize: 10, fontWeight: 700, color: C.accent, marginBottom: 6, textTransform: "uppercase", letterSpacing: 0.8 }}>{s.title}</div>
          {s.items.map((item, i) => (
            <div key={i} style={{ display: "flex", justifyContent: "space-between", gap: 6, marginBottom: 3 }}>
              <code style={{ fontSize: 10, color: C.green, whiteSpace: "nowrap" }}>{item.code}</code>
              <span style={{ fontSize: 9, color: C.textMuted, textAlign: "right" }}>{item.desc}</span>
            </div>
          ))}
        </div>
      ))}
    </div>
  );
}

// ── Examples (bottom panel) ──
function ExamplesContent({ onRun }) {
  return (
    <div style={{ padding: "8px 12px", overflowY: "auto" }}>
      {EXAMPLES.map(cat => (
        <div key={cat.category} style={{ marginBottom: 10 }}>
          <div style={{ fontSize: 10, fontWeight: 700, color: C.accent, textTransform: "uppercase", letterSpacing: 0.6, marginBottom: 5 }}>{cat.icon} {cat.category}</div>
          <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(220px, 1fr))", gap: 5 }}>
            {cat.items.map(item => (
              <div key={item.title} onClick={() => onRun(item)} style={{ background: C.bg2, border: `1px solid ${C.border}`, borderRadius: 6, padding: "8px 10px", cursor: "pointer", transition: "all 0.15s" }}
                onMouseEnter={e => { e.currentTarget.style.borderColor=C.accent; e.currentTarget.style.background=C.bg3; }}
                onMouseLeave={e => { e.currentTarget.style.borderColor=C.border; e.currentTarget.style.background=C.bg2; }}>
                <div style={{ fontSize: 11, fontWeight: 600, color: C.text }}>{item.title}</div>
                <div style={{ fontSize: 9, color: C.textMuted, marginBottom: 3 }}>{item.description}</div>
                <pre style={{ fontSize: 9, color: C.textDim, background: C.bg0, borderRadius: 3, padding: "3px 5px", margin: 0, overflow: "hidden", maxHeight: "2.4em", whiteSpace: "pre", fontFamily: FONT }}>
                  {item.code.split("\n").slice(0,2).join("\n")}{item.code.split("\n").length>2?"\n…":""}
                </pre>
              </div>
            ))}
          </div>
        </div>
      ))}
    </div>
  );
}

// ═══════════════════════════════════════════
// Main IDE
// ═══════════════════════════════════════════
export default function MLabREPL({ engine: engineProp, status: statusProp }) {
  const [showLeft, setShowLeft] = useState(true);
  const [showCenter, setShowCenter] = useState(true);
  const [showRight, setShowRight] = useState(false);
  const [showBottom, setShowBottom] = useState(true);
  const [bottomTab, setBottomTab] = useState("console");
  const [output, setOutput] = useState([]);
  const [inputVal, setInputVal] = useState("");
  const [history, setHistory] = useState([]);
  const [histIdx, setHistIdx] = useState(-1);
  const [savedInput, setSavedInput] = useState("");
  const [helpTopic, setHelpTopic] = useState(null);
  const [plots, setPlots] = useState([]);
  const [acItems, setAcItems] = useState([]);
  const [acIdx, setAcIdx] = useState(-1);
  const [acPartial, setAcPartial] = useState("");
  const [execTimeMs, setExecTimeMs] = useState(null);
  const [variables, setVariables] = useState({});
  const [errorLine, setErrorLine] = useState(null);
  const [tabs, setTabs] = useState([{ id: "1", name: "untitled.m", code: "", modified: false, vfsPath: null, source: null }]);
  const [activeTab, setActiveTab] = useState("1");
  const [vfsRefreshKey, setVfsRefreshKey] = useState(0);
  const [showSaveDialog, setShowSaveDialog] = useState(false);
  const [saveFileName, setSaveFileName] = useState("");

  const outputRef = useRef(null);
  const inputRef = useRef(null);
  const tabCountRef = useRef(1);
  const editorRef = useRef(null);
  const gutterRef = useRef(null);
  const engine = engineProp;

  const scrollBottom = useCallback(() => { requestAnimationFrame(() => { if (outputRef.current) outputRef.current.scrollTop = outputRef.current.scrollHeight; }); }, []);
  useEffect(() => { setOutput([{ type: "system", text: "MLab REPL v2.3 — Web IDE" }, { type: "system", text: 'Type commands below. "help <topic>" for function info.' }]); }, []);
  useEffect(scrollBottom, [output, plots]);

  const addOutput = useCallback(items => { setOutput(prev => { for (const i of items) if (i.text === "__CLEAR__") return []; return [...prev, ...items.filter(i => i.text !== "__CLEAR__")]; }); }, []);

  const runCode = useCallback(code => {
    const t0 = performance.now();
    const result = engine.execute(code);
    setExecTimeMs(performance.now() - t0); setErrorLine(null);
    const items = [];
    if (result.output) for (const line of result.output.split("\n")) {
      if (line === "__CLEAR__") { setOutput([]); continue; }
      items.push({ type: /^Error/.test(line) ? "error" : /^Warning:/.test(line) ? "warning" : "result", text: line });
    }
    addOutput(items);
    if (result.plots?.length) setPlots(prev => [...prev, ...result.plots]);
    if (result.errorLine) setErrorLine(result.errorLine);
    setVariables(engine.getVars());
  }, [engine, addOutput]);

  const handleSubmit = useCallback(() => {
    const val = inputVal.trim(); if (!val) return;
    addOutput([{ type: "input", text: val }]);
    setHistory(prev => { const h = [...prev, val]; return h.length > 200 ? h.slice(-200) : h; });
    setHistIdx(-1); setInputVal(""); setAcItems([]);
    const hm = val.match(/^help\s+(\w+)$/);
    if (hm && HELP_DB[hm[1]]) { setHelpTopic(hm[1]); return; }
    if (val === "help") { setHelpTopic(null); addOutput([{ type: "system", text: "Commands: clc, clear, who, whos, help <topic>" }]); return; }
    runCode(val);
  }, [inputVal, addOutput, runCode]);

  const handleKeyDown = useCallback(e => {
    if (acItems.length > 0) {
      if (e.key === "ArrowDown") { e.preventDefault(); setAcIdx(i => (i+1)%acItems.length); return; }
      if (e.key === "ArrowUp") { e.preventDefault(); setAcIdx(i => (i-1+acItems.length)%acItems.length); return; }
      if ((e.key === "Enter" || e.key === "Tab") && acIdx >= 0) { e.preventDefault(); const item=acItems[acIdx],val=inputVal,cur=inputRef.current?.selectionStart||val.length; let ws=cur-1; while(ws>=0&&/[a-zA-Z0-9_]/.test(val[ws]))ws--;ws++; setInputVal(val.substring(0,ws)+item+val.substring(cur)); setAcItems([]); return; }
      if (e.key === "Escape") { setAcItems([]); return; }
    }
    if (e.key === "Enter" && !e.shiftKey) { e.preventDefault(); handleSubmit(); return; }
    if (e.key === "ArrowUp" && !e.shiftKey && !inputVal.includes("\n")) { e.preventDefault(); if (!history.length) return; const ni=histIdx===-1?history.length-1:Math.max(0,histIdx-1); if(histIdx===-1)setSavedInput(inputVal); setHistIdx(ni); setInputVal(history[ni]); return; }
    if (e.key === "ArrowDown" && !e.shiftKey && !inputVal.includes("\n")) { e.preventDefault(); if(histIdx===-1)return; if(histIdx<history.length-1){setHistIdx(histIdx+1);setInputVal(history[histIdx+1]);}else{setHistIdx(-1);setInputVal(savedInput);} return; }
    if (e.key === "Tab") { e.preventDefault(); const val=inputVal,cur=inputRef.current?.selectionStart||val.length; let ws=cur-1; while(ws>=0&&/[a-zA-Z0-9_]/.test(val[ws]))ws--;ws++; const partial=val.substring(ws,cur); if(partial){const items=engine.complete(partial); if(items.length===1){setInputVal(val.substring(0,ws)+items[0]+val.substring(cur));setAcItems([]);}else if(items.length>1){setAcItems(items);setAcIdx(0);setAcPartial(partial);}} return; }
    if (e.key === "l" && e.ctrlKey) { e.preventDefault(); setOutput([]); setPlots([]); }
  }, [inputVal, handleSubmit, history, histIdx, savedInput, acItems, acIdx, engine]);

  const runExample = useCallback(item => { setBottomTab("console"); addOutput([{ type: "system", text: `── ${item.title} ──` }, { type: "input", text: item.code }]); runCode(item.code); inputRef.current?.focus(); }, [addOutput, runCode]);

  // Tabs
  const newTab = useCallback(() => { tabCountRef.current++; const id=String(tabCountRef.current); setTabs(p=>[...p,{id,name:`script${tabCountRef.current}.m`,code:"",modified:false,vfsPath:null,source:null}]); setActiveTab(id); }, []);
  const closeTab = useCallback(id => { setTabs(p=>{const n=p.filter(t=>t.id!==id);if(!n.length)return p;if(activeTab===id)setActiveTab(n[n.length-1].id);return n;}); }, [activeTab]);
  const renameTab = useCallback((id,name) => { if(!name.trim())return; setTabs(p=>p.map(t=>t.id===id?{...t,name:name.trim()}:t)); }, []);
  const activeTabData = tabs.find(t => t.id === activeTab) || tabs[0];
  const updateTabCode = useCallback(code => { setTabs(p=>p.map(t=>t.id===activeTab?{...t,code,modified:true}:t)); }, [activeTab]);

  const runActiveTab = useCallback(() => {
    const tab = tabs.find(t=>t.id===activeTab); if(!tab||!tab.code.trim()) return;
    setBottomTab("console"); setShowBottom(true);
    addOutput([{ type: "system", text: `── Running ${tab.name} ──` }, { type: "input", text: tab.code }]);
    runCode(tab.code);
    setTabs(p=>p.map(t=>t.id===activeTab?{...t,modified:false}:t));
  }, [tabs, activeTab, addOutput, runCode]);

  // File operations
  const handleOpenFile = useCallback((filename, content, vfsPath, source) => {
    const existing = tabs.find(t => t.vfsPath && t.vfsPath === vfsPath);
    if (existing) { setActiveTab(existing.id); return; }
    tabCountRef.current++; const id = String(tabCountRef.current);
    setTabs(p=>[...p,{id,name:filename,code:content,modified:false,vfsPath:vfsPath||null,source:source||null}]);
    setActiveTab(id); setShowCenter(true);
  }, [tabs]);

  const handleSaveToVFS = useCallback(async (path, name) => {
    const tab = tabs.find(t=>t.id===activeTab); if(!tab) return;
    const fullPath = path || tab.vfsPath; if(!fullPath) return;
    await vfs.writeFile(fullPath, tab.code);
    setTabs(p=>p.map(t=>t.id===activeTab?{...t,modified:false,vfsPath:fullPath,name:name||t.name}:t));
    setVfsRefreshKey(k=>k+1);
    addOutput([{ type: "system", text: `Saved ${name||tab.name}` }]);
  }, [tabs, activeTab, addOutput]);

  const handleSave = useCallback(() => {
    const tab = tabs.find(t=>t.id===activeTab); if(!tab) return;
    if (tab.vfsPath) handleSaveToVFS(tab.vfsPath, tab.name);
    else { setSaveFileName(tab.name); setShowSaveDialog(true); }
  }, [tabs, activeTab, handleSaveToVFS]);

  const handleSaveDialogSubmit = useCallback(async () => {
    if (!saveFileName.trim()) return;
    const name = saveFileName.trim().endsWith('.m') ? saveFileName.trim() : saveFileName.trim() + '.m';
    await handleSaveToVFS(`/${name}`, name);
    setShowSaveDialog(false);
  }, [saveFileName, handleSaveToVFS]);

  const handleDownload = useCallback(() => {
    const tab = tabs.find(t=>t.id===activeTab); if(!tab) return;
    const blob = new Blob([tab.code], { type: "text/plain" }); const url = URL.createObjectURL(blob);
    const a = document.createElement("a"); a.href = url; a.download = tab.name; a.click(); URL.revokeObjectURL(url);
  }, [tabs, activeTab]);

  const handleFileLoad = useCallback(() => {
    const input = document.createElement("input"); input.type = "file"; input.accept = ".m,.txt";
    input.onchange = e => { const file=e.target.files[0]; if(!file)return; const r=new FileReader(); r.onload=ev=>{handleOpenFile(file.name,ev.target.result,null,'disk');}; r.readAsText(file); };
    input.click();
  }, [handleOpenFile]);

  useEffect(() => {
    const h = e => { if ((e.ctrlKey||e.metaKey) && e.key === 's') { e.preventDefault(); handleSave(); } };
    window.addEventListener('keydown', h); return () => window.removeEventListener('keydown', h);
  }, [handleSave]);

  // UI helpers
  const PanelBtn = ({ active, onClick, icon, label, title }) => (
    <button onClick={onClick} title={title} style={{ display:"flex",alignItems:"center",gap:4,padding:"4px 8px",border:"none",borderRadius:4, background:active?`${C.accent}25`:"transparent",color:active?C.accent:C.textMuted, fontFamily:FONT_UI,fontSize:11,fontWeight:500,cursor:"pointer",transition:"all 0.15s",whiteSpace:"nowrap" }}
    onMouseEnter={e=>{if(!active)e.currentTarget.style.background=C.bg3;}} onMouseLeave={e=>{if(!active)e.currentTarget.style.background="transparent";}}>
      <span style={{fontSize:13}}>{icon}</span>{label}
    </button>
  );
  const ActBtn = ({ onClick, icon, label, color, title }) => (
    <button onClick={onClick} title={title} style={{ display:"flex",alignItems:"center",gap:3,padding:"4px 8px",border:`1px solid ${C.border}`,borderRadius:4, background:C.bg2,color:color||C.textDim,fontFamily:FONT_UI,fontSize:11,fontWeight:500,cursor:"pointer",transition:"all 0.15s",whiteSpace:"nowrap" }}
    onMouseEnter={e=>e.currentTarget.style.borderColor=C.borderHi} onMouseLeave={e=>e.currentTarget.style.borderColor=C.border}>
      <span style={{fontSize:12}}>{icon}</span>{label}
    </button>
  );
  const bottomTabBtn = (id, label) => (
    <button onClick={()=>setBottomTab(id)} style={{ padding:"5px 12px",border:"none",borderBottom:bottomTab===id?`2px solid ${C.accent}`:"2px solid transparent", background:"transparent",color:bottomTab===id?C.text:C.textMuted,fontFamily:FONT_UI,fontSize:11,fontWeight:bottomTab===id?600:400,cursor:"pointer",transition:"all 0.15s" }}>{label}</button>
  );

  return (
    <div style={{ display:"flex",flexDirection:"column",height:"100vh",width:"100%",background:C.bg0,color:C.text,fontFamily:FONT,fontSize:13,overflow:"hidden" }}>
      {/* Top Bar */}
      <div style={{ display:"flex",alignItems:"center",justifyContent:"space-between",padding:"5px 12px",background:C.bg1,borderBottom:`1px solid ${C.border}`,flexShrink:0,zIndex:30,gap:8 }}>
        <div style={{ display:"flex",alignItems:"baseline",gap:6,flexShrink:0 }}>
          <span style={{ fontSize:15,fontWeight:700,letterSpacing:-0.5,fontFamily:FONT_UI }}>MLab <span style={{color:C.accent}}>IDE</span></span>
          <span style={{ fontSize:9,color:C.textMuted }}>v2.3</span>
        </div>
        <div style={{ display:"flex",gap:2,alignItems:"center",background:C.bg0,borderRadius:6,padding:"2px 3px" }}>
          <PanelBtn active={showLeft} onClick={()=>setShowLeft(!showLeft)} icon="📂" label="Explorer" title="File Browser" />
          <PanelBtn active={showCenter} onClick={()=>setShowCenter(!showCenter)} icon="📝" label="Editor" title="Code Editor" />
          <PanelBtn active={showRight} onClick={()=>setShowRight(!showRight)} icon="🔍" label="Workspace" title="Variable Inspector" />
          <PanelBtn active={showBottom} onClick={()=>setShowBottom(!showBottom)} icon="💻" label="Terminal" title="Bottom Panel" />
        </div>
        <div style={{ display:"flex",gap:3,flexShrink:0 }}>
          {showCenter && <ActBtn onClick={runActiveTab} icon="▶" label="Run" color={C.green} title="Run current script" />}
          {showCenter && <ActBtn onClick={handleSave} icon="💾" label="Save" title="Save to local FS (Ctrl+S)" />}
          <ActBtn onClick={handleFileLoad} icon="📂" label="Open" title="Open from disk" />
          {showCenter && <ActBtn onClick={handleDownload} icon="⬇" label="Export" title="Download to disk" />}
          <ActBtn onClick={()=>{setOutput([]);setPlots([]);}} icon="🗑" label="Clear" title="Clear console" />
          <ActBtn onClick={()=>{engine.reset();setVariables({});addOutput([{type:"system",text:"Workspace cleared."}]);}} icon="🔄" label="Reset" title="Reset workspace" />
        </div>
      </div>

      {/* Main */}
      <div style={{ flex:1,display:"flex",flexDirection:"column",overflow:"hidden" }}>
        <div style={{ flex:showBottom?"1 1 60%":1,display:"flex",overflow:"hidden",minHeight:0 }}>
          {showLeft && (
            <div style={{ width:280,minWidth:220,flexShrink:0,background:C.bg1,borderRight:`1px solid ${C.border}`,display:"flex",flexDirection:"column",overflow:"hidden" }}>
              <FileBrowser onOpenFile={handleOpenFile} defaultGitHubRepo="skynetjudgmentday/mlab-demo" vfsRefreshKey={vfsRefreshKey} />
            </div>
          )}
          {showCenter && (
            <div style={{ flex:1,display:"flex",flexDirection:"column",overflow:"hidden",minWidth:0 }}>
              <TabBar tabs={tabs} activeTab={activeTab} onSelect={setActiveTab} onClose={closeTab} onNew={newTab} onRename={renameTab} />
              <div style={{ flex:1,display:"flex",overflow:"hidden",position:"relative" }}>
                <div ref={gutterRef} style={{ padding:"8px 0",background:C.bg0,borderRight:`1px solid ${C.border}`,userSelect:"none",minWidth:34,textAlign:"right",overflowY:"hidden",flexShrink:0 }}>
                  {(activeTabData?.code||"").split("\n").map((_,i)=>{const ln=i+1,isErr=errorLine===ln;return<div key={i} style={{fontSize:10,padding:"0 6px",lineHeight:"20px",color:isErr?C.red:C.textMuted,background:isErr?`${C.red}18`:"transparent",fontWeight:isErr?700:400}}>{ln}</div>;})}
                </div>
                <div style={{ flex:1,position:"relative",overflow:"hidden" }}>
                  {errorLine&&<div style={{position:"absolute",left:0,right:0,top:(errorLine-1)*20+8,height:20,background:`${C.red}12`,borderLeft:`2px solid ${C.red}`,pointerEvents:"none",zIndex:1}}/>}
                  <textarea ref={editorRef} value={activeTabData?.code||""} onChange={e=>{updateTabCode(e.target.value);setErrorLine(null);}} spellCheck={false}
                    onScroll={e=>{if(gutterRef.current)gutterRef.current.scrollTop=e.target.scrollTop;}}
                    style={{width:"100%",height:"100%",background:C.bg1,color:C.text,border:"none",outline:"none",fontFamily:FONT,fontSize:13,lineHeight:"20px",padding:8,resize:"none",caretColor:C.accent,overflow:"auto",position:"relative",zIndex:2}}/>
                </div>
              </div>
            </div>
          )}
          {showRight && (
            <div style={{ width:260,minWidth:200,flexShrink:0,background:C.bg1,borderLeft:`1px solid ${C.border}`,display:"flex",flexDirection:"column",overflow:"hidden" }}>
              <div style={{ padding:"7px 10px",borderBottom:`1px solid ${C.border}`,display:"flex",justifyContent:"space-between",alignItems:"center",flexShrink:0 }}>
                <span style={{ fontSize:11,fontWeight:600,color:C.text,fontFamily:FONT_UI }}>🔍 Workspace</span>
                <button onClick={()=>setShowRight(false)} style={{background:"none",border:"none",color:C.textMuted,cursor:"pointer",fontSize:16,lineHeight:1}}>×</button>
              </div>
              <VarInspector variables={variables} />
            </div>
          )}
          {!showLeft&&!showCenter&&!showRight&&<div style={{flex:1,display:"flex",alignItems:"center",justifyContent:"center",color:C.textMuted,fontSize:12,fontFamily:FONT_UI}}>Toggle panels from the toolbar</div>}
        </div>

        {/* Bottom */}
        {showBottom && (
          <div style={{ flex:"0 0 40%",minHeight:140,maxHeight:"55%",display:"flex",flexDirection:"column",borderTop:`2px solid ${C.border}`,overflow:"hidden" }}>
            <div style={{ display:"flex",alignItems:"center",background:C.bg0,borderBottom:`1px solid ${C.border}`,flexShrink:0,justifyContent:"space-between" }}>
              <div style={{display:"flex"}}>{bottomTabBtn("console","💻 Console")}{bottomTabBtn("examples","📋 Examples")}{bottomTabBtn("cheatsheet","📖 Reference")}</div>
              <button onClick={()=>setShowBottom(false)} style={{background:"none",border:"none",color:C.textMuted,cursor:"pointer",fontSize:16,padding:"0 10px",lineHeight:1}}>×</button>
            </div>
            <div style={{ flex:1,overflow:"hidden",display:"flex",flexDirection:"column" }}>
              {bottomTab==="console"&&(
                <div style={{ flex:1,display:"flex",flexDirection:"column",overflow:"hidden" }}>
                  <div ref={outputRef} style={{ flex:1,overflowY:"auto",padding:"8px 12px",background:C.bg1 }}>
                    {output.map((item,i)=>{
                      const clr={input:C.textMuted,result:C.text,error:C.red,warning:C.orange,system:C.textMuted,info:C.cyan};
                      if(item.type==="input")return<div key={i} style={{padding:"1px 0",whiteSpace:"pre-wrap",wordBreak:"break-word"}}><span style={{color:C.green,fontWeight:700,userSelect:"none"}}>{">> "}</span><span style={{color:C.textMuted}}>{item.text}</span></div>;
                      return<div key={i} style={{padding:"1px 0",color:clr[item.type]||C.text,fontStyle:item.type==="system"?"italic":"normal",whiteSpace:"pre-wrap",wordBreak:"break-word"}}>{item.text}</div>;
                    })}
                    {helpTopic&&HELP_DB[helpTopic]&&(
                      <div style={{background:C.bg2,border:`1px solid ${C.borderHi}`,borderRadius:6,padding:"8px 12px",margin:"4px 0",position:"relative"}}>
                        <button onClick={()=>setHelpTopic(null)} style={{position:"absolute",top:4,right:6,background:"none",border:"none",color:C.textMuted,cursor:"pointer",fontSize:14}}>×</button>
                        <div style={{fontSize:13,fontWeight:700,color:C.accent,marginBottom:3}}>{HELP_DB[helpTopic].sig}</div>
                        <div style={{fontSize:11,color:C.text,marginBottom:3}}>{HELP_DB[helpTopic].desc}</div>
                        <div style={{fontSize:10,color:C.textMuted}}>Category: {HELP_DB[helpTopic].cat}</div>
                        <div style={{fontSize:11,color:C.green,marginTop:3,fontFamily:FONT}}>{HELP_DB[helpTopic].ex}</div>
                      </div>
                    )}
                    {plots.map((p,i)=><PlotPanel key={i} data={p} onClose={()=>setPlots(prev=>prev.filter((_,j)=>j!==i))}/>)}
                  </div>
                  <div style={{display:"flex",alignItems:"flex-start",padding:"8px 12px",background:C.bg0,borderTop:`1px solid ${C.border}`,flexShrink:0,position:"relative"}}>
                    <span style={{color:C.green,fontWeight:700,marginRight:6,marginTop:2,userSelect:"none",flexShrink:0,fontSize:13}}>&gt;&gt;</span>
                    <div style={{flex:1,position:"relative"}}>
                      {acItems.length>1&&(
                        <div style={{position:"absolute",bottom:"calc(100% + 4px)",left:0,minWidth:160,maxWidth:320,maxHeight:160,overflowY:"auto",background:C.bg3,border:`1px solid ${C.border}`,borderRadius:5,boxShadow:"0 -4px 16px rgba(0,0,0,0.5)",zIndex:100}}>
                          {acItems.map((item,i)=>(
                            <div key={item} onClick={()=>{const val=inputVal,cur=inputRef.current?.selectionStart||val.length;let ws=cur-1;while(ws>=0&&/[a-zA-Z0-9_]/.test(val[ws]))ws--;ws++;setInputVal(val.substring(0,ws)+item+val.substring(cur));setAcItems([]);inputRef.current?.focus();}}
                              style={{padding:"4px 8px",cursor:"pointer",fontSize:11,color:i===acIdx?C.text:C.textDim,background:i===acIdx?C.border:"transparent"}}>
                              <span style={{color:C.accent,fontWeight:600}}>{item.substring(0,acPartial.length)}</span>{item.substring(acPartial.length)}
                            </div>
                          ))}
                        </div>
                      )}
                      <textarea ref={inputRef} value={inputVal} onChange={e=>{setInputVal(e.target.value);setAcItems([]);}}
                        onKeyDown={handleKeyDown} rows={1} spellCheck={false} autoComplete="off" placeholder="Enter MLab command…"
                        style={{width:"100%",background:"transparent",border:"none",outline:"none",color:C.text,fontFamily:FONT,fontSize:13,lineHeight:1.6,resize:"none",overflow:"hidden",caretColor:C.accent}}
                        onInput={e=>{e.target.style.height="auto";e.target.style.height=e.target.scrollHeight+"px";}}/>
                    </div>
                  </div>
                </div>
              )}
              {bottomTab==="examples"&&<div style={{flex:1,overflowY:"auto"}}><ExamplesContent onRun={runExample}/></div>}
              {bottomTab==="cheatsheet"&&<div style={{flex:1,overflowY:"auto"}}><CheatSheetContent/></div>}
            </div>
          </div>
        )}
      </div>

      {/* Save Dialog */}
      {showSaveDialog&&(
        <div style={{position:"fixed",inset:0,background:"rgba(0,0,0,0.6)",display:"flex",alignItems:"center",justifyContent:"center",zIndex:1000}} onClick={()=>setShowSaveDialog(false)}>
          <div onClick={e=>e.stopPropagation()} style={{background:C.bg2,border:`1px solid ${C.borderHi}`,borderRadius:8,padding:20,width:320,boxShadow:"0 8px 32px rgba(0,0,0,0.5)"}}>
            <div style={{fontSize:13,fontWeight:600,color:C.text,marginBottom:12}}>Save to Local Files</div>
            <div style={{fontSize:10,color:C.textMuted,marginBottom:8}}>File will be saved to Local Files</div>
            <input value={saveFileName} onChange={e=>setSaveFileName(e.target.value)} autoFocus
              onKeyDown={e=>{if(e.key==="Enter")handleSaveDialogSubmit();if(e.key==="Escape")setShowSaveDialog(false);}}
              placeholder="filename.m" style={{width:"100%",padding:"8px 10px",borderRadius:5,fontSize:12,background:C.bg0,border:`1px solid ${C.border}`,color:C.text,outline:"none",fontFamily:FONT,marginBottom:12,boxSizing:"border-box"}}/>
            <div style={{display:"flex",gap:8,justifyContent:"flex-end"}}>
              <button onClick={()=>setShowSaveDialog(false)} style={{padding:"6px 14px",borderRadius:5,fontSize:11,background:C.bg3,border:`1px solid ${C.border}`,color:C.textDim,cursor:"pointer",fontFamily:FONT_UI}}>Cancel</button>
              <button onClick={handleSaveDialogSubmit} style={{padding:"6px 14px",borderRadius:5,fontSize:11,fontWeight:600,background:C.accent,border:"none",color:"#fff",cursor:"pointer",fontFamily:FONT_UI}}>Save</button>
            </div>
          </div>
        </div>
      )}

      {/* Status Bar */}
      <div style={{display:"flex",justifyContent:"space-between",alignItems:"center",padding:"3px 12px",background:C.bg1,borderTop:`1px solid ${C.border}`,fontSize:9,color:C.textMuted,flexShrink:0}}>
        <div style={{display:"flex",alignItems:"center",gap:5}}>
          <span style={{width:6,height:6,borderRadius:"50%",background:statusProp==="ready"?C.green:C.yellow,display:"inline-block"}}/>
          <span>{statusProp==="ready"?"WASM":"Demo"}</span>
          <span style={{color:C.border}}>|</span>
          <span>{activeTabData?.name}</span>
          {activeTabData?.vfsPath&&<><span style={{color:C.border}}>|</span><span style={{color:C.green}}>📁 local</span></>}
        </div>
        <div style={{display:"flex",alignItems:"center",gap:6}}>
          {execTimeMs!==null&&<span>{execTimeMs.toFixed(1)}ms</span>}
          <span style={{color:C.border}}>|</span><span>Ctrl+S: save</span>
          <span style={{color:C.border}}>|</span><span>Tab: autocomplete</span>
          <span style={{color:C.border}}>|</span><span>↑↓: history</span>
        </div>
      </div>
    </div>
  );
}
