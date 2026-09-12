// Pure STAGE math (rule R5): the coordinate-system models a chart
// renders onto. Cartesian = viewport limits + linear projection;
// polar = r-max + centre/radius mapping. Framework-free.
import type { WireAxes } from './wire.js';
import { lineSeries } from './series.js';

export interface Viewport { x0: number; x1: number; y0: number; y1: number; }
export interface PixelBox { x: number; y: number; w: number; h: number; }
export interface Projection { X(v: number): number; Y(v: number): number; }
export interface PolarModel {
  rMax: number; cx: number; cy: number;
  R(r: number): number;
  X(th: number, r: number): number;
  Y(th: number, r: number): number;
}

export function viewport(ax: WireAxes | null): Viewport {
  const cfg = ax?.config ?? {};
  let [x0, x1] = (cfg.xlim as [number, number]) ?? [Infinity, -Infinity];
  let [y0, y1] = (cfg.ylim as [number, number]) ?? [Infinity, -Infinity];
  for (const ds of lineSeries(ax)) {
    const xs = ds.x ?? [], ys = ds.y ?? [];
    for (let k = 0; k < Math.min(xs.length, ys.length); k++)
      if (Number.isFinite(xs[k]!) && Number.isFinite(ys[k]!)) {
        if (!cfg.xlim) { x0 = Math.min(x0, xs[k]!); x1 = Math.max(x1, xs[k]!); }
        if (!cfg.ylim) { y0 = Math.min(y0, ys[k]!); y1 = Math.max(y1, ys[k]!); }
      }
  }
  if (!(x1 > x0)) { x0 = 0; x1 = 1; }
  if (!(y1 > y0)) { y0 -= 1; y1 += 1; }
  const pad = (hi: number, lo: number) => (hi - lo) * 0.05;
  if (!cfg.xlim) { x0 -= pad(x1, x0); x1 += pad(x1, x0); }
  if (!cfg.ylim) { y0 -= pad(y1, y0); y1 += pad(y1, y0); }
  return { x0, x1, y0, y1 };
}

export function cartesianProjection(vp: Viewport, box: PixelBox): Projection {
  return {
    X: (v) => box.x + ((v - vp.x0) / (vp.x1 - vp.x0)) * box.w,
    Y: (v) => box.y + box.h - ((v - vp.y0) / (vp.y1 - vp.y0)) * box.h,
  };
}

export function polarModel(ax: WireAxes | null, box: PixelBox): PolarModel {
  let rMax = 0;
  for (const ds of lineSeries(ax))
    for (const v of ds.y ?? [])
      if (Number.isFinite(v)) rMax = Math.max(rMax, Math.abs(v));
  if (!(rMax > 0)) rMax = 1;
  const cx = box.x + box.w / 2;
  const cy = box.y + box.h / 2;
  const scale = Math.min(box.w, box.h) / 2 - 6;
  const R = (r: number) => (Math.abs(r) / rMax) * scale;
  return {
    rMax, cx, cy, R,
    X: (th, r) => cx + R(r) * Math.cos(th),
    Y: (th, r) => cy - R(r) * Math.sin(th),
  };
}
