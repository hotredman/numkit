<script>
  // Figure (R3): the renderer ROOT — owns the canvas box, dispatches
  // to the per-type components (cartesian / polar / …) and draws the
  // title + legend. New chart types plug in here, one import each.
  import { T } from '../tokens.js';
  import PlotCartesian from './PlotCartesian.svelte';
  import PlotPolar from './PlotPolar.svelte';
  import PlotLegend from './PlotLegend.svelte';

  let { fig, width = 560, height = 420 } = $props();

  const ax = $derived(fig?.axes?.[0] ?? null);
  const cfg = $derived(ax?.config ?? {});
  const polar = $derived(!!cfg.polar);
  const hasLegend = $derived((ax?.legend ?? []).length > 0);

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
  <rect width="100%" height="100%" fill={T.plotBg} />
  {#if cfg.title}
    <text x={width / 2} y={16} font-size="13" text-anchor="middle"
          fill={T.plotText}>{cfg.title}</text>
  {/if}
  {#if ax}
    {#if polar}
      <PlotPolar {ax} {box} />
    {:else}
      <PlotCartesian {ax} {box} />
    {/if}
    {#if hasLegend}
      <PlotLegend labels={ax.legend} datasets={ax.datasets}
                  x={box.x + box.w - 110} y={box.y + 10} />
    {/if}
  {/if}
</svg>
