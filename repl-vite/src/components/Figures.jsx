import { useEffect, useRef } from "react";
import * as d3 from "d3";
import C, { FONT_UI } from "../theme";

const COLORS = ['#7c6ff0','#60d0f0','#6ee7a0','#f0a060','#e070c0','#e8d060','#f07070','#70b0f0'];

/**
 * Render a single figure with full config support.
 * Figure object: { id, datasets: [{x,y,type,label?,style?}], config: {title,xlabel,ylabel,xlim?,ylim?,grid,legend?} }
 */
function FigurePanel({ figure, onClose }) {
  const svgRef = useRef(null);
  const containerRef = useRef(null);

  useEffect(() => {
    if (!figure || !svgRef.current || !containerRef.current) return;
    const svg = d3.select(svgRef.current);
    svg.selectAll("*").remove();

    const cw = containerRef.current.clientWidth;
    const width = Math.max(200, cw - 16);
    const hasTitle = !!figure.config?.title;
    const hasXLabel = !!figure.config?.xlabel;
    const hasYLabel = !!figure.config?.ylabel;
    const hasLegend = figure.config?.legend?.length > 0 || figure.datasets.some(ds => ds.label);
    const height = 220 + (hasTitle ? 10 : 0) + (hasLegend ? 24 : 0);
    const margin = {
      top: hasTitle ? 32 : 16,
      right: 16,
      bottom: hasXLabel ? 42 : 30,
      left: hasYLabel ? 52 : 42,
    };
    const iw = width - margin.left - margin.right;
    const ih = height - margin.top - margin.bottom - (hasLegend ? 24 : 0);

    svg.attr("width", width).attr("height", height);
    const g = svg.append("g").attr("transform", `translate(${margin.left},${margin.top})`);

    // Compute data extents
    const allX = figure.datasets.flatMap(ds => ds.x);
    const allY = figure.datasets.flatMap(ds => ds.y).filter(v => v !== null);

    let xMin = Math.min(...allX), xMax = Math.max(...allX);
    let yMin = Math.min(...allY), yMax = Math.max(...allY);

    // Apply xlim/ylim if provided
    if (figure.config?.xlim && figure.config.xlim.length >= 2) {
      xMin = figure.config.xlim[0];
      xMax = figure.config.xlim[1];
    }
    if (figure.config?.ylim && figure.config.ylim.length >= 2) {
      yMin = figure.config.ylim[0];
      yMax = figure.config.ylim[1];
    } else {
      // Auto-pad y range
      const yPad = (yMax - yMin) * 0.05 || 1;
      yMin -= yPad;
      yMax += yPad;
    }

    const xScale = d3.scaleLinear().domain([xMin, xMax]).range([0, iw]).nice();
    const yScale = d3.scaleLinear().domain([yMin, yMax]).range([ih, 0]).nice();

    // Grid lines
    if (figure.config?.grid) {
      g.append("g").selectAll("line").data(yScale.ticks(5)).enter().append("line")
        .attr("x1", 0).attr("x2", iw).attr("y1", d => yScale(d)).attr("y2", d => yScale(d))
        .attr("stroke", C.border).attr("stroke-dasharray", "3,3").attr("opacity", 0.6);
      g.append("g").selectAll("line").data(xScale.ticks(5)).enter().append("line")
        .attr("x1", d => xScale(d)).attr("x2", d => xScale(d)).attr("y1", 0).attr("y2", ih)
        .attr("stroke", C.border).attr("stroke-dasharray", "3,3").attr("opacity", 0.6);
    } else {
      // Light horizontal guides
      g.append("g").selectAll("line").data(yScale.ticks(4)).enter().append("line")
        .attr("x1", 0).attr("x2", iw).attr("y1", d => yScale(d)).attr("y2", d => yScale(d))
        .attr("stroke", C.border).attr("stroke-dasharray", "2,4");
    }

    // Axes
    g.append("g").attr("transform", `translate(0,${ih})`).call(d3.axisBottom(xScale).ticks(5))
      .selectAll("text,line,path").attr("fill", C.textMuted).attr("stroke", C.textMuted);
    g.append("g").call(d3.axisLeft(yScale).ticks(4))
      .selectAll("text,line,path").attr("fill", C.textMuted).attr("stroke", C.textMuted);

    // Datasets
    figure.datasets.forEach((ds, idx) => {
      const color = parseStyleColor(ds.style) || COLORS[idx % COLORS.length];
      const dasharray = parseStyleDash(ds.style);
      const plotType = ds.type || 'line';

      if (plotType === "line") {
        const lineGen = d3.line()
          .defined((_, i) => ds.y[i] !== null)
          .x((_, i) => xScale(ds.x[i]))
          .y((_, i) => yScale(ds.y[i]))
          .curve(d3.curveMonotoneX);
        g.append("path").datum(ds.y)
          .attr("d", lineGen)
          .attr("fill", "none")
          .attr("stroke", color)
          .attr("stroke-width", 2)
          .attr("stroke-dasharray", dasharray);
      } else if (plotType === "scatter") {
        g.selectAll(`.dot-${idx}`)
          .data(ds.x.map((x, i) => ({ x, y: ds.y[i] })).filter(d => d.y !== null))
          .enter().append("circle")
          .attr("cx", d => xScale(d.x)).attr("cy", d => yScale(d.y))
          .attr("r", 4).attr("fill", color).attr("opacity", 0.8);
      } else if (plotType === "bar") {
        const bw = Math.max(2, iw / ds.x.length * 0.7);
        g.selectAll(`.bar-${idx}`)
          .data(ds.x.map((x, i) => ({ x, y: ds.y[i] })).filter(d => d.y !== null))
          .enter().append("rect")
          .attr("x", d => xScale(d.x) - bw / 2).attr("y", d => yScale(Math.max(0, d.y)))
          .attr("width", bw).attr("height", d => Math.abs(yScale(0) - yScale(d.y)))
          .attr("fill", color).attr("opacity", 0.85).attr("rx", 2);
      }
    });

    // Title
    if (hasTitle) {
      svg.append("text")
        .attr("x", width / 2).attr("y", 16)
        .attr("text-anchor", "middle").attr("fill", C.text)
        .attr("font-size", 12).attr("font-weight", 600)
        .text(figure.config.title);
    }

    // X label
    if (hasXLabel) {
      svg.append("text")
        .attr("x", width / 2).attr("y", height - (hasLegend ? 28 : 4))
        .attr("text-anchor", "middle").attr("fill", C.textMuted).attr("font-size", 10)
        .text(figure.config.xlabel);
    }

    // Y label
    if (hasYLabel) {
      svg.append("text")
        .attr("transform", `translate(12,${margin.top + ih / 2}) rotate(-90)`)
        .attr("text-anchor", "middle").attr("fill", C.textMuted).attr("font-size", 10)
        .text(figure.config.ylabel);
    }

    // Legend
    if (hasLegend) {
      const legendLabels = figure.config.legend || figure.datasets.map(ds => ds.label).filter(Boolean);
      const legendG = svg.append("g")
        .attr("transform", `translate(${margin.left}, ${height - 18})`);
      let xOff = 0;
      legendLabels.forEach((label, i) => {
        const color = COLORS[i % COLORS.length];
        legendG.append("rect")
          .attr("x", xOff).attr("y", 0).attr("width", 12).attr("height", 3)
          .attr("fill", color).attr("rx", 1);
        legendG.append("text")
          .attr("x", xOff + 16).attr("y", 4)
          .attr("fill", C.textDim).attr("font-size", 9).attr("alignment-baseline", "middle")
          .text(label);
        xOff += 16 + label.length * 6 + 12;
      });
    }
  }, [figure]);

  if (!figure) return null;

  return (
    <div ref={containerRef} style={{
      background: C.bg2, border: `1px solid ${C.border}`, borderRadius: 6,
      padding: 6, position: "relative",
    }}>
      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 4, padding: "0 2px" }}>
        <span style={{ fontSize: 9, color: C.textMuted }}>
          Figure {figure.id}
          {figure.datasets.length > 1 && ` · ${figure.datasets.length} datasets`}
        </span>
        <button onClick={onClose} style={{ background: "none", border: "none", color: C.textMuted, cursor: "pointer", fontSize: 13, lineHeight: 1 }}>×</button>
      </div>
      <svg ref={svgRef} style={{ display: "block", margin: "0 auto" }} />
    </div>
  );
}

