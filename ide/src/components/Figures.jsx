import { useEffect, useRef, useState } from "react";
import * as d3 from "d3";
import { useTheme, FONT_UI } from "../theme";

const COLORS = ['#7c6ff0','#60d0f0','#6ee7a0','#f0a060','#e070c0','#e8d060','#f07070','#70b0f0'];

// ════════════════════════════════════════════════════════════════════
// Custom MATLAB-style colormaps
// ════════════════════════════════════════════════════════════════════

function lerpColor(stops, t) {
  t = Math.max(0, Math.min(1, t));
  if (t <= stops[0][0]) return stops[0];
  if (t >= stops[stops.length - 1][0]) return stops[stops.length - 1];
  for (let i = 0; i < stops.length - 1; i++) {
    const [p0, r0, g0, b0] = stops[i];
    const [p1, r1, g1, b1] = stops[i + 1];
    if (t >= p0 && t <= p1) {
      const f = (t - p0) / (p1 - p0);
      return [t, r0 + f * (r1 - r0), g0 + f * (g1 - g0), b0 + f * (b1 - b0)];
    }
  }
  return stops[stops.length - 1];
}

function makeInterpolator(stops) {
  return t => {
    const [, r, g, b] = lerpColor(stops, t);
    return `rgb(${Math.round(r * 255)},${Math.round(g * 255)},${Math.round(b * 255)})`;
  };
}

// Build a 256-entry LUT [r,g,b,r,g,b,...] for a given interpolator (fast path)
function buildLUT(interpolator) {
  const lut = new Uint8Array(256 * 3);
  for (let i = 0; i < 256; i++) {
    const c = interpolator(i / 255);
    // parse "rgb(r,g,b)" or d3 color string
    const m = c.match(/(\d+)/g);
    if (m && m.length >= 3) {
      lut[i * 3]     = parseInt(m[0]);
      lut[i * 3 + 1] = parseInt(m[1]);
      lut[i * 3 + 2] = parseInt(m[2]);
    }
  }
  return lut;
}

// ── parula (MATLAB default, approximated) ──
const interpolateParula = makeInterpolator([
  [0.00, 0.2422, 0.1504, 0.6603],
  [0.10, 0.2810, 0.1856, 0.7468],
  [0.20, 0.2272, 0.3391, 0.8007],
  [0.30, 0.1397, 0.5083, 0.7437],
  [0.40, 0.0200, 0.6400, 0.6500],
  [0.50, 0.1657, 0.7240, 0.5265],
  [0.60, 0.4544, 0.7678, 0.3723],
  [0.70, 0.7372, 0.7636, 0.2227],
  [0.80, 0.9644, 0.7150, 0.0777],
  [0.90, 0.9926, 0.7993, 0.1672],
  [1.00, 0.9769, 0.9839, 0.0805],
]);
const interpolateJet = makeInterpolator([
  [0.000, 0.0, 0.0, 0.5],[0.125, 0.0, 0.0, 1.0],[0.250, 0.0, 0.5, 1.0],
  [0.375, 0.0, 1.0, 1.0],[0.500, 0.5, 1.0, 0.5],[0.625, 1.0, 1.0, 0.0],
  [0.750, 1.0, 0.5, 0.0],[0.875, 1.0, 0.0, 0.0],[1.000, 0.5, 0.0, 0.0],
]);
const interpolateHot = makeInterpolator([
  [0.000, 0.04, 0.0, 0.0],[0.375, 1.0, 0.0, 0.0],
  [0.750, 1.0, 1.0, 0.0],[1.000, 1.0, 1.0, 1.0],
]);
const interpolateCool = makeInterpolator([[0.0,0.0,1.0,1.0],[1.0,1.0,0.0,1.0]]);
const interpolateGray = makeInterpolator([[0.0,0.0,0.0,0.0],[1.0,1.0,1.0,1.0]]);
function interpolateHsv(t) {
  t = Math.max(0, Math.min(1, t));
  const h = t * 360, c = 1, x = c * (1 - Math.abs((h / 60) % 2 - 1)), m = 0;
  let r, g, b;
  if (h < 60)       { r=c;g=x;b=0; } else if (h<120) { r=x;g=c;b=0; }
  else if (h < 180) { r=0;g=c;b=x; } else if (h<240) { r=0;g=x;b=c; }
  else if (h < 300) { r=x;g=0;b=c; } else             { r=c;g=0;b=x; }
  return `rgb(${Math.round((r+m)*255)},${Math.round((g+m)*255)},${Math.round((b+m)*255)})`;
}
const interpolateBone = makeInterpolator([
  [0.000,0.0,0.0,0.0],[0.375,0.3215,0.3215,0.4460],
  [0.750,0.6540,0.7840,0.7840],[1.000,1.0,1.0,1.0],
]);
const interpolateCopper = makeInterpolator([[0.0,0.0,0.0,0.0],[0.8,1.0,0.504,0.320],[1.0,1.0,0.630,0.400]]);
const interpolateSpring = makeInterpolator([[0.0,1.0,0.0,1.0],[1.0,1.0,1.0,0.0]]);
const interpolateSummer = makeInterpolator([[0.0,0.0,0.5,0.4],[1.0,1.0,1.0,0.4]]);
const interpolateAutumn = makeInterpolator([[0.0,1.0,0.0,0.0],[1.0,1.0,1.0,0.0]]);
const interpolateWinter = makeInterpolator([[0.0,0.0,0.0,1.0],[1.0,0.0,1.0,0.5]]);

