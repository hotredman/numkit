import { useEffect, useRef } from "react";
import * as d3 from "d3";
import C, { FONT_UI } from "../theme";

const COLORS = ['#7c6ff0','#60d0f0','#6ee7a0','#f0a060','#e070c0','#e8d060','#f07070','#70b0f0'];

function parseStyleColor(s) { if(!s)return null; const m={r:'#f07070',g:'#6ee7a0',b:'#60d0f0',k:'#d4d4f0',m:'#e070c0',c:'#60d0f0',y:'#e8d060',w:'#ffffff'}; for(const c of s){if(m[c])return m[c];} return null; }
function parseStyleDash(s) { if(!s)return null; if(s.includes('-.'))return'8,3,2,3'; if(s.includes('--'))return'8,4'; if(s.includes(':'))return'2,4'; return null; }
function parseStyleMarker(s) { if(!s)return null; const ms=['o','s','d','^','v','<','>','p','h','*','+','x','.']; for(const c of s){if(ms.includes(c))return c;} return null; }
function parseStyleHasLine(s) { if(!s)return true; if(s.includes('-')||s.includes(':'))return true; if(parseStyleMarker(s))return false; return true; }

function drawMarker(g, cx, cy, marker, color, size) {
  const r = size || 3;
  switch (marker) {
    case '.': g.append("circle").attr("cx",cx).attr("cy",cy).attr("r",r*0.5).attr("fill",color); break;
    case 'o': g.append("circle").attr("cx",cx).attr("cy",cy).attr("r",r).attr("fill","none").attr("stroke",color).attr("stroke-width",1.5); break;
    case '*': g.append("text").attr("x",cx).attr("y",cy).attr("text-anchor","middle").attr("alignment-baseline","central").attr("fill",color).attr("font-size",r*3).text("*"); break;
    case '+': g.append("line").attr("x1",cx-r).attr("y1",cy).attr("x2",cx+r).attr("y2",cy).attr("stroke",color).attr("stroke-width",1.5); g.append("line").attr("x1",cx).attr("y1",cy-r).attr("x2",cx).attr("y2",cy+r).attr("stroke",color).attr("stroke-width",1.5); break;
    case 'x': g.append("line").attr("x1",cx-r).attr("y1",cy-r).attr("x2",cx+r).attr("y2",cy+r).attr("stroke",color).attr("stroke-width",1.5); g.append("line").attr("x1",cx-r).attr("y1",cy+r).attr("x2",cx+r).attr("y2",cy-r).attr("stroke",color).attr("stroke-width",1.5); break;
    case 's': g.append("rect").attr("x",cx-r).attr("y",cy-r).attr("width",r*2).attr("height",r*2).attr("fill","none").attr("stroke",color).attr("stroke-width",1.5); break;
    case 'd': g.append("polygon").attr("points",`${cx},${cy-r} ${cx+r},${cy} ${cx},${cy+r} ${cx-r},${cy}`).attr("fill","none").attr("stroke",color).attr("stroke-width",1.5); break;
    case '^': g.append("polygon").attr("points",`${cx},${cy-r} ${cx+r},${cy+r} ${cx-r},${cy+r}`).attr("fill","none").attr("stroke",color).attr("stroke-width",1.5); break;
    case 'v': g.append("polygon").attr("points",`${cx},${cy+r} ${cx+r},${cy-r} ${cx-r},${cy-r}`).attr("fill","none").attr("stroke",color).attr("stroke-width",1.5); break;
    default: g.append("circle").attr("cx",cx).attr("cy",cy).attr("r",r).attr("fill",color);
  }
}

function thetaZeroOffset(loc) {
  switch(loc){ case 'top':return Math.PI/2; case 'left':return Math.PI; case 'bottom':return -Math.PI/2; default:return 0; }
}

let clipIdCounter = 0;

/**
 * Normalize figure data — support both old format (datasets+config at top)
 * and new format (axes array). Returns array of axes objects.
 */
function getAxesList(figure) {
  if (figure.axes && Array.isArray(figure.axes)) return figure.axes;
  // Legacy: flat datasets + config → single axes
  if (figure.datasets) return [{ datasets: figure.datasets, config: figure.config || {} }];
  return [{ datasets: [], config: {} }];
}

/**
 * Render one axes panel (polar or cartesian) into an SVG group.
 * ax = { datasets, config, subplotIndex? }
 * Returns the used height.
 */