/** Parse MATLAB-style color from style string like 'r', 'b--', 'g:' */
function parseStyleColor(style) {
  if (!style) return null;
  const colorMap = { r: '#f07070', g: '#6ee7a0', b: '#60d0f0', k: '#d4d4f0', m: '#e070c0', c: '#60d0f0', y: '#e8d060', w: '#ffffff' };
  for (const ch of style) {
    if (colorMap[ch]) return colorMap[ch];
  }
  return null;
}

/** Parse dash style from style string like '--', ':', '-.' */
function parseStyleDash(style) {
  if (!style) return null;
  if (style.includes('-.')) return '8,3,2,3';
  if (style.includes('--')) return '8,4';
  if (style.includes(':')) return '2,4';
  return null;
}

/**
 * Figures panel — shows all figures, with figure management.
 * New figures with the same ID replace old ones (MATLAB behavior).
 *
 * Props:
 *   figures      — array of figure objects from engine
 *   onSetFigures — state setter
 *   onClose      — close panel
 */
export default function Figures({ figures, onSetFigures, onClose }) {
  // Deduplicate by ID — keep latest version of each figure
  const deduped = [];
  const seen = new Set();
  for (let i = figures.length - 1; i >= 0; i--) {
    const id = figures[i].id;
    if (!seen.has(id)) {
      seen.add(id);
      deduped.unshift(figures[i]);
    }
  }

  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%", overflow: "hidden" }}>
      <div style={{ padding: "7px 10px", borderBottom: `1px solid ${C.border}`, display: "flex", justifyContent: "space-between", alignItems: "center", flexShrink: 0 }}>
        <span style={{ fontSize: 11, fontWeight: 600, color: C.text, fontFamily: FONT_UI }}>📊 Figures</span>
        <div style={{ display: "flex", gap: 4, alignItems: "center" }}>
          {deduped.length > 0 && (
            <button onClick={() => onSetFigures([])} title="Clear all figures"
              style={{ background: "none", border: "none", color: C.textMuted, cursor: "pointer", fontSize: 10, fontFamily: FONT_UI }}>Clear all</button>
          )}
          <button onClick={onClose} style={{ background: "none", border: "none", color: C.textMuted, cursor: "pointer", fontSize: 16, lineHeight: 1 }}>×</button>
        </div>
      </div>
      <div style={{ flex: 1, overflowY: "auto", padding: 8, display: "flex", flexDirection: "column", gap: 8 }}>
        {deduped.length === 0 ? (
          <div style={{ color: C.textMuted, fontSize: 11, padding: 16, textAlign: "center", lineHeight: 1.6 }}>
            No figures yet.<br />Use plot(), bar(), scatter() or hist().
          </div>
        ) : (
          deduped.map((fig) => (
            <FigurePanel key={fig.id} figure={fig}
              onClose={() => onSetFigures(prev => prev.filter(f => f.id !== fig.id))} />
          ))
        )}
      </div>
    </div>
  );
}