// ── LUT cache ──
const lutCache = new Map();
function getLUT(interpolator) {
  if (lutCache.has(interpolator)) return lutCache.get(interpolator);
  const lut = buildLUT(interpolator);
  lutCache.set(interpolator, lut);
  return lut;
}

function getColormapInterpolator(name) {
  if (!name) return d3.interpolateViridis;
  switch (name.toLowerCase()) {
    case 'parula':    return interpolateParula;
    case 'jet':       return interpolateJet;
    case 'hot':       return interpolateHot;
    case 'cool':      return interpolateCool;
    case 'gray': case 'grey': return interpolateGray;
    case 'bone':      return interpolateBone;
    case 'copper':    return interpolateCopper;
    case 'spring':    return interpolateSpring;
    case 'summer':    return interpolateSummer;
    case 'autumn':    return interpolateAutumn;
    case 'winter':    return interpolateWinter;
    case 'hsv':       return interpolateHsv;
    case 'viridis':   return d3.interpolateViridis;
    case 'inferno':   return d3.interpolateInferno;
    case 'magma':     return d3.interpolateMagma;
    case 'plasma':    return d3.interpolatePlasma;
    case 'turbo':     return d3.interpolateTurbo;
    case 'cividis':   return d3.interpolateCividis;
    default:          return d3.interpolateViridis;
  }
}

/**
 * Render a 2D matrix to a data-URL via offscreen Canvas + ImageData.
 * Returns a data:image/png;base64,... string.
 * This is O(rows*cols) in pure typed-array ops — no DOM elements.
 */
function renderImagescToDataURL(z, numRows, numCols, cmin, cmax, cmapInterp) {
  const canvas = document.createElement('canvas');
  canvas.width = numCols;
  canvas.height = numRows;
  const ctx = canvas.getContext('2d');
  const imgData = ctx.createImageData(numCols, numRows);
  const pixels = imgData.data; // Uint8ClampedArray [r,g,b,a, r,g,b,a, ...]
  const lut = getLUT(cmapInterp);
  const range = cmax - cmin || 1;
  const inv = 255 / range;

  for (let r = 0; r < numRows; r++) {
    const row = z[r];
    const rowOff = r * numCols * 4;
    for (let c = 0; c < numCols; c++) {
      const v = row[c];
      const off = rowOff + c * 4;
      if (v === null || v === undefined) {
        pixels[off] = 0; pixels[off+1] = 0; pixels[off+2] = 0; pixels[off+3] = 0;
      } else {
        // Map value to LUT index [0..255]
        let idx = ((v - cmin) * inv) | 0;
        if (idx < 0) idx = 0; else if (idx > 255) idx = 255;
        const li = idx * 3;
        pixels[off]     = lut[li];
        pixels[off + 1] = lut[li + 1];
        pixels[off + 2] = lut[li + 2];
        pixels[off + 3] = 255;
      }
    }
  }
  ctx.putImageData(imgData, 0, 0);
  return canvas.toDataURL();
}

// ════════════════════════════════════════════════════════════════════

