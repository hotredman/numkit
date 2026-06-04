// axesModel.js — the MATLAB Axes <-> legacy-cell bridge for FigureWindow:
// pure read / write / convert functions over a single Axes object (grid,
// scale, dir, visibility, viewport). No component state — args + figureSchema.
import { isOn, onOff, setProp } from './figureSchema';

export function axisGridOn(axes) {
  return isOn(axes && axes.XGrid)     || isOn(axes && axes.YGrid)
      || isOn(axes && axes.ZGrid)
      || isOn(axes && axes.RGrid)     || isOn(axes && axes.ThetaGrid);
}
export function axisGridMinorOn(axes) {
  // Minor grids on every axis the schema models — cartesian X/Y/Z
  // and polar R/θ. Mirrors MATLAB R2025b: `grid minor` lights the
  // minor grid for every axis the current axes type supports.
  return isOn(axes && axes.XMinorGrid)     || isOn(axes && axes.YMinorGrid)
      || isOn(axes && axes.ZMinorGrid)
      || isOn(axes && axes.RMinorGrid)     || isOn(axes && axes.ThetaMinorGrid);
}
// Adapter — same shape the old `cells: CellSettings[]` exposed.
// Used by SubplotGrid (fed via the cellState renderFigure prop).
export function axesToLegacyCell(axes) {
  if (!axes) return {};
  return {
    showMajor:    axisGridOn(axes),
    showMinor:    axisGridMinorOn(axes),
    // Per-axis grid (preserves XGrid/YGrid info for SubplotGrid →
    // CompositePlot per-axis renderer split).
    xGrid:        isOn(axes.XGrid),
    yGrid:        isOn(axes.YGrid),
    xMinor:       isOn(axes.XMinorGrid),
    yMinor:       isOn(axes.YMinorGrid),
    zMinor:       isOn(axes.ZMinorGrid),
    rMinor:       isOn(axes.RMinorGrid),
    thetaMinor:   isOn(axes.ThetaMinorGrid),
    xLog:         axes.XScale === 'log',
    yLog:         axes.YScale === 'log',
    zLog:         axes.ZScale === 'log',
    showTitle:    isOn(axes.Title    && axes.Title.Visible),
    showXLabel:   isOn(axes.XLabel   && axes.XLabel.Visible),
    showYLabel:   isOn(axes.YLabel   && axes.YLabel.Visible),
    showZLabel:   isOn(axes.ZLabel   && axes.ZLabel.Visible),
    showLegend:   isOn(axes.Legend   && axes.Legend.Visible),
    showColorbar: isOn(axes.Colorbar && axes.Colorbar.Visible),
    showAxis:     isOn(axes.Visible),
    showBox:      isOn(axes.Box),
    xReverse:     axes.XDir === 'reverse',
    yReverse:     axes.YDir === 'reverse',
    zReverse:     axes.ZDir === 'reverse',
    // Aspect mode — UI-set value flows to CompositePlot's panel-
    // shrink path. Defaults to '' (auto) when neither script nor UI
    // set it. CompositePlot reads `axisMode` prop with fallback to
    // figure.axisMode (script value).
    axisMode:     axes.AxisMode || '',
    legendLocation:   axes.Legend   && axes.Legend.Location,
    colorbarLocation: axes.Colorbar && axes.Colorbar.Location,
    colormap:     axes.Colormap || null,
  };
}

