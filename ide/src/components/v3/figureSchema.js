/**
 * figureSchema.js — MATLAB-style HG2 schema for figure UI state.
 *
 * Follows MATLAB R2025b axes property names so users with MATLAB
 * background find familiar terminology and so the schema is amenable
 * to future `get(gca, 'XGrid')`-style script API.
 *
 * Shape (one Axes per cell; non-subplot figures = single Axes):
 *
 *   Axes {
 *     // Visibility / box
 *     Visible: 'on'|'off'             axis lines + ticks (axis on/off)
 *     Box: 'on'|'off'                 plot frame (box on/off)
 *
 *     // Per-axis grid (MATLAB lets X/Y/Z grid be toggled independently)
 *     XGrid, YGrid, ZGrid: 'on'|'off'
 *     XMinorGrid, YMinorGrid, ZMinorGrid: 'on'|'off'
 *
 *     // Per-axis scale + direction
 *     XScale, YScale, ZScale: 'linear'|'log'
 *     XDir,   YDir,   ZDir:   'normal'|'reverse'
 *
 *     // Limits (viewport)
 *     XLim, YLim: [lo, hi]
 *     ZLim:       [lo, hi] (3-D)
 *     RLim, ThetaLim: polar
 *     XLimMode, YLimMode, ZLimMode: 'auto'|'manual'  (manual = user-pinned)
 *
 *     // Aspect mode (axis equal/square/image/tight)
 *     DataAspectRatioMode:  'auto'|'manual'
 *     PlotBoxAspectRatioMode: 'auto'|'manual'
 *
 *     // 3-D camera
 *     View: [az, el]
 *
 *     // Children — text objects with own props
 *     Title:    Text
 *     Subtitle: Text
 *     XLabel, YLabel, ZLabel, YLabel2: Text
 *
 *     // Legend / colorbar (companion graphic objects, may be null)
 *     Legend:   Legend   | null
 *     Colorbar: Colorbar | null
 *
 *     // Colour
 *     Colormap: string | null         null = use cell's script colormap
 *     CustomColormap: number[][] | null
 *     CLim: [cmin, cmax] | null       null = auto from script
 *   }
 *
 *   Text {
 *     String: string        the text content (mirrors script-set value)
 *     Visible: 'on'|'off'   user toggle (display ▾ title row)
 *   }
 *
 *   Legend {
 *     Visible: 'on'|'off'
 *     Location: 'best'|'north'|'south'|...|'none'|null   null = follow script
 *   }
 *
 *   Colorbar {
 *     Visible: 'on'|'off'
 *     Location: 'east'|'west'|'north'|'south'|null
 *   }
 *
 * Helpers:
 *   initAxesFromCell(cell): build an Axes from our internal figure-cell JSON
 *   getProp(axes, path):    nested read, e.g. ['Title', 'Visible']
 *   setProp(axes, path, v): immutable nested write
 *   everyAxes(axesArr, pred): aggregate predicate
 *   isOn(value): treats 'on' / true / 1 as on
 *   onOff(bool): 'on' | 'off'
 */

import { defaultViewport } from './figureCellState';

/** MATLAB convention — boolean as 'on'/'off' string. */
export const onOff = (b) => (b ? 'on' : 'off');
/** Inverse: 'on' | true | 1 → true; 'off' | false → false. */
export const isOn  = (v) => v === 'on' || v === true || v === 1;