function parseStyleColor(s){if(!s)return null;const m={r:'#f07070',g:'#6ee7a0',b:'#60d0f0',k:'#d4d4f0',m:'#e070c0',c:'#60d0f0',y:'#e8d060',w:'#ffffff'};for(const c of s){if(m[c])return m[c];}return null;}
function parseStyleDash(s){if(!s)return null;if(s.includes('-.'))return'8,3,2,3';if(s.includes('--'))return'8,4';if(s.includes(':'))return'2,4';return null;}
function parseStyleMarker(s){if(!s)return null;const ms=['o','s','d','^','v','<','>','p','h','*','+','x','.'];for(const c of s){if(ms.includes(c))return c;}return null;}
function parseStyleHasLine(s){if(!s)return true;if(s.includes('-')||s.includes(':'))return true;if(parseStyleMarker(s))return false;return true;}
function drawMarker(g,cx,cy,marker,color,size){const r=size||3;switch(marker){case'.':g.append("circle").attr("cx",cx).attr("cy",cy).attr("r",r*0.5).attr("fill",color);break;case'o':g.append("circle").attr("cx",cx).attr("cy",cy).attr("r",r).attr("fill","none").attr("stroke",color).attr("stroke-width",1.5);break;case'*':g.append("text").attr("x",cx).attr("y",cy).attr("text-anchor","middle").attr("alignment-baseline","central").attr("fill",color).attr("font-size",r*3).text("*");break;case'+':g.append("line").attr("x1",cx-r).attr("y1",cy).attr("x2",cx+r).attr("y2",cy).attr("stroke",color).attr("stroke-width",1.5);g.append("line").attr("x1",cx).attr("y1",cy-r).attr("x2",cx).attr("y2",cy+r).attr("stroke",color).attr("stroke-width",1.5);break;case'x':g.append("line").attr("x1",cx-r).attr("y1",cy-r).attr("x2",cx+r).attr("y2",cy+r).attr("stroke",color).attr("stroke-width",1.5);g.append("line").attr("x1",cx-r).attr("y1",cy+r).attr("x2",cx+r).attr("y2",cy-r).attr("stroke",color).attr("stroke-width",1.5);break;case's':g.append("rect").attr("x",cx-r).attr("y",cy-r).attr("width",r*2).attr("height",r*2).attr("fill","none").attr("stroke",color).attr("stroke-width",1.5);break;case'd':g.append("polygon").attr("points",`${cx},${cy-r} ${cx+r},${cy} ${cx},${cy+r} ${cx-r},${cy}`).attr("fill","none").attr("stroke",color).attr("stroke-width",1.5);break;case'^':g.append("polygon").attr("points",`${cx},${cy-r} ${cx+r},${cy+r} ${cx-r},${cy+r}`).attr("fill","none").attr("stroke",color).attr("stroke-width",1.5);break;case'v':g.append("polygon").attr("points",`${cx},${cy+r} ${cx+r},${cy-r} ${cx-r},${cy-r}`).attr("fill","none").attr("stroke",color).attr("stroke-width",1.5);break;default:g.append("circle").attr("cx",cx).attr("cy",cy).attr("r",r).attr("fill",color);}}
function thetaZeroOffset(loc){switch(loc){case'top':return Math.PI/2;case'left':return Math.PI;case'bottom':return-Math.PI/2;default:return 0;}}
let clipIdCounter=0;
function getAxesList(figure){if(figure.axes&&Array.isArray(figure.axes))return figure.axes;if(figure.datasets)return[{datasets:figure.datasets,config:figure.config||{}}];return[{datasets:[],config:{}}];}

