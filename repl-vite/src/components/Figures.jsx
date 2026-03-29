import { useEffect, useRef } from "react";
import * as d3 from "d3";
import C, { FONT_UI } from "../theme";

const COLORS = ['#7c6ff0','#60d0f0','#6ee7a0','#f0a060','#e070c0','#e8d060','#f07070','#70b0f0'];

function parseStyleColor(style) {
  if (!style) return null;
  const colorMap = { r: '#f07070', g: '#6ee7a0', b: '#60d0f0', k: '#d4d4f0', m: '#e070c0', c: '#60d0f0', y: '#e8d060', w: '#ffffff' };
  for (const ch of style) { if (colorMap[ch]) return colorMap[ch]; }
  return null;
}

function parseStyleDash(style) {
  if (!style) return null;
  if (style.includes('-.')) return '8,3,2,3';
  if (style.includes('--')) return '8,4';
  if (style.includes(':')) return '2,4';
  return null;
}

function parseStyleMarker(style) {
  if (!style) return null;
  const markers = ['o','s','d','^','v','<','>','p','h','*','+','x','.'];
  for (const ch of style) { if (markers.includes(ch)) return ch; }
  return null;
}

function parseStyleHasLine(style) {
  if (!style) return true;
  if (style.includes('-') || style.includes(':')) return true;
  if (parseStyleMarker(style)) return false;
  return true;
}

function drawMarker(g, cx, cy, marker, color, size) {
  const r = size || 3;
  switch (marker) {
    case '.': g.append("circle").attr("cx",cx).attr("cy",cy).attr("r",r*0.5).attr("fill",color); break;
    case 'o': g.append("circle").attr("cx",cx).attr("cy",cy).attr("r",r).attr("fill","none").attr("stroke",color).attr("stroke-width",1.5); break;
    case '*': g.append("text").attr("x",cx).attr("y",cy).attr("text-anchor","middle").attr("alignment-baseline","central").attr("fill",color).attr("font-size",r*3).text("*"); break;
    case '+':
      g.append("line").attr("x1",cx-r).attr("y1",cy).attr("x2",cx+r).attr("y2",cy).attr("stroke",color).attr("stroke-width",1.5);
      g.append("line").attr("x1",cx).attr("y1",cy-r).attr("x2",cx).attr("y2",cy+r).attr("stroke",color).attr("stroke-width",1.5);
      break;
    case 'x':
      g.append("line").attr("x1",cx-r).attr("y1",cy-r).attr("x2",cx+r).attr("y2",cy+r).attr("stroke",color).attr("stroke-width",1.5);
      g.append("line").attr("x1",cx-r).attr("y1",cy+r).attr("x2",cx+r).attr("y2",cy-r).attr("stroke",color).attr("stroke-width",1.5);
      break;
    case 's': g.append("rect").attr("x",cx-r).attr("y",cy-r).attr("width",r*2).attr("height",r*2).attr("fill","none").attr("stroke",color).attr("stroke-width",1.5); break;
    case 'd': g.append("polygon").attr("points",`${cx},${cy-r} ${cx+r},${cy} ${cx},${cy+r} ${cx-r},${cy}`).attr("fill","none").attr("stroke",color).attr("stroke-width",1.5); break;
    case '^': g.append("polygon").attr("points",`${cx},${cy-r} ${cx+r},${cy+r} ${cx-r},${cy+r}`).attr("fill","none").attr("stroke",color).attr("stroke-width",1.5); break;
    case 'v': g.append("polygon").attr("points",`${cx},${cy+r} ${cx+r},${cy-r} ${cx-r},${cy-r}`).attr("fill","none").attr("stroke",color).attr("stroke-width",1.5); break;
    default: g.append("circle").attr("cx",cx).attr("cy",cy).attr("r",r).attr("fill",color);
  }
}

/** Convert thetaZeroLocation to rotation offset in radians */
function thetaZeroOffset(loc) {
  switch (loc) {
    case 'top': return Math.PI / 2;
    case 'left': return Math.PI;
    case 'bottom': return -Math.PI / 2;
    default: return 0; // 'right'
  }
}

