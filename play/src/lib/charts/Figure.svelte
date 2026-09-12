<script lang="ts">
  // Figure (R3): the renderer ROOT — owns the canvas box, dispatches
  // to the per-STAGE components (cartesian / polar / …) and draws the
  // title + legend. New coordinate systems and chart types plug in
  // here, one import each.
  import type { WireFigure } from './wire.js';
  import CartesianStage from './CartesianStage.svelte';
  import PolarStage from './PolarStage.svelte';
  import Legend from './Legend.svelte';

  let { fig, width = 560, height = 420 }: {
    fig: WireFigure; width?: number; height?: number;
  } = $props();

  const ax = $derived(fig?.axes?.[0] ?? null);
  const cfg = $derived(ax?.config ?? {});
  const polar = $derived(!!cfg.polar);
  const legendLabels = $derived((cfg.legend as string[] | undefined) ?? []);
  const hasLegend = $derived(legendLabels.length > 0);

  // Plot box: margins per MATLAB-ish chrome (labels/title earn room).
  const padL = $derived(56 + (cfg.ylabel ? 8 : 0));
  const padT = $derived(24 + (cfg.title ? 14 : 0));
  const box = $derived({
    x: padL, y: padT,
    w: Math.max(10, width - padL - 14),
    h: Math.max(10, height - padT - (42 + (cfg.xlabel ? 8 : 0))),
  });
</script>

<svg viewBox="0 0 {width} {height}" width="100%" xmlns="http://www.w3.org/2000/svg">
  <rect width="100%" height="100%" fill="var(--nk-plot-bg)" />
  {#if cfg.title}
    <text x={width / 2} y={16} font-size="13" text-anchor="middle"
          fill="var(--nk-plot-text)">{cfg.title as string}</text>
  {/if}
  {#if ax}
    {#if polar}
      <PolarStage {ax} {box} />
    {:else}
      <CartesianStage {ax} {box} />
    {/if}
    {#if hasLegend}
      <Legend labels={legendLabels} datasets={ax.datasets}
              x={box.x + box.w - 110} y={box.y + 10} />
    {/if}
  {/if}
</svg>
