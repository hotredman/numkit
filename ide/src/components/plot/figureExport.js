// Pure builders for FigureWindow's CSV / TSV / JSON exporters.
//
// Extracted from FigureWindow so the non-trivial per-kind branch logic
// (3-D / heatmap / subplot / composite / plain series) can be unit-tested
// without mounting the window. Everything here is pure: the component keeps
// thin download handlers and passes the 3-D rows it pulled from the GL handle
// in as ctx.threeRows, so this module never touches a ref.
//
// ctx shape: { figure, is3D, isHeatmap, isSubplot, isComposite,
//              heatmapLayer, seriesLayers, compositeLayers, threeRows }

export function seriesBody(source, sep) {
  const list = Array.isArray(source) ? source : (source.series || []);
  const has3D = list.some((s) => Array.isArray(s.z));
  const rows = [`name${sep}x${sep}y${has3D ? sep + 'z' : ''}`];
  list.forEach((s) => {
    const xs = s.x || s.theta || [];
    const ys = s.y || s.rho   || [];
    const zs = Array.isArray(s.z) ? s.z : null;
    for (let i = 0; i < xs.length; i++) {
      let row = `${s.name}${sep}${xs[i]}`;
      if (ys[i] != null) row += sep + ys[i];
      if (zs && zs[i] != null) row += sep + zs[i];
      rows.push(row);
    }
  });
  return rows.join('\n');
}

// Composite cell exporter — pulls heatmap layer's z if present, else series.
export function compositeCellBody(cell, sep) {
  const layers = cell.layers || [];
  const hl = layers.find((l) => l.kind === 'heatmap');
  if (hl) return hl.z.map((row) => row.map((v) => v == null ? '' : v).join(sep)).join('\n');
  return seriesBody(layers.filter((l) => l.kind === 'series'), sep);
}

// Unified CSV / TSV body — the two were byte-identical apart from the
// delimiter; the component picks the mime type + file extension.
export function buildDelimited(ctx, sep) {
  const { figure, is3D, isHeatmap, isSubplot, isComposite, heatmapLayer, seriesLayers, threeRows } = ctx;
  if (is3D) {
    return seriesBody(threeRows, sep);
  }
  if (isHeatmap) {
    const z = heatmapLayer.z;
    return z.map((row) => row.map((v) => v == null ? '' : v).join(sep)).join('\n');
  }
  if (isSubplot) {
    const parts = figure.cells.map((c, i) => {
      const tag = `# subplot ${c.subplotIndex || i + 1} — ${c.title || c.kind}`;
      if (c.kind === 'composite') return `${tag}\n` + compositeCellBody(c, sep);
      return `${tag}\n` + seriesBody(c, sep);
    });
    return parts.join('\n\n');
  }
  if (isComposite) {
    return seriesBody(seriesLayers, sep);
  }
  return seriesBody(figure, sep);
}

// JSON export object — structurally per-kind. Returns a plain object; the
// component stringifies + downloads it.
export function buildJsonObject(ctx) {
  const { figure, is3D, isHeatmap, isSubplot, isComposite, heatmapLayer, compositeLayers, threeRows } = ctx;
  if (is3D) {
    return {
      id: figure.id, kind: 'composite3d',
      title: figure.title,
      xLabel: figure.xLabel, yLabel: figure.yLabel, zLabel: figure.zLabel,
      view: figure.view,
      series: threeRows,
    };
  }
  if (isHeatmap) {
    return {
      id: figure.id, kind: 'heatmap', title: figure.title,
      xRange: figure.xRange, yRange: figure.yRange,
      cmin: heatmapLayer.cmin, cmax: heatmapLayer.cmax,
      colormap: heatmapLayer.colormap, z: heatmapLayer.z,
    };
  }
  if (isSubplot) {
    return {
      id: figure.id, kind: 'subplot', title: figure.title, grid: figure.grid,
      cells: figure.cells.map((c) => {
        if (c.kind === 'composite') {
          const layers = c.layers || [];
          return {
            subplotIndex: c.subplotIndex, kind: 'composite', title: c.title,
            xLabel: c.xLabel, yLabel: c.yLabel,
            xRange: c.xRange, yRange: c.yRange,
            layers: layers.map((ly) => {
              if (ly.kind === 'heatmap') return { kind: 'heatmap', z: ly.z, cmin: ly.cmin, cmax: ly.cmax };
              if (ly.kind === 'series')  return { kind: 'series', mode: ly.mode, name: ly.name, color: ly.color, x: ly.x, y: ly.y };
              return { ...ly };
            }),
          };
        }
        return { subplotIndex: c.subplotIndex, kind: c.kind, title: c.title,
          xLabel: c.xLabel, yLabel: c.yLabel,
          series: (c.series || []).map((s) => ({
            name: s.name, color: s.color, x: s.x ?? s.theta, y: s.y ?? s.rho,
          })),
        };
      }),
    };
  }
  if (isComposite) {
    return {
      id: figure.id, kind: 'composite', title: figure.title,
      xLabel: figure.xLabel, yLabel: figure.yLabel,
      xRange: figure.xRange, yRange: figure.yRange,
      layers: compositeLayers.map((ly) => {
        if (ly.kind === 'heatmap') return { kind: 'heatmap', z: ly.z, cmin: ly.cmin, cmax: ly.cmax, colormap: ly.colormap };
        if (ly.kind === 'series')  return { kind: 'series', mode: ly.mode, name: ly.name, color: ly.color, x: ly.x, y: ly.y };
        return { ...ly };
      }),
    };
  }
  return {
    id: figure.id, kind: figure.kind, title: figure.title,
    xLabel: figure.xLabel, yLabel: figure.yLabel,
    series: (figure.series || []).map((s) => ({
      name: s.name, color: s.color,
      x: s.x ?? s.theta, y: s.y ?? s.rho,
    })),
  };
}