function renderAxes(svg,ax,ox,oy,availW,availH,C){
  const cfg=ax.config||{};const clipId=`clip-${++clipIdCounter}`;
  let defs=svg.select("defs");if(defs.empty())defs=svg.append("defs");
  if(cfg.polar){
    const size=Math.min(availW,availH);const radius=size/2-40;
    const cx=ox+availW/2,cy=oy+size/2+(cfg.title?16:0);
    const g=svg.append("g").attr("transform",`translate(${cx},${cy})`);
    const thetaDirSign=cfg.thetaDir==='clockwise'?-1:1;const zeroOffset=thetaZeroOffset(cfg.thetaZeroLocation);
    let maxRho=0;(ax.datasets||[]).forEach(ds=>{if(ds.y)ds.y.forEach(v=>{if(v!==null&&Math.abs(v)>maxRho)maxRho=Math.abs(v);});});if(maxRho===0)maxRho=1;
    let rMin=0,rMax=maxRho;if(cfg.rlim?.length>=2){rMin=cfg.rlim[0];rMax=cfg.rlim[1];}
    const rScale=d3.scaleLinear().domain([rMin,rMax]).range([0,radius]).nice();const niceMax=rScale.domain()[1];
    defs.append("clipPath").attr("id",clipId).append("circle").attr("cx",0).attr("cy",0).attr("r",rScale(niceMax));
    rScale.ticks(4).filter(t=>t>0).forEach(t=>{g.append("circle").attr("r",rScale(t)).attr("fill","none").attr("stroke",C.border).attr("stroke-dasharray","2,4");g.append("text").attr("x",3).attr("y",-rScale(t)-2).attr("fill",C.textMuted).attr("font-size",8).text(t);});
    g.append("circle").attr("r",rScale(niceMax)).attr("fill","none").attr("stroke",C.border).attr("stroke-width",0.5);
    for(let deg=0;deg<360;deg+=30){const sa=zeroOffset+thetaDirSign*(deg*Math.PI/180);const gx=Math.cos(sa),gy=-Math.sin(sa);g.append("line").attr("x1",0).attr("y1",0).attr("x2",rScale(niceMax)*gx).attr("y2",rScale(niceMax)*gy).attr("stroke",C.border).attr("stroke-dasharray","2,4");g.append("text").attr("x",(rScale(niceMax)+12)*gx).attr("y",(rScale(niceMax)+12)*gy).attr("text-anchor","middle").attr("alignment-baseline","middle").attr("fill",C.textMuted).attr("font-size",8).text(`${deg}°`);}
    const dataG=g.append("g").attr("clip-path",`url(#${clipId})`);
    (ax.datasets||[]).forEach((ds,idx)=>{const color=parseStyleColor(ds.style)||COLORS[idx%COLORS.length];const marker=parseStyleMarker(ds.style);const hasLine=parseStyleHasLine(ds.style);const dasharray=parseStyleDash(ds.style);const lw=ds.lineWidth||2;const ms=ds.markerSize||3;const points=ds.x.map((theta,i)=>{if(ds.y[i]===null)return null;const r=rScale(ds.y[i]);const sa=zeroOffset+thetaDirSign*theta;return[r*Math.cos(sa),-r*Math.sin(sa)];}).filter(Boolean);if(hasLine&&points.length>1){const tr=ds.x.length>1?Math.abs(ds.x[ds.x.length-1]-ds.x[0]):0;dataG.append("path").datum(points).attr("d",d3.line().x(d=>d[0]).y(d=>d[1]).curve(tr>=Math.PI*1.9?d3.curveLinearClosed:d3.curveLinear)).attr("fill","none").attr("stroke",color).attr("stroke-width",lw).attr("stroke-dasharray",dasharray);}if(marker)points.forEach(p=>drawMarker(dataG,p[0],p[1],marker,color,ms));});
    if(cfg.title)svg.append("text").attr("x",cx).attr("y",oy+12).attr("text-anchor","middle").attr("fill",C.text).attr("font-size",11).attr("font-weight",600).text(cfg.title);
    return size+(cfg.title?20:0);
  }
  // ── Cartesian ──
  const hasTitle=!!cfg.title,hasXLabel=!!cfg.xlabel,hasYLabel=!!cfg.ylabel,hasLegend=cfg.legend?.length>0||(ax.datasets||[]).some(ds=>ds.label);
  const hasImagesc=(ax.datasets||[]).some(ds=>ds.type==='imagesc');
  const colorbarWidth=hasImagesc?50:0;
  const margin={top:hasTitle?28:12,right:12+colorbarWidth,bottom:hasXLabel?36:24,left:hasYLabel?46:36};
  const iw=availW-margin.left-margin.right,ih=availH-margin.top-margin.bottom-(hasLegend?20:0);
  if(iw<20||ih<20)return availH;
  const g=svg.append("g").attr("transform",`translate(${ox+margin.left},${oy+margin.top})`);
  const allX=(ax.datasets||[]).flatMap(ds=>ds.x||[]).filter(v=>v!==null),allY=(ax.datasets||[]).flatMap(ds=>ds.y||[]).filter(v=>v!==null);
  if(!allX.length||!allY.length)return availH;
  let xMin=Math.min(...allX),xMax=Math.max(...allX),yMin=Math.min(...allY),yMax=Math.max(...allY);
  if(hasImagesc){const imgDs=(ax.datasets||[]).find(ds=>ds.type==='imagesc');if(imgDs&&imgDs.z){const nR=imgDs.z.length,nC=imgDs.z[0]?.length||0;const xr=imgDs.x,yr=imgDs.y;const x0=xr[0],x1=xr[xr.length-1],y0=yr[0],y1=yr[yr.length-1];const cW=nC>1?(x1-x0)/(nC-1):1,cH=nR>1?(y1-y0)/(nR-1):1;xMin=x0-cW/2;xMax=x1+cW/2;yMin=y0-cH/2;yMax=y1+cH/2;}}
  if(cfg.xlim?.length>=2){xMin=cfg.xlim[0];xMax=cfg.xlim[1];}
  if(cfg.ylim?.length>=2){yMin=cfg.ylim[0];yMax=cfg.ylim[1];}else if(!hasImagesc){const p=(yMax-yMin)*0.05||1;yMin-=p;yMax+=p;}
  const axisMode=cfg.axisMode;
  if(axisMode==='equal'){const xR=xMax-xMin||1,yR=yMax-yMin||1,dA=xR/yR,vA=iw/ih;if(dA>vA){const ny=xR*ih/iw,ym=(yMin+yMax)/2;yMin=ym-ny/2;yMax=ym+ny/2;}else{const nx=yR*iw/ih,xm=(xMin+xMax)/2;xMin=xm-nx/2;xMax=xm+nx/2;}}
  else if(axisMode==='tight'){if(!cfg.ylim){yMin=Math.min(...allY);yMax=Math.max(...allY);}}
  const useLogX=cfg.xscale==='log',useLogY=cfg.yscale==='log',flipY=axisMode==='ij';
  let xScale,yScale;
  if(useLogX){if(xMin<=0)xMin=Math.min(...allX.filter(v=>v>0))||0.001;xScale=d3.scaleLog().domain([xMin,xMax]).range([0,iw]).nice();}else xScale=d3.scaleLinear().domain([xMin,xMax]).range([0,iw]);
  if(useLogY){if(yMin<=0)yMin=Math.min(...allY.filter(v=>v>0))||0.001;yScale=d3.scaleLog().domain([yMin,yMax]).range(flipY?[0,ih]:[ih,0]).nice();}else yScale=d3.scaleLinear().domain([yMin,yMax]).range(flipY?[0,ih]:[ih,0]);
  if(!hasImagesc){if(!useLogX)xScale=xScale.nice();if(!useLogY)yScale=yScale.nice();}
  defs.append("clipPath").attr("id",clipId).append("rect").attr("x",0).attr("y",0).attr("width",iw).attr("height",ih);
  const xTC=Math.max(2,Math.floor(iw/60)),yTC=Math.max(2,Math.floor(ih/40)),xTicks=xScale.ticks(xTC),yTicks=yScale.ticks(yTC);
  const gridMode=cfg.grid===true?'on':(cfg.grid||'');
  if(gridMode==='on'||gridMode==='minor'){
    g.append("g").selectAll("line").data(yTicks).enter().append("line").attr("x1",0).attr("x2",iw).attr("y1",d=>yScale(d)).attr("y2",d=>yScale(d)).attr("stroke",C.textMuted).attr("stroke-dasharray","3,3").attr("opacity",0.3);
    g.append("g").selectAll("line").data(xTicks).enter().append("line").attr("x1",d=>xScale(d)).attr("x2",d=>xScale(d)).attr("y1",0).attr("y2",ih).attr("stroke",C.textMuted).attr("stroke-dasharray","3,3").attr("opacity",0.3);
    if(gridMode==='minor'){const xM=[],yM=[];for(let i=0;i<xTicks.length-1;i++){const a=xTicks[i],b=xTicks[i+1],s=(b-a)/5;for(let j=1;j<5;j++)xM.push(a+s*j);}for(let i=0;i<yTicks.length-1;i++){const a=yTicks[i],b=yTicks[i+1],s=(b-a)/5;for(let j=1;j<5;j++)yM.push(a+s*j);}
    g.append("g").selectAll("line").data(yM).enter().append("line").attr("x1",0).attr("x2",iw).attr("y1",d=>yScale(d)).attr("y2",d=>yScale(d)).attr("stroke",C.textMuted).attr("stroke-dasharray","1,3").attr("opacity",0.15);
    g.append("g").selectAll("line").data(xM).enter().append("line").attr("x1",d=>xScale(d)).attr("x2",d=>xScale(d)).attr("y1",0).attr("y2",ih).attr("stroke",C.textMuted).attr("stroke-dasharray","1,3").attr("opacity",0.15);}
  }
  g.append("g").attr("transform",`translate(0,${ih})`).call(d3.axisBottom(xScale).ticks(xTC)).selectAll("text,line,path").attr("fill",C.textMuted).attr("stroke",C.textMuted);
  g.append("g").call(d3.axisLeft(yScale).ticks(yTC)).selectAll("text,line,path").attr("fill",C.textMuted).attr("stroke",C.textMuted);
  const dataG=g.append("g").attr("clip-path",`url(#${clipId})`);

  const cmapInterp = getColormapInterpolator(cfg.colormap);

  (ax.datasets||[]).forEach((ds,idx)=>{const color=parseStyleColor(ds.style)||COLORS[idx%COLORS.length];const dasharray=parseStyleDash(ds.style);const marker=parseStyleMarker(ds.style);const hasLine=parseStyleHasLine(ds.style);const plotType=ds.type||'line';const lw=ds.lineWidth||2;const ms=ds.markerSize||3;
    const valid=ds.x?ds.x.map((x,i)=>ds.y?ds.y[i]!==null&&(!useLogX||x>0)&&(!useLogY||ds.y[i]>0):false):[];

    if(plotType==="imagesc"){
      const z=ds.z;if(!z||!z.length)return;
      const numRows=z.length,numCols=z[0]?.length||0;if(numCols===0)return;
      const xr=ds.x||[1,numCols],yr=ds.y||[1,numRows];
      const x0=xr[0],x1=xr[xr.length-1],y0=yr[0],y1=yr[yr.length-1];
      let cmin=Infinity,cmax=-Infinity;
      if(cfg.clim&&cfg.clim.length>=2){cmin=cfg.clim[0];cmax=cfg.clim[1];}
      else{for(let r=0;r<numRows;r++)for(let c=0;c<numCols;c++){const v=z[r][c];if(v!==null&&v!==undefined){if(v<cmin)cmin=v;if(v>cmax)cmax=v;}}}
      if(cmin===cmax){cmin-=0.5;cmax+=0.5;}

      // ── Canvas fast path: render matrix → PNG data URL → single <image> ──
      const dataURL = renderImagescToDataURL(z, numRows, numCols, cmin, cmax, cmapInterp);
      const cellW=numCols>1?(x1-x0)/(numCols-1):1,cellH=numRows>1?(y1-y0)/(numRows-1):1;
      const imgX = xScale(x0 - cellW/2);
      const imgY = yScale(flipY ? (y0 - cellH/2) : (y1 + cellH/2));
      const imgW = Math.abs(xScale(x1 + cellW/2) - xScale(x0 - cellW/2));
      const imgH = Math.abs(yScale(y1 + cellH/2) - yScale(y0 - cellH/2));

      dataG.append("image")
        .attr("href", dataURL)
        .attr("x", imgX)
        .attr("y", Math.min(imgY, yScale(flipY ? (y1 + cellH/2) : (y0 - cellH/2))))
        .attr("width", imgW)
        .attr("height", imgH)
        .attr("preserveAspectRatio", "none")
        .style("image-rendering", "pixelated");

      // ── Colorbar (SVG — only ~64 rects, fast) ──
      if(colorbarWidth>0){
        const cbW=12,cbH=ih,cbX=iw+10,cbY=0,cbSteps=64,cbStepH=cbH/cbSteps;
        const colorScale=d3.scaleSequential(cmapInterp).domain([cmin,cmax]);
        for(let i=0;i<cbSteps;i++){const t=1-i/cbSteps;dataG.append("rect").attr("x",cbX).attr("y",cbY+i*cbStepH).attr("width",cbW).attr("height",cbStepH+0.5).attr("fill",colorScale(cmin+t*(cmax-cmin))).attr("stroke","none").attr("shape-rendering","crispEdges");}
        dataG.append("rect").attr("x",cbX).attr("y",cbY).attr("width",cbW).attr("height",cbH).attr("fill","none").attr("stroke",C.textMuted).attr("stroke-width",0.5);
        const cbScale=d3.scaleLinear().domain([cmin,cmax]).range([cbH,0]);cbScale.ticks(5).forEach(t=>{const ty=cbScale(t);dataG.append("line").attr("x1",cbX+cbW).attr("y1",ty).attr("x2",cbX+cbW+3).attr("y2",ty).attr("stroke",C.textMuted).attr("stroke-width",0.5);dataG.append("text").attr("x",cbX+cbW+5).attr("y",ty).attr("fill",C.textMuted).attr("font-size",7).attr("alignment-baseline","middle").text(t%1===0?t:t.toFixed(2));});
      }
    }
    else if(plotType==="line"){if(hasLine){dataG.append("path").datum(ds.y).attr("d",d3.line().defined((_,i)=>valid[i]).x((_,i)=>xScale(ds.x[i])).y((_,i)=>yScale(ds.y[i])).curve(d3.curveMonotoneX)).attr("fill","none").attr("stroke",color).attr("stroke-width",lw).attr("stroke-dasharray",dasharray);}if(marker)ds.x.forEach((x,i)=>{if(valid[i])drawMarker(dataG,xScale(x),yScale(ds.y[i]),marker,color,ms);});}
    else if(plotType==="scatter"){ds.x.forEach((x,i)=>{if(valid[i])drawMarker(dataG,xScale(x),yScale(ds.y[i]),marker||'o',color,ms>0?ms:4);});}
    else if(plotType==="bar"){const bw=Math.max(2,iw/ds.x.length*0.7);ds.x.forEach((x,i)=>{if(ds.y[i]!==null){const y0=yScale(useLogY?yScale.domain()[0]:0),y1=yScale(ds.y[i]);dataG.append("rect").attr("x",xScale(x)-bw/2).attr("y",Math.min(y0,y1)).attr("width",bw).attr("height",Math.abs(y1-y0)).attr("fill",color).attr("opacity",0.85).attr("rx",2);}});}
    else if(plotType==="stem"){const y0s=yScale(useLogY?yScale.domain()[0]:0);ds.x.forEach((x,i)=>{if(valid[i]){dataG.append("line").attr("x1",xScale(x)).attr("y1",y0s).attr("x2",xScale(x)).attr("y2",yScale(ds.y[i])).attr("stroke",color).attr("stroke-width",lw>2?lw*0.75:1.5);drawMarker(dataG,xScale(x),yScale(ds.y[i]),marker||'o',color,ms);}});}
    else if(plotType==="stairs"){if(ds.x.length>0&&valid[0]){let p=`M ${xScale(ds.x[0])} ${yScale(ds.y[0])}`;for(let i=1;i<ds.x.length;i++){if(!valid[i]||!valid[i-1])continue;p+=` H ${xScale(ds.x[i])} V ${yScale(ds.y[i])}`;}dataG.append("path").attr("d",p).attr("fill","none").attr("stroke",color).attr("stroke-width",lw).attr("stroke-dasharray",dasharray);if(marker)ds.x.forEach((x,i)=>{if(valid[i])drawMarker(dataG,xScale(x),yScale(ds.y[i]),marker,color,ms);});}}
  });
  if(hasTitle)svg.append("text").attr("x",ox+margin.left+iw/2).attr("y",oy+12).attr("text-anchor","middle").attr("fill",C.text).attr("font-size",11).attr("font-weight",600).text(cfg.title);
  if(hasXLabel)svg.append("text").attr("x",ox+margin.left+iw/2).attr("y",oy+availH-(hasLegend?22:2)).attr("text-anchor","middle").attr("fill",C.textMuted).attr("font-size",9).text(cfg.xlabel);
  if(hasYLabel)svg.append("text").attr("transform",`translate(${ox+10},${oy+margin.top+ih/2}) rotate(-90)`).attr("text-anchor","middle").attr("fill",C.textMuted).attr("font-size",9).text(cfg.ylabel);
  if(hasLegend){const labels=cfg.legend||(ax.datasets||[]).map(ds=>ds.label).filter(Boolean);const lg=svg.append("g").attr("transform",`translate(${ox+margin.left},${oy+availH-14})`);let xo=0;labels.forEach((l,i)=>{const c=COLORS[i%COLORS.length];lg.append("rect").attr("x",xo).attr("y",0).attr("width",10).attr("height",3).attr("fill",c).attr("rx",1);lg.append("text").attr("x",xo+14).attr("y",4).attr("fill",C.textDim).attr("font-size",8).attr("alignment-baseline","middle").text(l);xo+=14+l.length*5+10;});}
  return availH;
}

