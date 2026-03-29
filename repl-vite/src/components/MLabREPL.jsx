import { useState, useEffect, useRef, useCallback } from "react";
import FileBrowser from "./FileBrowser";
import Console from "./Console";
import Workspace from "./Workspace";
import Reference from "./Reference";
import Figures from "./Figures";
import SyntaxEditor from "./SyntaxEditor";
import vfs from "../vfs";
import { useTheme, FONT, FONT_UI } from "../theme";

function TabBar({ tabs, activeTab, onSelect, onClose, onNew, onRename }) {
  const C = useTheme();
  const [editingId, setEditingId] = useState(null);
  const [editName, setEditName] = useState("");
  return (
    <div style={{display:"flex",alignItems:"center",gap:1,padding:"3px 8px",background:C.bg0,borderBottom:`1px solid ${C.border}`,overflowX:"auto",minHeight:32,flexShrink:0}}>
      {tabs.map(tab=>(
        <div key={tab.id} onClick={()=>onSelect(tab.id)} style={{display:"flex",alignItems:"center",gap:4,padding:"3px 10px",borderRadius:5,cursor:"pointer",fontSize:11,background:tab.id===activeTab?C.bg3:"transparent",color:tab.id===activeTab?C.text:C.textMuted,border:`1px solid ${tab.id===activeTab?C.borderHi:"transparent"}`,whiteSpace:"nowrap",transition:"all 0.15s"}}>
          {editingId===tab.id?<input value={editName} autoFocus onChange={e=>setEditName(e.target.value)} onBlur={()=>{onRename(tab.id,editName);setEditingId(null);}} onKeyDown={e=>{if(e.key==="Enter"){onRename(tab.id,editName);setEditingId(null);}}} style={{background:"transparent",border:"none",color:C.text,fontSize:11,width:80,outline:"none",fontFamily:FONT}} onClick={e=>e.stopPropagation()}/>
          :<span onDoubleClick={e=>{e.stopPropagation();setEditingId(tab.id);setEditName(tab.name);}}>{tab.name}{tab.modified?" •":""}</span>}
          {tabs.length>1&&<span onClick={e=>{e.stopPropagation();onClose(tab.id);}} style={{color:C.textMuted,fontSize:13,lineHeight:1,marginLeft:2,opacity:0.5}}>×</span>}
        </div>
      ))}
      <button onClick={onNew} style={{background:"none",border:`1px dashed ${C.border}`,borderRadius:5,color:C.textMuted,fontSize:13,padding:"1px 8px",cursor:"pointer",lineHeight:1,marginLeft:4}}>+</button>
    </div>
  );
}

