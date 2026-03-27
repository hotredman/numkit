import { useState, useEffect, useRef, useCallback, forwardRef, useImperativeHandle } from "react";
import * as d3 from "d3";
import HELP_DB from "../data/help";
import C, { FONT } from "../theme";

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
      if (data.config.type === "line") {
        g.append("path").datum(ds.y).attr("d", d3.line().x((_,i)=>xScale(ds.x[i])).y((_,i)=>yScale(ds.y[i])).curve(d3.curveMonotoneX)).attr("fill","none").attr("stroke",color).attr("stroke-width",2);
      } else if (data.config.type === "scatter") {
        g.selectAll(`.dot-${idx}`).data(ds.x.map((x,i)=>({x,y:ds.y[i]}))).enter().append("circle").attr("cx",d=>xScale(d.x)).attr("cy",d=>yScale(d.y)).attr("r",4).attr("fill",color).attr("opacity",0.8);
      } else if (data.config.type === "bar") {
        const bw = Math.max(2, iw/ds.x.length*0.7);
        g.selectAll(`.bar-${idx}`).data(ds.x.map((x,i)=>({x,y:ds.y[i]}))).enter().append("rect").attr("x",d=>xScale(d.x)-bw/2).attr("y",d=>yScale(d.y)).attr("width",bw).attr("height",d=>ih-yScale(d.y)).attr("fill",color).attr("opacity",0.85).attr("rx",2);
      }
    });
    if (data.config.title) svg.append("text").attr("x",width/2).attr("y",16).attr("text-anchor","middle").attr("fill",C.text).attr("font-size",12).attr("font-weight",600).text(data.config.title);
    if (data.config.xlabel) svg.append("text").attr("x",width/2).attr("y",height-4).attr("text-anchor","middle").attr("fill",C.textMuted).attr("font-size",10).text(data.config.xlabel);
    if (data.config.ylabel) svg.append("text").attr("transform",`translate(12,${height/2}) rotate(-90)`).attr("text-anchor","middle").attr("fill",C.textMuted).attr("font-size",10).text(data.config.ylabel);
  }, [data]);
  if (!data) return null;
  return (
    <div ref={containerRef} style={{background:C.bg1,border:`1px solid ${C.border}`,borderRadius:6,margin:"4px 0",padding:6,position:"relative"}}>
      <button onClick={onClose} style={{position:"absolute",top:4,right:6,background:"none",border:"none",color:C.textMuted,cursor:"pointer",fontSize:14,lineHeight:1}}>×</button>
      <svg ref={svgRef} style={{display:"block",margin:"0 auto"}} />
    </div>
  );
}

/**
 * Console — terminal with input, output, plots, help, autocomplete.
 *
 * Props:
 *   engine        — the MLab engine (execute, complete, reset, getVars)
 *   output        — output lines array [{type, text}]
 *   onAddOutput   — (items) => void
 *   onRunCode     — (code) => void
 *   plots         — plot data array
 *   onSetPlots    — setter
 *   helpTopic     — current help topic string | null
 *   onSetHelpTopic — setter
 */
const Console = forwardRef(function Console({ engine, output, onAddOutput, onRunCode, plots, onSetPlots, helpTopic, onSetHelpTopic }, ref) {
  const [inputVal, setInputVal] = useState("");
  const [history, setHistory] = useState([]);
  const [histIdx, setHistIdx] = useState(-1);
  const [savedInput, setSavedInput] = useState("");
  const [acItems, setAcItems] = useState([]);
  const [acIdx, setAcIdx] = useState(-1);
  const [acPartial, setAcPartial] = useState("");

  const outputRef = useRef(null);
  const inputRef = useRef(null);

  // Expose focus method to parent
  useImperativeHandle(ref, () => ({
    focus: () => inputRef.current?.focus(),
  }));

  // Auto-scroll
  useEffect(() => {
    requestAnimationFrame(() => { if (outputRef.current) outputRef.current.scrollTop = outputRef.current.scrollHeight; });
  }, [output, plots]);

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
    if (e.key === "l" && e.ctrlKey) { e.preventDefault(); onAddOutput([{text:"__CLEAR__"}]); onSetPlots([]); }
  }, [inputVal, handleSubmit, history, histIdx, savedInput, acItems, acIdx, engine, onAddOutput, onSetPlots]);

  return (
    <div style={{ flex:1,display:"flex",flexDirection:"column",overflow:"hidden" }}>
      {/* Output */}
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
        {plots.map((p,i)=><PlotPanel key={i} data={p} onClose={()=>onSetPlots(prev=>prev.filter((_,j)=>j!==i))}/>)}
      </div>
      {/* Input */}
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
  );
});

export default Console;
