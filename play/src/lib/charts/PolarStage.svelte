<script lang="ts">
  // PolarStage (R3 — one stage per file): 30° spokes, r-rings with
  // labels; series via the shared LineChart (stage-agnostic).
  import type { WireAxes } from './wire.js';
  import type { PixelBox } from './stage.js';
  import { polarModel } from './stage.js';
  import { ticks } from './series.js';
  import LineChart from './LineChart.svelte';

  let { ax, box }: { ax: WireAxes | null; box: PixelBox } = $props();

  const pm = $derived(polarModel(ax, box));
  const spokes = Array.from({ length: 12 }, (_, i) => (i * Math.PI) / 6);
  const rTicks = $derived(
    ticks(0, pm.rMax, 5).filter((v) => v > 0 && v <= pm.rMax));
</script>

{#key pm.rMax}
  {#each spokes as th}
    <line x1={pm.cx} y1={pm.cy} x2={pm.X(th, pm.rMax)} y2={pm.Y(th, pm.rMax)}
          stroke="var(--nk-plot-grid)" stroke-width="1" />
  {/each}
  {#each rTicks as rt}
    <circle cx={pm.cx} cy={pm.cy} r={pm.R(rt)} fill="none" stroke="var(--nk-plot-grid)" />
    <text x={pm.cx + 3} y={pm.cy - pm.R(rt) - 2} font-size="9"
          fill="var(--nk-text-dim)">{rt}</text>
  {/each}
  <LineChart {ax} project={(x, y) => pm.X(x, y).toFixed(1) + ' ' + pm.Y(x, y).toFixed(1)} />
{/key}