export function legacyRead(a, key) {
  if (!a) return undefined;
  switch (key) {
    case 'showMajor':    return axisGridOn(a);
    case 'showMinor':    return axisGridMinorOn(a);
    case 'xGrid':        return isOn(a.XGrid);
    case 'yGrid':        return isOn(a.YGrid);
    case 'zGrid':        return isOn(a.ZGrid);
    case 'rGrid':        return isOn(a.RGrid);
    case 'thetaGrid':    return isOn(a.ThetaGrid);
    case 'xMinor':       return isOn(a.XMinorGrid);
    case 'yMinor':       return isOn(a.YMinorGrid);
    case 'zMinor':       return isOn(a.ZMinorGrid);
    case 'rMinor':       return isOn(a.RMinorGrid);
    case 'thetaMinor':   return isOn(a.ThetaMinorGrid);
    case 'xLog':         return a.XScale === 'log';
    case 'yLog':         return a.YScale === 'log';
    case 'zLog':         return a.ZScale === 'log';
    case 'showTitle':    return isOn(a.Title    && a.Title.Visible);
    case 'showXLabel':   return isOn(a.XLabel   && a.XLabel.Visible);
    case 'showYLabel':   return isOn(a.YLabel   && a.YLabel.Visible);
    case 'showZLabel':   return isOn(a.ZLabel   && a.ZLabel.Visible);
    case 'showLegend':   return isOn(a.Legend   && a.Legend.Visible);
    case 'showColorbar': return isOn(a.Colorbar && a.Colorbar.Visible);
    case 'showAxis':     return isOn(a.Visible);
    case 'showBox':      return isOn(a.Box);
    case 'xReverse':     return a.XDir === 'reverse';
    case 'yReverse':     return a.YDir === 'reverse';
    case 'zReverse':     return a.ZDir === 'reverse';
    case 'axisMode':     return a.AxisMode || 'auto';
    case 'legendLocation':   return a.Legend   && a.Legend.Location;
    case 'colorbarLocation': return a.Colorbar && a.Colorbar.Location;
    case 'colormap':     return a.Colormap;
    case 'viewport':     return viewportFromAxes(a);
    default: return undefined;
  }
}
export function legacyWrite(a, key, value, cell) {
  // Same kind-gates as initAxesFromCell: polar plots have no X/Y/Z,
  // cartesian plots have no R/θ, only 3-D plots have Z. Used by the
  // combined showMajor / showMinor fan-out so the toolbar "grid all"
  // toggle on a polar figure doesn't silently set XGrid/YGrid (which
  // would later confuse the aggregate ✓ when the user flips RGrid).
  const kind = cell && cell.kind;
  const cart  = kind !== 'polar';
  const polar = kind === 'polar';
  const d3    = kind === 'composite3d';
  switch (key) {
    case 'showMajor':    {
      // Combined "grid" toggle — fan to only those per-axis grids
      // that exist on this figure type. MATLAB R2025b: `grid on` on
      // a polar axes lights RGrid+ThetaGrid only; cartesian lights
      // XGrid+YGrid (+ZGrid for 3-D).
      const f = onOff(!!value);
      return { ...a,
        XGrid: cart  ? f : a.XGrid,
        YGrid: cart  ? f : a.YGrid,
        ZGrid: d3    ? f : a.ZGrid,
        RGrid: polar ? f : a.RGrid,
        ThetaGrid: polar ? f : a.ThetaGrid };
    }
    case 'showMinor':    {
      const f = onOff(!!value);
      return { ...a,
        XMinorGrid: cart  ? f : a.XMinorGrid,
        YMinorGrid: cart  ? f : a.YMinorGrid,
        ZMinorGrid: d3    ? f : a.ZMinorGrid,
        RMinorGrid: polar ? f : a.RMinorGrid,
        ThetaMinorGrid: polar ? f : a.ThetaMinorGrid };
    }
    case 'xGrid':        return { ...a, XGrid:          onOff(!!value) };
    case 'yGrid':        return { ...a, YGrid:          onOff(!!value) };
    case 'zGrid':        return { ...a, ZGrid:          onOff(!!value) };
    case 'rGrid':        return { ...a, RGrid:          onOff(!!value) };
    case 'thetaGrid':    return { ...a, ThetaGrid:      onOff(!!value) };
    case 'xMinor':       return { ...a, XMinorGrid:     onOff(!!value) };
    case 'yMinor':       return { ...a, YMinorGrid:     onOff(!!value) };
    case 'zMinor':       return { ...a, ZMinorGrid:     onOff(!!value) };
    case 'rMinor':       return { ...a, RMinorGrid:     onOff(!!value) };
    case 'thetaMinor':   return { ...a, ThetaMinorGrid: onOff(!!value) };
    case 'xLog':         return { ...a, XScale: value ? 'log' : 'linear' };
    case 'yLog':         return { ...a, YScale: value ? 'log' : 'linear' };
    case 'zLog':         return { ...a, ZScale: value ? 'log' : 'linear' };
    case 'showTitle':    return setProp(a, ['Title',    'Visible'], onOff(!!value));
    case 'showXLabel':   return setProp(a, ['XLabel',   'Visible'], onOff(!!value));
    case 'showYLabel':   return setProp(a, ['YLabel',   'Visible'], onOff(!!value));
    case 'showZLabel':   return setProp(a, ['ZLabel',   'Visible'], onOff(!!value));
    case 'showLegend':   return setProp(a, ['Legend',   'Visible'], onOff(!!value));
    case 'showColorbar': return setProp(a, ['Colorbar', 'Visible'], onOff(!!value));
    case 'showAxis':     return { ...a, Visible: onOff(!!value) };
    case 'showBox':      return { ...a, Box:     onOff(!!value) };
    case 'xReverse':     return { ...a, XDir: value ? 'reverse' : 'normal' };
    case 'yReverse':     return { ...a, YDir: value ? 'reverse' : 'normal' };
    case 'zReverse':     return { ...a, ZDir: value ? 'reverse' : 'normal' };
    case 'axisMode':     {
      // Aspect — keeps the AxisMode shorthand in sync with the
      // derived MATLAB property pair (DataAspectRatioMode /
      // PlotBoxAspectRatioMode). Mirrors initAxesFromCell's mapping.
      const v = String(value || 'auto');
      return { ...a,
        AxisMode: v,
        DataAspectRatioMode:    (v === 'equal' || v === 'image') ? 'manual' : 'auto',
        PlotBoxAspectRatioMode: (v === 'square') ? 'manual' : 'auto',
      };
    }
    case 'legendLocation':   return setProp(a, ['Legend',   'Location'], value);
    case 'colorbarLocation': return setProp(a, ['Colorbar', 'Location'], value);
    case 'colormap':     return { ...a, Colormap: value };
    case 'viewport':     return applyViewport(a, value);
    default: return a;
  }
}

