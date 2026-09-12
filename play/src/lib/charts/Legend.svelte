<script lang="ts">
  // Legend (R3): corner legend fed by the wire's legend labels.
  import { seriesColor } from './series.js';
  import type { WireDataset } from './wire.js';

  let {
    labels = [],
    datasets = [],
    x = 0,
    y = 0,
  }: {
    labels?: string[];
    datasets?: WireDataset[];
    x?: number;
    y?: number;
  } = $props();
</script>

{#if labels.length}
  <g transform="translate({x}, {y})">
    {#each labels as label, i}
      <g transform="translate(0, {i * 16})">
        <line x1="0" y1="0" x2="18" y2="0"
              stroke={seriesColor((datasets[i] as { style?: string })?.style, i)}
              stroke-width="1.6" />
        <text x="24" y="3.5" font-size="10" fill="var(--nk-plot-text)">{label}</text>
      </g>
    {/each}
  </g>
{/if}
