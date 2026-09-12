<script lang="ts">
  // CartesianStage (R3 — one stage per file): frame, grid, ticks,
  // labels — the COORDINATE SYSTEM. Charts (LineChart, …) render on
  // top via the shared projection. MATLAB semantics from the wire
  // (10-interval ladder, labels 1.1×, axesLineWidth).
  import type { WireAxes } from './wire.js';
  import type { PixelBox } from './stage.js';
  import { viewport, cartesianProjection } from './stage.js';
  import { ticks } from './series.js';
  import LineChart from './LineChart.svelte';

  let { ax, box }: { ax: WireAxes | null; box: PixelBox } = $props();

  const cfg = $derived(ax?.config ?? {});
  const vp = $derived(viewport(ax));
  const { X, Y } = $derived(cartesianProjection(vp, box));
  const xt = $derived<number[]>(((cfg.xticks as number[])?.length ?? 0) > 0 ? (cfg.xticks as number[]) : ticks(vp.x0, vp.x1, 10));
  const yt = $derived<number[]>(((cfg.yticks as number[])?.length ?? 0) > 0 ? (cfg.yticks as number[]) : ticks(vp.y0, vp.y1, 10));
</script>

{#if cfg.grid === 'on'}
  {#each xt as v}
    <line x1={X(v)} y1={box.y} x2={X(v)} y2={box.y + box.h} stroke="var(--nk-plot-grid)" />
  {/each}
  {#each yt as v}
    <line x1={box.x} y1={Y(v)} x2={box.x + box.w} y2={Y(v)} stroke="var(--nk-plot-grid)" />
  {/each}
{/if}
<rect x={box.x} y={box.y} width={box.w} height={box.h} fill="none"
      stroke="var(--nk-plot-frame)" stroke-width={(cfg.axesLineWidth as number) ?? 0.5} />
{#each xt as v, i}
  <line x1={X(v)} y1={box.y + box.h} x2={X(v)} y2={box.y + box.h + 4}
        stroke="var(--nk-plot-frame)" stroke-width="0.5" />
  <text x={X(v)} y={box.y + box.h + 15} font-size="10" text-anchor="middle"
        fill="var(--nk-plot-text)">{(cfg.xticklabels as string[] | undefined)?.[i] ?? v}</text>
{/each}
{#each yt as v}
  <line x1={box.x - 4} y1={Y(v)} x2={box.x} y2={Y(v)}
        stroke="var(--nk-plot-frame)" stroke-width="0.5" />
  <text x={box.x - 7} y={Y(v) + 3} font-size="10" text-anchor="end"
        fill="var(--nk-plot-text)">{v}</text>
{/each}
{#if cfg.xlabel}
  <text x={box.x + box.w / 2} y={box.y + box.h + 34} font-size="11"
        text-anchor="middle" fill="var(--nk-plot-text)">{cfg.xlabel as string}</text>
{/if}
{#if cfg.ylabel}
  <text x={box.x - 46} y={box.y + box.h / 2} font-size="11"
        text-anchor="middle" fill="var(--nk-plot-text)"
        transform="rotate(-90 {box.x - 46} {box.y + box.h / 2})">{cfg.ylabel as string}</text>
{/if}
<LineChart {ax} project={(x, y) => X(x).toFixed(1) + ' ' + Y(y).toFixed(1)} />