function FigurePanel({figure,onClose}){
  const C = useTheme();
  const svgRef=useRef(null);const containerRef=useRef(null);const[containerWidth,setContainerWidth]=useState(0);
  useEffect(()=>{const el=containerRef.current;if(!el)return;const ro=new ResizeObserver(entries=>{for(const e of entries){const w=e.contentRect.width;if(w>0)setContainerWidth(w);}});ro.observe(el);setContainerWidth(el.clientWidth);return()=>ro.disconnect();},[]);
  useEffect(()=>{
    if(!figure||!svgRef.current||containerWidth<10)return;
    const svg=d3.select(svgRef.current);svg.selectAll("*").remove();
    const cw=containerWidth-12;const axesList=getAxesList(figure);const grid=figure.subplotGrid;
    if(grid&&grid.length>=2&&axesList.length>1){
      const[rows,cols]=grid;const cellW=Math.max(120,cw/cols);const cellH=Math.max(100,Math.min(220,cellW*0.65));
      svg.attr("width",cw).attr("height",cellH*rows+8);svg.append("defs");
      axesList.forEach(ax=>{const p=ax.subplotIndex||1;const row=Math.floor((p-1)/cols);const col=(p-1)%cols;renderAxes(svg,ax,col*cellW+4,row*cellH+4,cellW-8,cellH-8,C);});
    }else{
      const ax=axesList[0]||{datasets:[],config:{}};const isPolar=ax.config?.polar;
      const h=isPolar?Math.min(Math.max(200,cw),400)+(ax.config?.title?24:0):Math.max(180,Math.min(300,cw*0.55));
      svg.attr("width",cw).attr("height",h);svg.append("defs");renderAxes(svg,ax,0,0,cw,h,C);
    }
  },[figure,containerWidth,C]);
  if(!figure)return null;const grid=figure.subplotGrid;
  return(<div ref={containerRef} style={{background:C.bg2,border:`1px solid ${C.border}`,borderRadius:6,padding:6,position:"relative"}}>
    <div style={{display:"flex",justifyContent:"space-between",alignItems:"center",marginBottom:4,padding:"0 2px"}}>
      <span style={{fontSize:9,color:C.textMuted}}>Figure {figure.id}{grid&&` · subplot(${grid[0]}×${grid[1]})`}</span>
      <button onClick={onClose} style={{background:"none",border:"none",color:C.textMuted,cursor:"pointer",fontSize:13,lineHeight:1}}>×</button>
    </div><svg ref={svgRef} style={{display:"block",margin:"0 auto"}}/></div>);
}