// Polar uses the same array-pair schema as cartesian — { r: [lo, hi],
// theta: [lo, hi] } — so PolarPlot's vp.r / vp.theta accessors line up
// with what FigureWindow stores. Earlier {rmin,rmax} flat-field shape
// got dropped by PolarPlot's `Array.isArray(viewport.r)` guard, which
// made wheel-zoom / drag-zoom / inputs all no-ops.
export function viewportFromAxes(a) {
  if (!a) return null;
  if (Array.isArray(a.RLim)) {
    const out = { r: a.RLim.slice() };
    if (Array.isArray(a.ThetaLim)) out.theta = a.ThetaLim.slice();
    return out;
  }
  const out = {};
  if (Array.isArray(a.XLim)) out.x = a.XLim.slice();
  if (Array.isArray(a.YLim)) out.y = a.YLim.slice();
  if (Array.isArray(a.ZLim)) out.z = a.ZLim.slice();
  return Object.keys(out).length > 0 ? out : null;
}
export function applyViewport(a, vp) {
  if (!vp) return a;
  const out = { ...a };
  if (Array.isArray(vp.x))     out.XLim     = vp.x.slice();
  if (Array.isArray(vp.y))     out.YLim     = vp.y.slice();
  if (Array.isArray(vp.z))     out.ZLim     = vp.z.slice();
  if (Array.isArray(vp.r))     out.RLim     = vp.r.slice();
  if (Array.isArray(vp.theta)) out.ThetaLim = vp.theta.slice();
  return out;
}