function renderAxes(svg, ax, ox, oy, availW, availH) {
  const cfg = ax.config || {};
  const clipId = `clip-${++clipIdCounter}`;
  const defs = svg.select("defs").empty() ? svg.append("defs") : svg.select("defs");

  // ── Polar ──
  if (cfg.polar) {
    const size = Math.min(availW, availH);
    const radius = size / 2 - 40;
    const cx = ox + availW / 2, cy = oy + size / 2 + (cfg.title ? 16 : 0);
    const g = svg.append("g").attr("transform", `translate(${cx},${cy})`);

    const thetaDirSign = cfg.thetaDir === 'clockwise' ? -1 : 1;
    const zeroOffset = thetaZeroOffset(cfg.thetaZeroLocation);

    let maxRho = 0;
    (ax.datasets||[]).forEach(ds => ds.y.forEach(v => { if(v!==null&&Math.abs(v)>maxRho) maxRho=Math.abs(v); }));
    if (maxRho === 0) maxRho = 1;
    let rMin = 0, rMax = maxRho;
    if (cfg.rlim?.length >= 2) { rMin = cfg.rlim[0]; rMax = cfg.rlim[1]; }
    const rScale = d3.scaleLinear().domain([rMin, rMax]).range([0, radius]).nice();
    const niceMax = rScale.domain()[1];

    defs.append("clipPath").attr("id", clipId).append("circle").attr("cx",0).attr("cy",0).attr("r",rScale(niceMax));

    rScale.ticks(4).filter(t=>t>0).forEach(t => {
      g.append("circle").attr("r",rScale(t)).attr("fill","none").attr("stroke",C.border).attr("stroke-dasharray","2,4");
      g.append("text").attr("x",3).attr("y",-rScale(t)-2).attr("fill",C.textMuted).attr("font-size",8).text(t);
    });
    g.append("circle").attr("r",rScale(niceMax)).attr("fill","none").attr("stroke",C.border).attr("stroke-width",0.5);
    for (let deg=0;deg<360;deg+=30) {
      const sa = zeroOffset+thetaDirSign*(deg*Math.PI/180);
      const gx=Math.cos(sa), gy=-Math.sin(sa);
      g.append("line").attr("x1",0).attr("y1",0).attr("x2",rScale(niceMax)*gx).attr("y2",rScale(niceMax)*gy).attr("stroke",C.border).attr("stroke-dasharray","2,4");
      g.append("text").attr("x",(rScale(niceMax)+12)*gx).attr("y",(rScale(niceMax)+12)*gy).attr("text-anchor","middle").attr("alignment-baseline","middle").attr("fill",C.textMuted).attr("font-size",8).text(`${deg}°`);
    }
    const dataG = g.append("g").attr("clip-path",`url(#${clipId})`);
    (ax.datasets||[]).forEach((ds, idx) => {
      const color=parseStyleColor(ds.style)||COLORS[idx%COLORS.length]; const marker=parseStyleMarker(ds.style); const hasLine=parseStyleHasLine(ds.style); const dasharray=parseStyleDash(ds.style); const lw=ds.lineWidth||2; const ms=ds.markerSize||3;
      const points = ds.x.map((theta,i) => { if(ds.y[i]===null)return null; const r=rScale(ds.y[i]); const sa=zeroOffset+thetaDirSign*theta; return[r*Math.cos(sa),-r*Math.sin(sa)]; }).filter(Boolean);
      if (hasLine && points.length>1) {
        const thetaRange = ds.x.length>1 ? Math.abs(ds.x[ds.x.length-1]-ds.x[0]) : 0;
        const curve = thetaRange >= Math.PI*1.9 ? d3.curveLinearClosed : d3.curveLinear;
        dataG.append("path").datum(points).attr("d",d3.line().x(d=>d[0]).y(d=>d[1]).curve(curve)).attr("fill","none").attr("stroke",color).attr("stroke-width",lw).attr("stroke-dasharray",dasharray);
      }
      if (marker) points.forEach(p => drawMarker(dataG, p[0], p[1], marker, color, ms));
    });
    if (cfg.title) svg.append("text").attr("x",cx).attr("y",oy+12).attr("text-anchor","middle").attr("fill",C.text).attr("font-size",11).attr("font-weight",600).text(cfg.title);
    return size + (cfg.title ? 20 : 0);
  }

  // ── Cartesian ──
  const hasTitle = !!cfg.title, hasXLabel = !!cfg.xlabel, hasYLabel = !!cfg.ylabel;
  const hasLegend = cfg.legend?.length > 0 || (ax.datasets||[]).some(ds => ds.label);
  const margin = { top: hasTitle?28:12, right:12, bottom: hasXLabel?36:24, left: hasYLabel?46:36 };
  const iw = availW - margin.left - margin.right;
  const ih = availH - margin.top - margin.bottom - (hasLegend?20:0);
  if (iw < 20 || ih < 20) return availH;

  const g = svg.append("g").attr("transform",`translate(${ox+margin.left},${oy+margin.top})`);

  const allX = (ax.datasets||[]).flatMap(ds=>ds.x).filter(v=>v!==null);
  const allY = (ax.datasets||[]).flatMap(ds=>ds.y).filter(v=>v!==null);
  if (!allX.length || !allY.length) return availH;
  let xMin=Math.min(...allX), xMax=Math.max(...allX), yMin=Math.min(...allY), yMax=Math.max(...allY);

  if(cfg.xlim?.length>=2){xMin=cfg.xlim[0];xMax=cfg.xlim[1];}
  if(cfg.ylim?.length>=2){yMin=cfg.ylim[0];yMax=cfg.ylim[1];}
  else{const p=(yMax-yMin)*0.05||1;yMin-=p;yMax+=p;}

  const axisMode = cfg.axisMode;
  if (axisMode==='equal') {
    // True equal: 1 data unit = same pixels on both axes
    const xRange = xMax-xMin||1, yRange = yMax-yMin||1;
    const dataAspect = xRange / yRange;
    const viewAspect = iw / ih;
    if (dataAspect > viewAspect) {
      // X fills width, expand Y
      const newYRange = xRange * ih / iw;
      const yMid = (yMin+yMax)/2;
      yMin = yMid - newYRange/2; yMax = yMid + newYRange/2;
    } else {
      // Y fills height, expand X
      const newXRange = yRange * iw / ih;
      const xMid = (xMin+xMax)/2;
      xMin = xMid - newXRange/2; xMax = xMid + newXRange/2;
    }
  } else if (axisMode==='tight') {
    if(!cfg.ylim){yMin=Math.min(...allY);yMax=Math.max(...allY);}
  }

  const useLogX=cfg.xscale==='log', useLogY=cfg.yscale==='log', flipY=axisMode==='ij';
  let xScale, yScale;
  if(useLogX){if(xMin<=0)xMin=Math.min(...allX.filter(v=>v>0))||0.001;xScale=d3.scaleLog().domain([xMin,xMax]).range([0,iw]).nice();}
  else{xScale=d3.scaleLinear().domain([xMin,xMax]).range([0,iw]).nice();}
  if(useLogY){if(yMin<=0)yMin=Math.min(...allY.filter(v=>v>0))||0.001;yScale=d3.scaleLog().domain([yMin,yMax]).range(flipY?[0,ih]:[ih,0]).nice();}
  else{yScale=d3.scaleLinear().domain([yMin,yMax]).range(flipY?[0,ih]:[ih,0]).nice();}

  defs.append("clipPath").attr("id",clipId).append("rect").attr("x",0).attr("y",0).attr("width",iw).attr("height",ih);

  if(cfg.grid){
    g.append("g").selectAll("line").data(yScale.ticks(5)).enter().append("line").attr("x1",0).attr("x2",iw).attr("y1",d=>yScale(d)).attr("y2",d=>yScale(d)).attr("stroke",C.border).attr("stroke-dasharray","3,3").attr("opacity",0.6);
    g.append("g").selectAll("line").data(xScale.ticks(5)).enter().append("line").attr("x1",d=>xScale(d)).attr("x2",d=>xScale(d)).attr("y1",0).attr("y2",ih).attr("stroke",C.border).attr("stroke-dasharray","3,3").attr("opacity",0.6);
  } else {
    g.append("g").selectAll("line").data(yScale.ticks(4)).enter().append("line").attr("x1",0).attr("x2",iw).attr("y1",d=>yScale(d)).attr("y2",d=>yScale(d)).attr("stroke",C.border).attr("stroke-dasharray","2,4");
  }
  g.append("g").attr("transform",`translate(0,${ih})`).call(d3.axisBottom(xScale).ticks(5)).selectAll("text,line,path").attr("fill",C.textMuted).attr("stroke",C.textMuted);
  g.append("g").call(d3.axisLeft(yScale).ticks(4)).selectAll("text,line,path").attr("fill",C.textMuted).attr("stroke",C.textMuted);

  const dataG = g.append("g").attr("clip-path",`url(#${clipId})`);

  (ax.datasets||[]).forEach((ds,idx) => {
    const color=parseStyleColor(ds.style)||COLORS[idx%COLORS.length]; const dasharray=parseStyleDash(ds.style); const marker=parseStyleMarker(ds.style); const hasLine=parseStyleHasLine(ds.style); const plotType=ds.type||'line'; const lw=ds.lineWidth||2; const ms=ds.markerSize||3;
    const valid = ds.x.map((x,i)=>ds.y[i]!==null&&(!useLogX||x>0)&&(!useLogY||ds.y[i]>0));

    if(plotType==="line"){
      if(hasLine){dataG.append("path").datum(ds.y).attr("d",d3.line().defined((_,i)=>valid[i]).x((_,i)=>xScale(ds.x[i])).y((_,i)=>yScale(ds.y[i])).curve(d3.curveMonotoneX)).attr("fill","none").attr("stroke",color).attr("stroke-width",lw).attr("stroke-dasharray",dasharray);}
      if(marker){ds.x.forEach((x,i)=>{if(valid[i])drawMarker(dataG,xScale(x),yScale(ds.y[i]),marker,color,ms);});}
    } else if(plotType==="scatter"){
      ds.x.forEach((x,i)=>{if(valid[i])drawMarker(dataG,xScale(x),yScale(ds.y[i]),marker||'o',color,ms>0?ms:4);});
    } else if(plotType==="bar"){
      const bw=Math.max(2,iw/ds.x.length*0.7);
      ds.x.forEach((x,i)=>{if(ds.y[i]!==null){const y0=yScale(useLogY?yScale.domain()[0]:0),y1=yScale(ds.y[i]);dataG.append("rect").attr("x",xScale(x)-bw/2).attr("y",Math.min(y0,y1)).attr("width",bw).attr("height",Math.abs(y1-y0)).attr("fill",color).attr("opacity",0.85).attr("rx",2);}});
    } else if(plotType==="stem"){
      const y0s=yScale(useLogY?yScale.domain()[0]:0);
      ds.x.forEach((x,i)=>{if(valid[i]){dataG.append("line").attr("x1",xScale(x)).attr("y1",y0s).attr("x2",xScale(x)).attr("y2",yScale(ds.y[i])).attr("stroke",color).attr("stroke-width",lw>2?lw*0.75:1.5);drawMarker(dataG,xScale(x),yScale(ds.y[i]),marker||'o',color,ms);}});
    } else if(plotType==="stairs"){
      if(ds.x.length>0&&valid[0]){let p=`M ${xScale(ds.x[0])} ${yScale(ds.y[0])}`;for(let i=1;i<ds.x.length;i++){if(!valid[i]||!valid[i-1])continue;p+=` H ${xScale(ds.x[i])} V ${yScale(ds.y[i])}`;}dataG.append("path").attr("d",p).attr("fill","none").attr("stroke",color).attr("stroke-width",lw).attr("stroke-dasharray",dasharray);if(marker)ds.x.forEach((x,i)=>{if(valid[i])drawMarker(dataG,xScale(x),yScale(ds.y[i]),marker,color,ms);});}
    }
  });

  if(hasTitle) svg.append("text").attr("x",ox+margin.left+iw/2).attr("y",oy+12).attr("text-anchor","middle").attr("fill",C.text).attr("font-size",11).attr("font-weight",600).text(cfg.title);
  if(hasXLabel) svg.append("text").attr("x",ox+margin.left+iw/2).attr("y",oy+availH-(hasLegend?22:2)).attr("text-anchor","middle").attr("fill",C.textMuted).attr("font-size",9).text(cfg.xlabel);
  if(hasYLabel) svg.append("text").attr("transform",`translate(${ox+10},${oy+margin.top+ih/2}) rotate(-90)`).attr("text-anchor","middle").attr("fill",C.textMuted).attr("font-size",9).text(cfg.ylabel);
  if(hasLegend){
    const labels=cfg.legend||(ax.datasets||[]).map(ds=>ds.label).filter(Boolean);
    const lg=svg.append("g").attr("transform",`translate(${ox+margin.left},${oy+availH-14})`);
    let xo=0; labels.forEach((l,i)=>{const c=COLORS[i%COLORS.length];lg.append("rect").attr("x",xo).attr("y",0).attr("width",10).attr("height",3).attr("fill",c).attr("rx",1);lg.append("text").attr("x",xo+14).attr("y",4).attr("fill",C.textDim).attr("font-size",8).attr("alignment-baseline","middle").text(l);xo+=14+l.length*5+10;});
  }
  return availH;
}