export default function MLabREPL({ engine: engineProp, status: statusProp }) {
  const C = useTheme();
  const { themeName, toggleTheme } = C;

  const [showLeft, setShowLeft] = useState(true);
  const [showCenter, setShowCenter] = useState(true);
  const [showRight, setShowRight] = useState(false);
  const [showBottom, setShowBottom] = useState(true);
  const [bottomTab, setBottomTab] = useState("console");
  const [bottomHeight, setBottomHeight] = useState(300);
  const [output, setOutput] = useState([]);
  const [figures, setFigures] = useState([]);
  const [helpTopic, setHelpTopic] = useState(null);
  const [execTimeMs, setExecTimeMs] = useState(null);
  const [variables, setVariables] = useState({});
  const [errorLine, setErrorLine] = useState(null);
  const [tabs, setTabs] = useState([{id:"1",name:"untitled.m",code:"",modified:false,vfsPath:null,source:null}]);
  const [activeTab, setActiveTab] = useState("1");
  const [vfsRefreshKey, setVfsRefreshKey] = useState(0);
  const [showSaveDialog, setShowSaveDialog] = useState(false);
  const [saveFileName, setSaveFileName] = useState("");
  const [forceExplorerSource, setForceExplorerSource] = useState(null);
  const [consoleNotify, setConsoleNotify] = useState(false);
  const [figuresWidth, setFiguresWidth] = useState(360);

  const tabCountRef=useRef(1); const editorRef=useRef(null); const gutterRef=useRef(null); const consoleRef=useRef(null); const resizingRef=useRef(false); const resizingRightRef=useRef(false);
  const engine = engineProp;

  useEffect(()=>{setOutput([{type:"system",text:"MLab REPL v2.5 — Web IDE"},{type:"system",text:'Type commands below. "help <topic>" for function info.'}]);},[]);

  const addOutput=useCallback(items=>{setOutput(prev=>{for(const i of items)if(i.text==="__CLEAR__")return[];return[...prev,...items.filter(i=>i.text!=="__CLEAR__")];});},[]);

  const runCode=useCallback(code=>{
    const t0=performance.now();const result=engine.execute(code);setExecTimeMs(performance.now()-t0);setErrorLine(null);
    const items=[];if(result.output)for(const line of result.output.split("\n")){if(line==="__CLEAR__"){setOutput([]);continue;}items.push({type:/^Error/.test(line)?"error":/^Warning:/.test(line)?"warning":"result",text:line});}
    if(items.length){addOutput(items);setConsoleNotify(true);}
    if(result.closeAllFigures)setFigures([]);else if(result.closedFigureIds?.length){const closed=new Set(result.closedFigureIds);setFigures(prev=>prev.filter(f=>!closed.has(f.id)));}
    if(result.figures?.length){setFigures(prev=>{const map=new Map(prev.map(f=>[f.id,f]));for(const fig of result.figures)map.set(fig.id,fig);return Array.from(map.values());});setShowRight(true);}
    if(result.errorLine)setErrorLine(result.errorLine);setVariables(engine.getVars());
  },[engine,addOutput]);

  const handleBottomTabChange=useCallback(id=>{setBottomTab(id);if(id==="console")setConsoleNotify(false);},[]);
  const handleCloseFigure=useCallback(id=>{engine.execute(`close(${id})`);},[engine]);
  const handleCloseAllFigures=useCallback(()=>{engine.execute("close('all')");},[engine]);

  const newTab=useCallback(()=>{tabCountRef.current++;const id=String(tabCountRef.current);setTabs(p=>[...p,{id,name:`script${tabCountRef.current}.m`,code:"",modified:false,vfsPath:null,source:null}]);setActiveTab(id);},[]);
  const closeTab=useCallback(id=>{setTabs(p=>{const n=p.filter(t=>t.id!==id);if(!n.length)return p;if(activeTab===id)setActiveTab(n[n.length-1].id);return n;});},[activeTab]);
  const renameTab=useCallback((id,name)=>{if(!name.trim())return;setTabs(p=>p.map(t=>t.id===id?{...t,name:name.trim()}:t));},[]);
  const activeTabData=tabs.find(t=>t.id===activeTab)||tabs[0];
  const updateTabCode=useCallback(code=>{setTabs(p=>p.map(t=>t.id===activeTab?{...t,code,modified:true}:t));},[activeTab]);

  const runActiveTab=useCallback(()=>{const tab=tabs.find(t=>t.id===activeTab);if(!tab||!tab.code.trim())return;setShowBottom(true);addOutput([{type:"system",text:`── Running ${tab.name} ──`},{type:"input",text:tab.code}]);setConsoleNotify(true);runCode(tab.code);setTabs(p=>p.map(t=>t.id===activeTab?{...t,modified:false}:t));},[tabs,activeTab,addOutput,runCode]);

  const handleOpenFile=useCallback((filename,content,vfsPath,source)=>{const existing=tabs.find(t=>t.vfsPath&&t.vfsPath===vfsPath);if(existing){setActiveTab(existing.id);return;}tabCountRef.current++;const id=String(tabCountRef.current);setTabs(p=>[...p,{id,name:filename,code:content,modified:false,vfsPath:vfsPath||null,source:source||null}]);setActiveTab(id);setShowCenter(true);},[tabs]);
  const handleSaveToVFS=useCallback(async(path,name)=>{const tab=tabs.find(t=>t.id===activeTab);if(!tab)return;const fullPath=path||tab.vfsPath;if(!fullPath)return;await vfs.writeFile(fullPath,tab.code);setTabs(p=>p.map(t=>t.id===activeTab?{...t,modified:false,vfsPath:fullPath,name:name||t.name,source:'local'}:t));setVfsRefreshKey(k=>k+1);addOutput([{type:"system",text:`Saved ${name||tab.name}`}]);setConsoleNotify(true);},[tabs,activeTab,addOutput]);
  const handleSave=useCallback(()=>{const tab=tabs.find(t=>t.id===activeTab);if(!tab)return;if(tab.vfsPath)handleSaveToVFS(tab.vfsPath,tab.name);else{setSaveFileName(tab.name);setShowSaveDialog(true);}},[tabs,activeTab,handleSaveToVFS]);
  const handleSaveDialogSubmit=useCallback(async()=>{if(!saveFileName.trim())return;let name=saveFileName.trim();if(!name.includes('.'))name+='.m';await handleSaveToVFS(`/${name}`,name);setShowSaveDialog(false);},[saveFileName,handleSaveToVFS]);
  const handleDownload=useCallback(()=>{const tab=tabs.find(t=>t.id===activeTab);if(!tab)return;const blob=new Blob([tab.code],{type:"text/plain"});const url=URL.createObjectURL(blob);const a=document.createElement("a");a.href=url;a.download=tab.name;a.click();URL.revokeObjectURL(url);},[tabs,activeTab]);
  const handleImport=useCallback(()=>{const input=document.createElement("input");input.type="file";input.accept=".m,.txt";input.onchange=async e=>{const file=e.target.files[0];if(!file)return;const r=new FileReader();r.onload=async ev=>{const content=ev.target.result;const name=file.name.includes('.')?file.name:file.name+'.m';const vfsPath=`/${name}`;await vfs.writeFile(vfsPath,content);setVfsRefreshKey(k=>k+1);handleOpenFile(name,content,vfsPath,'local');setShowLeft(true);setForceExplorerSource('local');setTimeout(()=>setForceExplorerSource(null),100);addOutput([{type:"system",text:`Imported ${name}`}]);setConsoleNotify(true);};r.readAsText(file);};input.click();},[handleOpenFile,addOutput]);

  useEffect(()=>{const h=e=>{if((e.ctrlKey||e.metaKey)&&e.key==='s'){e.preventDefault();handleSave();}};window.addEventListener('keydown',h);return()=>window.removeEventListener('keydown',h);},[handleSave]);
  const handleResizeStart=useCallback(e=>{e.preventDefault();resizingRef.current=true;const sY=e.clientY,sH=bottomHeight;const onM=ev=>{if(!resizingRef.current)return;setBottomHeight(Math.max(100,Math.min(window.innerHeight*0.7,sH+sY-ev.clientY)));};const onU=()=>{resizingRef.current=false;document.removeEventListener('mousemove',onM);document.removeEventListener('mouseup',onU);};document.addEventListener('mousemove',onM);document.addEventListener('mouseup',onU);},[bottomHeight]);
  const handleRightResizeStart=useCallback(e=>{e.preventDefault();resizingRightRef.current=true;const sX=e.clientX,sW=figuresWidth;const onM=ev=>{if(!resizingRightRef.current)return;setFiguresWidth(Math.max(200,Math.min(window.innerWidth*0.5,sW+sX-ev.clientX)));};const onU=()=>{resizingRightRef.current=false;document.removeEventListener('mousemove',onM);document.removeEventListener('mouseup',onU);};document.addEventListener('mousemove',onM);document.addEventListener('mouseup',onU);},[figuresWidth]);
  const handleEditorScroll=useCallback(scrollTop=>{if(gutterRef.current)gutterRef.current.scrollTop=scrollTop;},[]);

  const PanelBtn=({active,onClick,icon,label,title,notify})=>(<button onClick={onClick} title={title} style={{display:"flex",alignItems:"center",gap:4,padding:"4px 8px",border:"none",borderRadius:4,background:active?`${C.accent}25`:"transparent",color:active?C.accent:C.textMuted,fontFamily:FONT_UI,fontSize:11,fontWeight:500,cursor:"pointer",transition:"all 0.15s",whiteSpace:"nowrap",position:"relative"}} onMouseEnter={e=>{if(!active)e.currentTarget.style.background=C.bg3;}} onMouseLeave={e=>{if(!active)e.currentTarget.style.background="transparent";}}><span style={{fontSize:13}}>{icon}</span>{label}{notify&&<span style={{width:6,height:6,borderRadius:"50%",background:C.green,position:"absolute",top:2,right:2}}/>}</button>);
  const ActBtn=({onClick,icon,label,color,title})=>(<button onClick={onClick} title={title} style={{display:"flex",alignItems:"center",gap:3,padding:"4px 8px",border:`1px solid ${C.border}`,borderRadius:4,background:C.bg2,color:color||C.textDim,fontFamily:FONT_UI,fontSize:11,fontWeight:500,cursor:"pointer",transition:"all 0.15s",whiteSpace:"nowrap"}} onMouseEnter={e=>e.currentTarget.style.borderColor=C.borderHi} onMouseLeave={e=>e.currentTarget.style.borderColor=C.border}><span style={{fontSize:12}}>{icon}</span>{label}</button>);
  const bottomTabBtn=(id,label,notify)=>(<button onClick={()=>handleBottomTabChange(id)} style={{padding:"5px 12px",border:"none",borderBottom:bottomTab===id?`2px solid ${C.accent}`:"2px solid transparent",background:"transparent",color:notify&&bottomTab!==id?C.green:bottomTab===id?C.text:C.textMuted,fontFamily:FONT_UI,fontSize:11,fontWeight:(notify&&bottomTab!==id)||bottomTab===id?600:400,cursor:"pointer",transition:"all 0.15s"}}>{label}</button>);

  return (
    <div style={{display:"flex",flexDirection:"column",height:"100vh",width:"100%",background:C.bg0,color:C.text,fontFamily:FONT,fontSize:13,overflow:"hidden"}}>
      <div style={{display:"flex",alignItems:"center",justifyContent:"space-between",padding:"5px 12px",background:C.bg1,borderBottom:`1px solid ${C.border}`,flexShrink:0,zIndex:30,gap:8}}>
        <div style={{display:"flex",alignItems:"baseline",gap:6,flexShrink:0}}><span style={{fontSize:15,fontWeight:700,letterSpacing:-0.5,fontFamily:FONT_UI}}>MLab <span style={{color:C.accent}}>IDE</span></span><span style={{fontSize:9,color:C.textMuted}}>v2.5</span></div>
        <div style={{display:"flex",gap:2,alignItems:"center",background:C.bg0,borderRadius:6,padding:"2px 3px"}}>
          <PanelBtn active={showLeft} onClick={()=>setShowLeft(!showLeft)} icon="📂" label="Explorer" title="File Browser"/>
          <PanelBtn active={showCenter} onClick={()=>setShowCenter(!showCenter)} icon="📝" label="Editor" title="Code Editor"/>
          <PanelBtn active={showRight} onClick={()=>setShowRight(!showRight)} icon="📊" label="Figures" title="Plot Figures" notify={figures.length>0&&!showRight}/>
          <PanelBtn active={showBottom} onClick={()=>setShowBottom(!showBottom)} icon="💻" label="Terminal" title="Bottom Panel"/>
        </div>
        <div style={{display:"flex",gap:3,flexShrink:0}}>
          {showCenter&&<ActBtn onClick={runActiveTab} icon="▶" label="Run" color={C.green} title="Run current script"/>}
          {showCenter&&<ActBtn onClick={handleSave} icon="💾" label="Save" title="Ctrl+S"/>}
          <ActBtn onClick={handleImport} icon="📥" label="Import" title="Import file"/>
          {showCenter&&<ActBtn onClick={handleDownload} icon="⬇" label="Export" title="Download"/>}
          <ActBtn onClick={()=>setOutput([])} icon="🗑" label="Clear" title="Clear console"/>
          <ActBtn onClick={()=>{engine.reset();setVariables({});addOutput([{type:"system",text:"Workspace cleared."}]);setConsoleNotify(true);}} icon="🔄" label="Reset" title="Reset workspace"/>
          <button onClick={toggleTheme} title={`Switch to ${themeName==='dark'?'light':'dark'} theme`}
            style={{display:"flex",alignItems:"center",gap:3,padding:"4px 8px",border:`1px solid ${C.border}`,borderRadius:4,background:C.bg2,color:C.textDim,fontFamily:FONT_UI,fontSize:11,fontWeight:500,cursor:"pointer",whiteSpace:"nowrap"}}>
            <span style={{fontSize:12}}>{themeName==='dark'?'☀️':'🌙'}</span>{themeName==='dark'?'Light':'Dark'}
          </button>
        </div>
      </div>

      <div style={{flex:1,display:"flex",flexDirection:"column",overflow:"hidden"}}>
        <div style={{flex:1,display:"flex",overflow:"hidden",minHeight:0}}>
          {showLeft&&<div style={{width:280,minWidth:220,flexShrink:0,background:C.bg1,borderRight:`1px solid ${C.border}`,display:"flex",flexDirection:"column",overflow:"hidden"}}><FileBrowser onOpenFile={handleOpenFile} defaultGitHubRepo="skynetjudgmentday/mlab-demo" vfsRefreshKey={vfsRefreshKey} forceSource={forceExplorerSource}/></div>}
          {showCenter&&<div style={{flex:1,display:"flex",flexDirection:"column",overflow:"hidden",minWidth:0}}>
            <TabBar tabs={tabs} activeTab={activeTab} onSelect={setActiveTab} onClose={closeTab} onNew={newTab} onRename={renameTab}/>
            <div style={{flex:1,display:"flex",overflow:"hidden",position:"relative"}}>
              <div ref={gutterRef} style={{padding:"8px 0",background:C.bg0,borderRight:`1px solid ${C.border}`,userSelect:"none",minWidth:34,textAlign:"right",overflowY:"hidden",flexShrink:0}}>
                {(activeTabData?.code||"").split("\n").map((_,i)=>{const ln=i+1,isErr=errorLine===ln;return<div key={i} style={{fontSize:10,padding:"0 6px",lineHeight:"20px",color:isErr?C.red:C.textMuted,background:isErr?`${C.red}18`:"transparent",fontWeight:isErr?700:400}}>{ln}</div>;})}
              </div>
              <div style={{flex:1,overflow:"hidden",background:C.bg1}}>
                <SyntaxEditor ref={editorRef} value={activeTabData?.code||""} onChange={val=>{updateTabCode(val);setErrorLine(null);}} onScroll={handleEditorScroll} errorLine={errorLine}/>
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
          {activeTabData?.vfsPath&&<><span style={{color:C.border}}>|</span><span style={{color:C.green}}>📁 local</span></>}
          {figures.length>0&&<><span style={{color:C.border}}>|</span><span>{figures.length} figure{figures.length>1?"s":""}</span></>}
        </div>
        <div style={{display:"flex",alignItems:"center",gap:6}}>
          {execTimeMs!==null&&<span>{execTimeMs.toFixed(1)}ms</span>}
          <span style={{color:C.border}}>|</span><span>Ctrl+S: save</span><span style={{color:C.border}}>|</span><span>Tab: autocomplete</span><span style={{color:C.border}}>|</span><span>↑↓: history</span>
        </div>
      </div>
    </div>
  );
}