function FigurePanel({ figure, onClose }) {
  const svgRef = useRef(null);
  const containerRef = useRef(null);

  useEffect(() => {
    if (!figure || !svgRef.current || !containerRef.current) return;
    const svg = d3.select(svgRef.current);
    svg.selectAll("*").remove();
    const cw = containerRef.current.clientWidth;

    // ── Polar plot ──
    if (figure.config?.polar) {
      const size = Math.min(Math.max(200, cw - 16), 400);
      const radius = size / 2 - 40;
      svg.attr("width", size).attr("height", size + (figure.config?.title ? 24 : 0));
      const cy = size / 2 + (figure.config?.title ? 24 : 0);
      const g = svg.append("g").attr("transform", `translate(${size/2},${cy})`);

      const thetaDirSign = figure.config?.thetaDir === 'clockwise' ? -1 : 1;
      const zeroOffset = thetaZeroOffset(figure.config?.thetaZeroLocation);

      let maxRho = 0;
      figure.datasets.forEach(ds => {
        ds.y.forEach(v => { if (v !== null && Math.abs(v) > maxRho) maxRho = Math.abs(v); });
      });
      if (maxRho === 0) maxRho = 1;

      // Apply rlim if set
      let rMin = 0, rMax = maxRho;
      if (figure.config?.rlim?.length >= 2) {
        rMin = figure.config.rlim[0];
        rMax = figure.config.rlim[1];
      }
      const rScale = d3.scaleLinear().domain([rMin, rMax]).range([0, radius]).nice();
      const niceMax = rScale.domain()[1];

      // Radial grid
      const ticks = rScale.ticks(4).filter(t => t > 0);
      ticks.forEach(t => {
        g.append("circle").attr("r", rScale(t)).attr("fill","none").attr("stroke",C.border).attr("stroke-dasharray","2,4");
        g.append("text").attr("x",3).attr("y",-rScale(t)-2).attr("fill",C.textMuted).attr("font-size",8).text(t);
      });

      // Angular grid — respects thetaDir and thetaZeroLocation
      for (let deg = 0; deg < 360; deg += 30) {
        const displayRad = deg * Math.PI / 180;
        // Convert display angle to screen coords
        const screenAngle = zeroOffset + thetaDirSign * displayRad;
        const gx = Math.cos(screenAngle);
        const gy = -Math.sin(screenAngle);
        g.append("line").attr("x1",0).attr("y1",0)
          .attr("x2",rScale(niceMax)*gx).attr("y2",rScale(niceMax)*gy)
          .attr("stroke",C.border).attr("stroke-dasharray","2,4");
        g.append("text")
          .attr("x",(rScale(niceMax)+12)*gx).attr("y",(rScale(niceMax)+12)*gy)
          .attr("text-anchor","middle").attr("alignment-baseline","middle")
          .attr("fill",C.textMuted).attr("font-size",8).text(`${deg}°`);
      }

      // Plot datasets
      figure.datasets.forEach((ds, idx) => {
        const color = parseStyleColor(ds.style) || COLORS[idx % COLORS.length];
        const marker = parseStyleMarker(ds.style);
        const hasLine = parseStyleHasLine(ds.style);
        const dasharray = parseStyleDash(ds.style);
        const lw = ds.lineWidth || 2;
        const ms = ds.markerSize || 3;

        // Polar to screen: apply thetaDir and thetaZeroLocation
        // Data theta is in standard math radians (0=right, CCW)
        // Screen: rotate by zeroOffset, apply direction sign
        const points = ds.x.map((theta, i) => {
          if (ds.y[i] === null) return null;
          const r = rScale(ds.y[i]);
          const screenAngle = zeroOffset + thetaDirSign * theta;
          return [r * Math.cos(screenAngle), -r * Math.sin(screenAngle)];
        }).filter(Boolean);

        if (hasLine && points.length > 1) {
          const lineGen = d3.line().x(d => d[0]).y(d => d[1]).curve(d3.curveLinearClosed);
          g.append("path").datum(points).attr("d", lineGen)
            .attr("fill","none").attr("stroke",color).attr("stroke-width",lw)
            .attr("stroke-dasharray", dasharray);
        }
        if (marker) points.forEach(p => drawMarker(g, p[0], p[1], marker, color, ms));
      });

      if (figure.config?.title) {
        svg.append("text").attr("x",size/2).attr("y",16)
          .attr("text-anchor","middle").attr("fill",C.text)
          .attr("font-size",12).attr("font-weight",600).text(figure.config.title);
      }
      return;
    }

    // ── Cartesian plot ──
    const width = Math.max(200, cw - 16);
    const hasTitle = !!figure.config?.title;
    const hasXLabel = !!figure.config?.xlabel;
    const hasYLabel = !!figure.config?.ylabel;
    const hasLegend = figure.config?.legend?.length > 0 || figure.datasets.some(ds => ds.label);
    const height = 220 + (hasTitle ? 10 : 0) + (hasLegend ? 24 : 0);
    const margin = { top: hasTitle ? 32 : 16, right: 16, bottom: hasXLabel ? 42 : 30, left: hasYLabel ? 52 : 42 };
    const iw = width - margin.left - margin.right;
    const ih = height - margin.top - margin.bottom - (hasLegend ? 24 : 0);

    svg.attr("width", width).attr("height", height);
    const g = svg.append("g").attr("transform", `translate(${margin.left},${margin.top})`);

    const allX = figure.datasets.flatMap(ds => ds.x).filter(v => v !== null);
    const allY = figure.datasets.flatMap(ds => ds.y).filter(v => v !== null);
    let xMin = Math.min(...allX), xMax = Math.max(...allX);
    let yMin = Math.min(...allY), yMax = Math.max(...allY);

    if (figure.config?.xlim?.length >= 2) { xMin = figure.config.xlim[0]; xMax = figure.config.xlim[1]; }
    if (figure.config?.ylim?.length >= 2) { yMin = figure.config.ylim[0]; yMax = figure.config.ylim[1]; }
    else { const yPad = (yMax - yMin) * 0.05 || 1; yMin -= yPad; yMax += yPad; }

    // Axis mode adjustments
    const axisMode = figure.config?.axisMode;
    if (axisMode === 'equal') {
      const range = Math.max(xMax - xMin, yMax - yMin);
      const xMid = (xMin + xMax) / 2, yMid = (yMin + yMax) / 2;
      xMin = xMid - range / 2; xMax = xMid + range / 2;
      yMin = yMid - range / 2; yMax = yMid + range / 2;
    } else if (axisMode === 'tight') {
      // No padding — use raw data extents
      const rawYMin = Math.min(...allY), rawYMax = Math.max(...allY);
      if (!figure.config?.ylim) { yMin = rawYMin; yMax = rawYMax; }
    }

    // Create scales based on xscale/yscale
    const useLogX = figure.config?.xscale === 'log';
    const useLogY = figure.config?.yscale === 'log';
    const flipY = axisMode === 'ij';

    let xScale, yScale;
    if (useLogX) {
      if (xMin <= 0) xMin = Math.min(...allX.filter(v => v > 0)) || 0.001;
      xScale = d3.scaleLog().domain([xMin, xMax]).range([0, iw]).nice();
    } else {
      xScale = d3.scaleLinear().domain([xMin, xMax]).range([0, iw]).nice();
    }
    if (useLogY) {
      if (yMin <= 0) yMin = Math.min(...allY.filter(v => v > 0)) || 0.001;
      yScale = d3.scaleLog().domain([yMin, yMax]).range(flipY ? [0, ih] : [ih, 0]).nice();
    } else {
      yScale = d3.scaleLinear().domain([yMin, yMax]).range(flipY ? [0, ih] : [ih, 0]).nice();
    }

    // Grid
    if (figure.config?.grid) {
      g.append("g").selectAll("line").data(yScale.ticks(5)).enter().append("line")
        .attr("x1",0).attr("x2",iw).attr("y1",d=>yScale(d)).attr("y2",d=>yScale(d))
        .attr("stroke",C.border).attr("stroke-dasharray","3,3").attr("opacity",0.6);
      g.append("g").selectAll("line").data(xScale.ticks(5)).enter().append("line")
        .attr("x1",d=>xScale(d)).attr("x2",d=>xScale(d)).attr("y1",0).attr("y2",ih)
        .attr("stroke",C.border).attr("stroke-dasharray","3,3").attr("opacity",0.6);
    } else {
      g.append("g").selectAll("line").data(yScale.ticks(4)).enter().append("line")
        .attr("x1",0).attr("x2",iw).attr("y1",d=>yScale(d)).attr("y2",d=>yScale(d))
        .attr("stroke",C.border).attr("stroke-dasharray","2,4");
    }

    // Axes
    g.append("g").attr("transform",`translate(0,${ih})`).call(d3.axisBottom(xScale).ticks(5))
      .selectAll("text,line,path").attr("fill",C.textMuted).attr("stroke",C.textMuted);
    g.append("g").call(d3.axisLeft(yScale).ticks(4))
      .selectAll("text,line,path").attr("fill",C.textMuted).attr("stroke",C.textMuted);

    // Datasets
    figure.datasets.forEach((ds, idx) => {
      const color = parseStyleColor(ds.style) || COLORS[idx % COLORS.length];
      const dasharray = parseStyleDash(ds.style);
      const marker = parseStyleMarker(ds.style);
      const hasLine = parseStyleHasLine(ds.style);
      const plotType = ds.type || 'line';
      const lw = ds.lineWidth || 2;
      const ms = ds.markerSize || 3;

      // Filter out non-positive values for log scales
      const validIdx = ds.x.map((x, i) => {
        if (ds.y[i] === null) return false;
        if (useLogX && x <= 0) return false;
        if (useLogY && ds.y[i] <= 0) return false;
        return true;
      });

      if (plotType === "line") {
        if (hasLine) {
          const lineGen = d3.line()
            .defined((_, i) => validIdx[i])
            .x((_, i) => xScale(ds.x[i]))
            .y((_, i) => yScale(ds.y[i]))
            .curve(d3.curveMonotoneX);
          g.append("path").datum(ds.y).attr("d", lineGen)
            .attr("fill","none").attr("stroke",color).attr("stroke-width",lw)
            .attr("stroke-dasharray", dasharray);
        }
        if (marker) {
          ds.x.forEach((x, i) => {
            if (validIdx[i]) drawMarker(g, xScale(x), yScale(ds.y[i]), marker, color, ms);
          });
        }
      } else if (plotType === "scatter") {
        ds.x.forEach((x, i) => {
          if (validIdx[i]) drawMarker(g, xScale(x), yScale(ds.y[i]), marker || 'o', color, ms > 0 ? ms : 4);
        });
      } else if (plotType === "bar") {
        const bw = Math.max(2, iw / ds.x.length * 0.7);
        ds.x.forEach((x, i) => {
          if (ds.y[i] !== null) {
            const y0 = yScale(0), y1 = yScale(ds.y[i]);
            g.append("rect")
              .attr("x", xScale(x) - bw/2).attr("y", Math.min(y0, y1))
              .attr("width", bw).attr("height", Math.abs(y1 - y0))
              .attr("fill", color).attr("opacity", 0.85).attr("rx", 2);
          }
        });
      } else if (plotType === "stem") {
        const y0screen = yScale(useLogY ? yScale.domain()[0] : 0);
        ds.x.forEach((x, i) => {
          if (validIdx[i]) {
            g.append("line")
              .attr("x1", xScale(x)).attr("y1", y0screen)
              .attr("x2", xScale(x)).attr("y2", yScale(ds.y[i]))
              .attr("stroke", color).attr("stroke-width", lw > 2 ? lw * 0.75 : 1.5);
            drawMarker(g, xScale(x), yScale(ds.y[i]), marker || 'o', color, ms);
          }
        });
      } else if (plotType === "stairs") {
        if (ds.x.length > 0 && validIdx[0]) {
          let pathD = `M ${xScale(ds.x[0])} ${yScale(ds.y[0])}`;
          for (let i = 1; i < ds.x.length; i++) {
            if (!validIdx[i] || !validIdx[i-1]) continue;
            pathD += ` H ${xScale(ds.x[i])} V ${yScale(ds.y[i])}`;
          }
          g.append("path").attr("d", pathD)
            .attr("fill","none").attr("stroke",color).attr("stroke-width",lw)
            .attr("stroke-dasharray", dasharray);
          if (marker) {
            ds.x.forEach((x, i) => {
              if (validIdx[i]) drawMarker(g, xScale(x), yScale(ds.y[i]), marker, color, ms);
            });
          }
        }
      }
    });

    // Title, labels, legend
    if (hasTitle) svg.append("text").attr("x",width/2).attr("y",16).attr("text-anchor","middle").attr("fill",C.text).attr("font-size",12).attr("font-weight",600).text(figure.config.title);
    if (hasXLabel) svg.append("text").attr("x",width/2).attr("y",height-(hasLegend?28:4)).attr("text-anchor","middle").attr("fill",C.textMuted).attr("font-size",10).text(figure.config.xlabel);
    if (hasYLabel) svg.append("text").attr("transform",`translate(12,${margin.top+ih/2}) rotate(-90)`).attr("text-anchor","middle").attr("fill",C.textMuted).attr("font-size",10).text(figure.config.ylabel);
    if (hasLegend) {
      const legendLabels = figure.config.legend || figure.datasets.map(ds => ds.label).filter(Boolean);
      const legendG = svg.append("g").attr("transform",`translate(${margin.left},${height-18})`);
      let xOff = 0;
      legendLabels.forEach((label, i) => {
        const color = COLORS[i % COLORS.length];
        legendG.append("rect").attr("x",xOff).attr("y",0).attr("width",12).attr("height",3).attr("fill",color).attr("rx",1);
        legendG.append("text").attr("x",xOff+16).attr("y",4).attr("fill",C.textDim).attr("font-size",9).attr("alignment-baseline","middle").text(label);
        xOff += 16 + label.length * 6 + 12;
      });
    }
  }, [figure]);

  if (!figure) return null;
  return (
    <div ref={containerRef} style={{ background:C.bg2, border:`1px solid ${C.border}`, borderRadius:6, padding:6, position:"relative" }}>
      <div style={{ display:"flex", justifyContent:"space-between", alignItems:"center", marginBottom:4, padding:"0 2px" }}>
        <span style={{ fontSize:9, color:C.textMuted }}>
          Figure {figure.id}
          {figure.config?.subplot && ` · subplot(${figure.config.subplot.join(',')})`}
          {figure.datasets.length > 1 && ` · ${figure.datasets.length} datasets`}
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

  const handleCloseFigure = (id) => {
    onSetFigures(prev => prev.filter(f => f.id !== id));
    if (onCloseFigure) onCloseFigure(id);
  };
  const handleCloseAll = () => {
    onSetFigures([]);
    if (onCloseAll) onCloseAll();
  };

  return (
    <div style={{ display:"flex", flexDirection:"column", height:"100%", overflow:"hidden" }}>
      <div style={{ padding:"7px 10px", borderBottom:`1px solid ${C.border}`, display:"flex", justifyContent:"space-between", alignItems:"center", flexShrink:0 }}>
        <span style={{ fontSize:11, fontWeight:600, color:C.text, fontFamily:FONT_UI }}>📊 Figures</span>
        <div style={{ display:"flex", gap:4, alignItems:"center" }}>
          {deduped.length > 0 && (
            <button onClick={handleCloseAll} title="Close all figures"
              style={{ background:"none", border:"none", color:C.textMuted, cursor:"pointer", fontSize:10, fontFamily:FONT_UI }}>Close all</button>
          )}
          <button onClick={onClose} style={{ background:"none", border:"none", color:C.textMuted, cursor:"pointer", fontSize:16, lineHeight:1 }}>×</button>
        </div>
      </div>
      <div style={{ flex:1, overflowY:"auto", padding:8, display:"flex", flexDirection:"column", gap:8 }}>
        {deduped.length === 0 ? (
          <div style={{ color:C.textMuted, fontSize:11, padding:16, textAlign:"center", lineHeight:1.6 }}>
            No figures yet.<br/>Use plot(), bar(), scatter() or hist().
          </div>
        ) : deduped.map(fig => (
          <FigurePanel key={fig.id} figure={fig} onClose={() => handleCloseFigure(fig.id)} />
        ))}
      </div>
    </div>
  );
}