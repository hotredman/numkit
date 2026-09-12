// Pure heatmap math (rule R5): colormap LUT + clim scaling for the
// wire's imagesc datasets. Two palette sources, BOTH engine-truth:
//   customColormap — N×3 in [0,1] after colormap(M)/named call dumps
//   colormap name  — resolved through the IDE's MATLAB-dumped tables
//                    (@ide-plot/colormaps.js, portion-35 parity).
import type { WireAxes, WireDataset } from './wire.js';
import { getColormap, getLUT } from '@ide-plot/colormaps.js';

export interface RgbTuple {
  r: number;
  g: number;
  b: number;
}

export function parsePalette(
  json: string | number[][] | undefined,
): RgbTuple[] {
  const rows: number[][] =
    typeof json === 'string' ? (JSON.parse(json) as number[][]) : (json ?? []);
  return rows.map(([r, g, b]) => ({
    r: Math.round((r as number) * 255),
    g: Math.round((g as number) * 255),
    b: Math.round((b as number) * 255),
  }));
}

/** Named palette → 256 RGB triplets from the IDE's MATLAB-dumped
 *  tables (identical to the engine export renderer's bake). */
export function namedPalette(name: string): RgbTuple[] {
  const lut = getLUT(getColormap(name));
  const out: RgbTuple[] = [];
  for (let i = 0; i < 256; i++)
    out.push({ r: lut[i * 3]!, g: lut[i * 3 + 1]!, b: lut[i * 3 + 2]! });
  return out;
}

export interface HeatmapModel {
  rows: number;
  cols: number;
  cell(r: number, c: number): number;
  range: number;
  cmin: number;
  cmax: number;
}

export function heatmapModel(ds: WireDataset): HeatmapModel | null {
  const z = (ds as { z?: number[][] }).z;
  const rows = (ds as { originalRows?: number }).originalRows
    ?? (z?.length ?? 0);
  const cols = (ds as { originalCols?: number }).originalCols
    ?? (z?.[0]?.length ?? 0);
  if (!rows || !cols || !z) return null;
  const cmin = (ds as { cminOrig?: number }).cminOrig ?? 0;
  const cmax = (ds as { cmaxOrig?: number }).cmaxOrig ?? 1;
  const range = cmax - cmin || 1;
  const n = z.length;
  const cell = (r: number, c: number): number => {
    const v = z[Math.min(r, n - 1)]?.[Math.min(c, (z[r]?.length ?? 1) - 1)];
    return Number.isFinite(v as number) ? (v as number) : cmin;
  };
  return { rows, cols, cell, range, cmin, cmax };
}

export function paintCells(
  model: HeatmapModel,
  palette: RgbTuple[],
): RgbTuple[][] {
  const cells: RgbTuple[][] = [];
  const last = palette.length - 1;
  for (let r = 0; r < model.rows; r++) {
    const row: RgbTuple[] = [];
    for (let c = 0; c < model.cols; c++) {
      const v = model.cell(r, c);
      const t = Math.min(1, Math.max(0, (v - model.cmin) / model.range));
      row.push(palette[Math.round(t * last)] ?? palette[0]!);
    }
    cells.push(row);
  }
  return cells;
}