/** Build one Axes object from our internal figure-cell JSON. */
export function initAxesFromCell(cell) {
  if (!cell) return clone(EMPTY_AXES);

  const legendUserAsked = (Array.isArray(cell.legend) && cell.legend.length > 0)
                       || (cell.legendLocation && cell.legendLocation !== 'none');
  const colorbarUserAsked = !!cell.colorbarLocation && cell.colorbarLocation !== 'off';
  const titleSet  = !!(cell.title && !cell.titleAuto);

  const vp = defaultViewport(cell);

  return {
    // Axis visibility / box
    Visible: onOff(cell.axisVisible !== false),
    Box:     onOff(cell.boxOn !== false),

    // Grid (per-axis). The wire `cell.grid` is figure-wide on/off; we
    // copy it onto X/Y (and Z for 3-D) so each axis can be toggled
    // independently from the toolbar.
    XGrid:      onOff(cell.grid === 'on'),
    YGrid:      onOff(cell.grid === 'on'),
    ZGrid:      onOff(cell.grid === 'on' && cell.kind === 'composite3d'),
    XMinorGrid: onOff(cell.gridMinor === 'on'),
    YMinorGrid: onOff(cell.gridMinor === 'on'),
    ZMinorGrid: onOff(cell.gridMinor === 'on' && cell.kind === 'composite3d'),

    // Scale & direction
    XScale: cell.xscale === 'log' ? 'log' : 'linear',
    YScale: cell.yscale === 'log' ? 'log' : 'linear',
    ZScale: 'linear',
    XDir:   cell.xDir === 'reverse' ? 'reverse' : 'normal',
    YDir:   cell.yDir === 'reverse' ? 'reverse' : 'normal',
    ZDir:   'normal',

    // Limits — derived from defaultViewport(cell), which handles
    // polar / 3-D / 2-D. polar viewport carries .rmax etc.; cartesian
    // / 3-D carry .x .y .z. Keep both shapes accessible via XLim/YLim
    // for cartesian + RLim/ThetaLim for polar.
    XLim: Array.isArray(vp.x) ? vp.x.slice() : null,
    YLim: Array.isArray(vp.y) ? vp.y.slice() : null,
    ZLim: Array.isArray(vp.z) ? vp.z.slice() : null,
    RLim: vp.rmin != null && vp.rmax != null ? [vp.rmin, vp.rmax] : null,
    ThetaLim: vp.thetaMin != null && vp.thetaMax != null
              ? [vp.thetaMin, vp.thetaMax] : null,
    XLimMode: 'auto', YLimMode: 'auto', ZLimMode: 'auto',
    RLimMode: 'auto', ThetaLimMode: 'auto',

    // Aspect (axisMode)
    DataAspectRatioMode:    cell.axisMode === 'equal' || cell.axisMode === 'image'
                            ? 'manual' : 'auto',
    PlotBoxAspectRatioMode: cell.axisMode === 'square' ? 'manual' : 'auto',
    AxisMode: cell.axisMode || 'auto',  // shorthand: auto/tight/equal/square/image

    // 3-D camera
    View: Array.isArray(cell.view) && cell.view.length === 2
          ? cell.view.slice() : null,

    // Text children. Each Text has its own .Visible toggle so display ▾
    // can hide a title independently of the script's text.
    Title:    { String: titleSet ? cell.title : '',     Visible: onOff(titleSet) },
    Subtitle: { String: cell.subtitle || '',            Visible: onOff(!!cell.subtitle) },
    XLabel:   { String: cell.xLabel || '',              Visible: onOff(!!cell.xLabel) },
    YLabel:   { String: cell.yLabel || '',              Visible: onOff(!!cell.yLabel) },
    YLabel2:  { String: cell.yLabel2 || '',             Visible: onOff(!!cell.yLabel2) },
    ZLabel:   { String: cell.zLabel || '',              Visible: onOff(!!cell.zLabel) },

    // Legend / Colorbar companion objects.
    Legend: {
      Visible:  onOff(!!legendUserAsked),
      Location: null,   // null = follow script's legendLocation
    },
    Colorbar: {
      Visible:  onOff(colorbarUserAsked),
      Location: null,
    },

    // Colour
    Colormap:       null,   // null = follow cell's script-set heatmap.colormap
    CustomColormap: null,
    CLim:           null,   // null = auto
  };
}

/** Default Axes for missing/empty cell. */
export const EMPTY_AXES = Object.freeze({
  Visible: 'on', Box: 'on',
  XGrid: 'off', YGrid: 'off', ZGrid: 'off',
  XMinorGrid: 'off', YMinorGrid: 'off', ZMinorGrid: 'off',
  XScale: 'linear', YScale: 'linear', ZScale: 'linear',
  XDir: 'normal', YDir: 'normal', ZDir: 'normal',
  XLim: [-1, 1], YLim: [-1, 1], ZLim: null,
  RLim: null, ThetaLim: null,
  XLimMode: 'auto', YLimMode: 'auto', ZLimMode: 'auto',
  RLimMode: 'auto', ThetaLimMode: 'auto',
  DataAspectRatioMode: 'auto',
  PlotBoxAspectRatioMode: 'auto',
  AxisMode: 'auto',
  View: null,
  Title:    { String: '', Visible: 'off' },
  Subtitle: { String: '', Visible: 'off' },
  XLabel:   { String: '', Visible: 'off' },
  YLabel:   { String: '', Visible: 'off' },
  YLabel2:  { String: '', Visible: 'off' },
  ZLabel:   { String: '', Visible: 'off' },
  Legend:   { Visible: 'off', Location: null },
  Colorbar: { Visible: 'off', Location: null },
  Colormap: null, CustomColormap: null, CLim: null,
});

/** Read a property by path. e.g. getProp(axes, 'XGrid') or
 *  getProp(axes, ['Title', 'Visible']). Returns undefined on miss. */
export function getProp(axes, path) {
  if (!axes) return undefined;
  const parts = Array.isArray(path) ? path : [path];
  let cur = axes;
  for (const p of parts) {
    if (cur == null) return undefined;
    cur = cur[p];
  }
  return cur;
}

/** Immutably set a property by path. Returns a new Axes object. */
export function setProp(axes, path, value) {
  const parts = Array.isArray(path) ? path : [path];
  if (parts.length === 0) return axes;
  if (parts.length === 1) {
    return { ...axes, [parts[0]]: value };
  }
  const [head, ...rest] = parts;
  const child = axes && axes[head];
  return { ...axes, [head]: setProp(child || {}, rest, value) };
}

/** Aggregate predicate over an axes array. true iff every axes
 *  satisfies pred(getProp(axes, path)). For booleanish props the
 *  caller can pass `isOn` as pred. */
export function everyAxes(axesArr, path, pred = isOn) {
  if (!Array.isArray(axesArr) || axesArr.length === 0) return false;
  return axesArr.every((a) => !!pred(getProp(a, path)));
}

/** Aggregate set: set the same value on every axes (toolbar fan-all). */
export function setAllAxes(axesArr, path, value) {
  return axesArr.map((a) => setProp(a, path, value));
}

/** Set a single axes by index. */
export function setAxesAt(axesArr, idx, path, value) {
  const out = axesArr.slice();
  out[idx] = setProp(out[idx] || clone(EMPTY_AXES), path, value);
  return out;
}

function clone(obj) { return JSON.parse(JSON.stringify(obj)); }
