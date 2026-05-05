import { useState, useEffect, useRef, useCallback, forwardRef, useImperativeHandle } from "react";
import HELP_DB from "../data/help";
import { useTheme, FONT } from "../theme";

const Console = forwardRef(function Console({ engine, output, onAddOutput, onRunCode, helpTopic, onSetHelpTopic }, ref) {
  const C = useTheme();
  const [inputVal, setInputVal] = useState("");
  const [history, setHistory] = useState([]);
  const [histIdx, setHistIdx] = useState(-1);
  const [savedInput, setSavedInput] = useState("");
  const [acItems, setAcItems] = useState([]);
  const [acIdx, setAcIdx] = useState(-1);
  const [acPartial, setAcPartial] = useState("");

  const outputRef = useRef(null);
  const inputRef = useRef(null);

  useImperativeHandle(ref, () => ({ focus: () => inputRef.current?.focus() }));

  useEffect(() => {
    requestAnimationFrame(() => { if (outputRef.current) outputRef.current.scrollTop = outputRef.current.scrollHeight; });
  }, [output]);

  const handleSubmit = useCallback(() => {
    const val = inputVal.trim(); if (!val) return;
    onAddOutput([{ type: "input", text: val }]);
    setHistory(prev => { const h = [...prev, val]; return h.length > 200 ? h.slice(-200) : h; });
    setHistIdx(-1); setInputVal(""); setAcItems([]);
    const hm = val.match(/^help\s+(\w+)$/);
    if (hm && HELP_DB[hm[1]]) { onSetHelpTopic(hm[1]); return; }
    if (val === "help") { onSetHelpTopic(null); onAddOutput([{ type: "system", text: "Commands: clc, clear, who, whos, help <topic>" }]); return; }
    onRunCode(val);
  }, [inputVal, onAddOutput, onRunCode, onSetHelpTopic]);

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
    if (e.key === "l" && e.ctrlKey) { e.preventDefault(); onAddOutput([{text:"__CLEAR__"}]); }
  }, [inputVal, handleSubmit, history, histIdx, savedInput, acItems, acIdx, engine, onAddOutput]);

  return (
    <div style={{ flex:1,display:"flex",flexDirection:"column",overflow:"hidden" }}>
      <div ref={outputRef} style={{ flex:1,overflowY:"auto",padding:"8px 12px",background:C.bg1 }}>
        {output.map((item,i)=>{
          const clr={input:C.textMuted,result:C.text,error:C.red,warning:C.orange,system:C.textMuted,info:C.cyan};
          if(item.type==="input")return<div key={i} style={{padding:"1px 0",whiteSpace:"pre-wrap",wordBreak:"break-word"}}><span style={{color:C.green,fontWeight:700,userSelect:"none"}}>{">> "}</span><span style={{color:C.textMuted}}>{item.text}</span></div>;
          return<div key={i} style={{padding:"1px 0",color:clr[item.type]||C.text,fontStyle:item.type==="system"?"italic":"normal",whiteSpace:"pre-wrap",wordBreak:"break-word"}}>{item.text}</div>;
        })}
        {helpTopic&&HELP_DB[helpTopic]&&(
          <div style={{background:C.bg2,border:`1px solid ${C.borderHi}`,borderRadius:6,padding:"8px 12px",margin:"4px 0",position:"relative"}}>
            <button onClick={()=>onSetHelpTopic(null)} style={{position:"absolute",top:4,right:6,background:"none",border:"none",color:C.textMuted,cursor:"pointer",fontSize:14}}>×</button>
            <div style={{fontSize:13,fontWeight:700,color:C.accent,marginBottom:3}}>{HELP_DB[helpTopic].sig}</div>
            <div style={{fontSize:11,color:C.text,marginBottom:3}}>{HELP_DB[helpTopic].desc}</div>
            <div style={{fontSize:10,color:C.textMuted}}>Category: {HELP_DB[helpTopic].cat}</div>
            <div style={{fontSize:11,color:C.green,marginTop:3,fontFamily:FONT}}>{HELP_DB[helpTopic].ex}</div>
          </div>
        )}
      </div>
      <div style={{display:"flex",alignItems:"flex-start",padding:"8px 12px",background:C.bg0,borderTop:`1px solid ${C.border}`,flexShrink:0,position:"relative"}}>
        <span style={{color:C.green,fontWeight:700,marginRight:6,marginTop:2,userSelect:"none",flexShrink:0,fontSize:13}}>&gt;&gt;</span>
        <div style={{flex:1,position:"relative"}}>
          {acItems.length>1&&(
            <div style={{position:"absolute",bottom:"calc(100% + 4px)",left:0,minWidth:160,maxWidth:320,maxHeight:160,overflowY:"auto",background:C.bg3,border:`1px solid ${C.border}`,borderRadius:5,boxShadow:"0 -4px 16px rgba(0,0,0,0.3)",zIndex:100}}>
              {acItems.map((item,i)=>(
                <div key={item} onClick={()=>{const val=inputVal,cur=inputRef.current?.selectionStart||val.length;let ws=cur-1;while(ws>=0&&/[a-zA-Z0-9_]/.test(val[ws]))ws--;ws++;setInputVal(val.substring(0,ws)+item+val.substring(cur));setAcItems([]);inputRef.current?.focus();}}
                  style={{padding:"4px 8px",cursor:"pointer",fontSize:11,color:i===acIdx?C.text:C.textDim,background:i===acIdx?C.border:"transparent"}}>
                  <span style={{color:C.accent,fontWeight:600}}>{item.substring(0,acPartial.length)}</span>{item.substring(acPartial.length)}
                </div>
              ))}
            </div>
          )}
          <textarea ref={inputRef} value={inputVal} onChange={e=>{setInputVal(e.target.value);setAcItems([]);}}
            onKeyDown={handleKeyDown} rows={1} spellCheck={false} autoComplete="off" placeholder="Enter numkit command…"
            style={{width:"100%",background:"transparent",border:"none",outline:"none",color:C.text,fontFamily:FONT,fontSize:13,lineHeight:1.6,resize:"none",overflow:"hidden",caretColor:C.accent}}
            onInput={e=>{e.target.style.height="auto";e.target.style.height=e.target.scrollHeight+"px";}}/>
        </div>
      </div>
    </div>
  );
});

export default Console;