function FigurePanel({ figure, onClose }) {
  const svgRef = useRef(null);
  const containerRef = useRef(null);

  useEffect(() => {
    if (!figure || !svgRef.current || !containerRef.current) return;
    const svg = d3.select(svgRef.current);
    svg.selectAll("*").remove();
    const cw = containerRef.current.clientWidth;
    const axesList = getAxesList(figure);
    const grid = figure.subplotGrid; // [rows, cols] or undefined

    if (grid && grid.length >= 2 && axesList.length > 1) {
      // ── Subplot grid layout ──
      const [rows, cols] = grid;
      const cellW = Math.max(120, (cw - 16) / cols);
      const cellH = Math.max(100, 200);
      const totalW = cw;
      const totalH = cellH * rows + 8;
      svg.attr("width", totalW).attr("height", totalH);
      svg.append("defs"); // shared defs

      axesList.forEach(ax => {
        const p = ax.subplotIndex || 1;
        const row = Math.floor((p - 1) / cols);
        const col = (p - 1) % cols;
        const ox = col * cellW + 4;
        const oy = row * cellH + 4;
        renderAxes(svg, ax, ox, oy, cellW - 8, cellH - 8);
      });
    } else {
      // ── Single axes ──
      const ax = axesList[0] || { datasets: [], config: {} };
      const isPolar = ax.config?.polar;
      const h = isPolar ? Math.min(Math.max(200, cw - 16), 400) + (ax.config?.title ? 24 : 0) : 240;
      svg.attr("width", cw).attr("height", h);
      svg.append("defs");
      renderAxes(svg, ax, 0, 0, cw, h);
    }
  }, [figure]);

  if (!figure) return null;
  const grid = figure.subplotGrid;
  return (
    <div ref={containerRef} style={{ background:C.bg2, border:`1px solid ${C.border}`, borderRadius:6, padding:6, position:"relative" }}>
      <div style={{ display:"flex", justifyContent:"space-between", alignItems:"center", marginBottom:4, padding:"0 2px" }}>
        <span style={{ fontSize:9, color:C.textMuted }}>
          Figure {figure.id}
          {grid && ` · subplot(${grid[0]}×${grid[1]})`}
        </span>
        <button onClick={onClose} style={{ background:"none", border:"none", color:C.textMuted, cursor:"pointer", fontSize:13, lineHeight:1 }}>×</button>
      </div>
      <svg ref={svgRef} style={{ display:"block", margin:"0 auto" }} />
    </div>
  );
}

