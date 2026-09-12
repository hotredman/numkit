// THE WIRE CONTRACT, typed (rule R7 + R8). This is the shape the
// engine's FigureManager emits as __FIGURE_DATA__ (built and
// parity-verified in portions 29–41). Every field here mirrors the
// emitter in src/figure/include/nk/figure/figure_manager.hpp — when
// the wire grows, this file grows IN THE SAME COMMIT.

// ── Chart datasets (per-type field unions) ──────────────────────────
export interface LineDataset {
  type?: 'line' | 'comet' | 'animatedline';
  x: number[];
  y: number[];
  /** MATLAB linespec hint, e.g. "r--o", or kv "color=#hex;…". */
  style?: string;
  label?: string;
  lineWidth?: number;
  markerSize?: number;
  visible?: boolean;
}
export interface ImageDataset {
  type: 'imagesc' | 'image-rgb';
  x: number[];
  y: number[];
  /** Nested quantized indices (imagesc) — renderer resolves via cmap. */
  z?: unknown;
  cminOrig?: number;
  cmaxOrig?: number;
  originalRows?: number;
  originalCols?: number;
}
export interface TextDataset {
  type: 'text';
  x: number[];
  y: number[];
  label?: string;
}
export type WireDataset = LineDataset | ImageDataset | TextDataset
  | ({ type: string } & Record<string, unknown>);

// ── Axes config (chrome) ────────────────────────────────────────────
export interface WireAxesConfig {
  title?: string;
  subtitle?: string;
  xlabel?: string;
  ylabel?: string;
  polar?: boolean;
  xscale?: 'linear' | 'log';
  yscale?: 'linear' | 'log';
  xlim?: [number, number];
  ylim?: [number, number];
  xticks?: number[];
  yticks?: number[];
  xticklabels?: string[];
  yticklabels?: string[];
  grid?: 'on' | 'off';
  /** Portion 34: fontSize (labels 1.1×), fontWeight, fontName. */
  fontSize?: number;
  fontWeight?: string;
  fontName?: string;
  axesLineWidth?: number;
  /** Portion 38: colorbar ("east"|"west"|…) + location. */
  colorbarLocation?: string;
  colormap?: string;
  legend?: string[];
  legendLocation?: string;
  axisMode?: string;
  /** false = axis off (imshow). */
  axisVisible?: boolean;
  yDir?: 'normal' | 'reverse';
  [k: string]: unknown;
}

export interface WireAxes {
  config: WireAxesConfig;
  datasets: WireDataset[];
}
export interface WireFigure {
  id: number;
  subplotGrid?: [number, number];
  axes: WireAxes[];
  superTitle?: string;
}

// ── Wire channel parsing ────────────────────────────────────────────
export interface WireParseResult {
  figures: WireFigure[];
  closed: (number | 'all')[];
}

export function parseFigureWire(text: string | null | undefined): WireParseResult {
  const figures: WireFigure[] = [];
  const closed: (number | 'all')[] = [];
  for (const line of String(text ?? '').split('\n')) {
    const t = line.trim();
    if (t.startsWith('__FIGURE_DATA__:')) {
      try {
        figures.push(JSON.parse(t.slice('__FIGURE_DATA__:'.length)));
      } catch { /* skip malformed line */ }
    } else if (t.startsWith('__FIGURE_CLOSE__:')) {
      closed.push(parseInt(t.split(':')[1], 10));
    } else if (t === '__FIGURE_CLOSE_ALL__') {
      closed.push('all');
    }
  }
  return { figures, closed };
}

// ── @play manifest ──────────────────────────────────────────────────
export interface PlaySliderControl {
  kind: 'slider';
  var: string;
  label: string;
  min: number;
  max: number;
  step: number;
  unit: string;
  value: number;
}
export interface PlaySelectControl {
  kind: 'select';
  var: string;
  label: string;
  options: string[];
  value: string;
}
export type PlayControl = PlaySliderControl | PlaySelectControl;
export interface PlayControlGroup {
  name: string;
  controls: PlayControl[];
}

export function flattenControls(
  manifestJson: string | { groups?: PlayManifestGroup[] } | null | undefined,
): PlayControlGroup[] {
  if (!manifestJson) return [];
  const m = typeof manifestJson === 'string'
    ? (JSON.parse(manifestJson) as { groups?: PlayManifestGroup[] })
    : (manifestJson as { groups?: PlayManifestGroup[] });
  const out: PlayControlGroup[] = [];
  for (const g of m.groups ?? []) {
    const controls: PlayControl[] = [];
    for (const c of g.controls ?? []) {
      controls.push(c.type === 'select'
        ? { kind: 'select', var: c.var, label: c.label ?? c.var,
            options: c.options ?? [],
            value: String(c.default ?? (c.options?.[0] ?? '')) }
        : { kind: 'slider', var: c.var, label: c.label ?? c.var,
            min: c.min ?? 0, max: c.max ?? 100, step: c.step ?? 1,
            unit: c.unit ?? '',
            value: Number(c.default ?? c.min ?? 0) });
    }
    if (controls.length || g.name)
      out.push({ name: g.name ?? '', controls });
  }
  return out;
}

interface PlayManifestGroup {
  name?: string;
  controls?: Array<{
    var: string;
    label?: string;
    type?: string;
    min?: number;
    max?: number;
    step?: number;
    unit?: string;
    default?: number | string;
    options?: string[];
  }>;
}
