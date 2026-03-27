import { useEffect, useRef } from "react";
import * as d3 from "d3";
import C, { FONT_UI } from "../theme";

// ── Single Plot ──
function PlotPanel({ data, index, onClose }) {
  const svgRef = useRef(null);
  const containerRef = useRef(null);
  useEffect(() => {
    if (!data || !svgRef.current || !containerRef.current) return;
    const svg = d3.select(svgRef.current); svg.selectAll("*").remove();
    const cw = containerRef.current.clientWidth;
    const width = Math.max(200, cw - 16), height = 200;
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
    <div ref={containerRef} style={{background:C.bg2,border:`1px solid ${C.border}`,borderRadius:6,padding:6,position:"relative"}}>
      <div style={{display:"flex",justifyContent:"space-between",alignItems:"center",marginBottom:4,padding:"0 2px"}}>
        <span style={{fontSize:9,color:C.textMuted}}>Figure {index + 1}</span>
        <button onClick={onClose} style={{background:"none",border:"none",color:C.textMuted,cursor:"pointer",fontSize:13,lineHeight:1}}>×</button>
      </div>
      <svg ref={svgRef} style={{display:"block",margin:"0 auto"}} />
    </div>
  );
}

/**
 * Figures — right panel showing all plots.
 * Props:
 *   plots     — array of plot data objects
 *   onSetPlots — setter to remove individual plots
 *   onClose   — close the panel
 */
export default function Figures({ plots, onSetPlots, onClose }) {
  return (
    <div style={{display:"flex",flexDirection:"column",height:"100%",overflow:"hidden"}}>
      <div style={{padding:"7px 10px",borderBottom:`1px solid ${C.border}`,display:"flex",justifyContent:"space-between",alignItems:"center",flexShrink:0}}>
        <span style={{fontSize:11,fontWeight:600,color:C.text,fontFamily:FONT_UI}}>📊 Figures</span>
        <div style={{display:"flex",gap:4,alignItems:"center"}}>
          {plots.length > 0 && (
            <button onClick={()=>onSetPlots([])} title="Clear all figures"
              style={{background:"none",border:"none",color:C.textMuted,cursor:"pointer",fontSize:10,fontFamily:FONT_UI}}>Clear all</button>
          )}
          <button onClick={onClose} style={{background:"none",border:"none",color:C.textMuted,cursor:"pointer",fontSize:16,lineHeight:1}}>×</button>
        </div>
      </div>
      <div style={{flex:1,overflowY:"auto",padding:8,display:"flex",flexDirection:"column",gap:8}}>
        {plots.length === 0 ? (
          <div style={{color:C.textMuted,fontSize:11,padding:16,textAlign:"center",lineHeight:1.6}}>
            No figures yet.<br/>Run plot(), bar(), scatter() or hist() to see charts here.
          </div>
        ) : (
          plots.map((p, i) => (
            <PlotPanel key={i} data={p} index={i} onClose={() => onSetPlots(prev => prev.filter((_, j) => j !== i))} />
          ))
        )}
      </div>
    </div>
  );
}