export default function Figures({ figures, onSetFigures, onCloseFigure, onCloseAll, onClose }) {
  const deduped = [];
  const seen = new Set();
  for (let i = figures.length - 1; i >= 0; i--) {
    const id = figures[i].id;
    if (!seen.has(id)) { seen.add(id); deduped.unshift(figures[i]); }
  }
  return (
    <div style={{ display:"flex", flexDirection:"column", height:"100%", overflow:"hidden" }}>
      <div style={{ padding:"7px 10px", borderBottom:`1px solid ${C.border}`, display:"flex", justifyContent:"space-between", alignItems:"center", flexShrink:0 }}>
        <span style={{ fontSize:11, fontWeight:600, color:C.text, fontFamily:FONT_UI }}>📊 Figures</span>
        <div style={{ display:"flex", gap:4, alignItems:"center" }}>
          {deduped.length > 0 && <button onClick={()=>{onSetFigures([]);if(onCloseAll)onCloseAll();}} title="Close all" style={{ background:"none", border:"none", color:C.textMuted, cursor:"pointer", fontSize:10, fontFamily:FONT_UI }}>Close all</button>}
          <button onClick={onClose} style={{ background:"none", border:"none", color:C.textMuted, cursor:"pointer", fontSize:16, lineHeight:1 }}>×</button>
        </div>
      </div>
      <div style={{ flex:1, overflowY:"auto", padding:8, display:"flex", flexDirection:"column", gap:8 }}>
        {deduped.length === 0 ? (
          <div style={{ color:C.textMuted, fontSize:11, padding:16, textAlign:"center", lineHeight:1.6 }}>No figures yet.<br/>Use plot(), bar(), scatter() or hist().</div>
        ) : deduped.map(fig => (
          <FigurePanel key={fig.id} figure={fig} onClose={()=>{onSetFigures(prev=>prev.filter(f=>f.id!==fig.id));if(onCloseFigure)onCloseFigure(fig.id);}} />
        ))}
      </div>
    </div>
  );
}
