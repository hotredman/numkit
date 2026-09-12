<script lang="ts">
  // HeatmapChart (R3 — one chart per file): imagesc/image datasets —
  // per-cell rects colored through the ENGINE-SUPPLIED palette
  // (customColormap on the axes wire: byte-identical to MATLAB by
  // construction). YDir reverse is baked into cell order (row 0 top).
  import type { WireAxes, WireDataset } from './wire.js';
  import { parsePalette, namedPalette, heatmapModel, paintCells } from './heatmap.js';
  import { cartesianProjection } from './stage.js';
  import { viewport } from './stage.js';

  let { ax, box }: { ax: WireAxes | null; box: { x: number; y: number; w: number; h: number } } = $props();

  const cfg = $derived(ax?.config ?? {});
  const vp = $derived(viewport(ax));
  const { X, Y } = $derived(cartesianProjection(vp, box));

  const img = $derived.by(() => {
    const ds = (ax?.datasets ?? []).find(
      (d: WireDataset) => d.type === 'imagesc',
    );
    if (!ds) return null;
    const model = heatmapModel(ds);
    if (!model) return null;
    const palette =
      parsePalette(
        (cfg.customColormap as string | number[][] | undefined) ?? undefined,
      ).length > 0
        ? parsePalette(cfg.customColormap as string | number[][])
        : namedPalette((cfg.colormap as string) || 'parula');
    if (!palette.length) return null;
    return { model, cells: paintCells(model, palette) };
  });

  // Cell edges from the wire's edge-vector x/y ([1, C] → ±0.5 spacing).
  const edges = $derived.by(() => {
    const ds = (ax?.datasets ?? []).find((d) => d.type === 'imagesc');
    const xs = (ds?.x as number[] | undefined) ?? [1, 2];
    const ys = (ds?.y as number[] | undefined) ?? [1, 2];
    const xsp = xs.length > 1 ? (xs[1]! - xs[0]!) / 2 : 0.5;
    const ysp = ys.length > 1 ? (ys[1]! - ys[0]!) / 2 : 0.5;
    return {
      x0: xs[0]! - xsp,
      x1: xs[xs.length - 1]! + xsp,
      y0: ys[0]! - ysp,
      y1: ys[ys.length - 1]! + ysp,
    };
  });
</script>

{#if img}
  {#each img.cells as row, r}
    {#each row as cell, c}
      {@const w = (edges.x1 - edges.x0) / img.model.cols}
      {@const h = (edges.y1 - edges.y0) / img.model.rows}
      <rect
        x={X(edges.x0 + c * w)}
        y={Y(edges.y0 + (r + 1) * h)}
        width={Math.max(0.5, X(edges.x0 + (c + 1) * w) - X(edges.x0 + c * w))}
        height={Math.max(0.5, Y(edges.y0 + r * h) - Y(edges.y0 + (r + 1) * h))}
        fill={`rgb(${cell.r},${cell.g},${cell.b})`}
        shape-rendering="crispEdges"
      />
    {/each}
  {/each}
{/if}