export default function Figures({figures,onSetFigures,onCloseFigure,onCloseAll,onClose}){
  const C = useTheme();
  const deduped=[];const seen=new Set();for(let i=figures.length-1;i>=0;i--){const id=figures[i].id;if(!seen.has(id)){seen.add(id);deduped.unshift(figures[i]);}}
  return(<div style={{display:"flex",flexDirection:"column",height:"100%",overflow:"hidden"}}>
    <div style={{padding:"7px 10px",borderBottom:`1px solid ${C.border}`,display:"flex",justifyContent:"space-between",alignItems:"center",flexShrink:0}}>
      <span style={{fontSize:11,fontWeight:600,color:C.text,fontFamily:FONT_UI}}>📊 Figures</span>
      <div style={{display:"flex",gap:4,alignItems:"center"}}>{deduped.length>0&&<button onClick={()=>{onSetFigures([]);if(onCloseAll)onCloseAll();}} title="Close all" style={{background:"none",border:"none",color:C.textMuted,cursor:"pointer",fontSize:10,fontFamily:FONT_UI}}>Close all</button>}<button onClick={onClose} style={{background:"none",border:"none",color:C.textMuted,cursor:"pointer",fontSize:16,lineHeight:1}}>×</button></div>
    </div>
    <div style={{flex:1,overflowY:"auto",padding:8,display:"flex",flexDirection:"column",gap:8}}>
      {deduped.length===0?<div style={{color:C.textMuted,fontSize:11,padding:16,textAlign:"center",lineHeight:1.6}}>No figures yet.<br/>Use plot(), bar(), scatter() or hist().</div>
      :deduped.map(fig=><FigurePanel key={fig.id} figure={fig} onClose={()=>{onSetFigures(prev=>prev.filter(f=>f.id!==fig.id));if(onCloseFigure)onCloseFigure(fig.id);}}/>)}
    </div></div>);
}