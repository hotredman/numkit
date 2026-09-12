<script>
  // PlotLegend (R3): corner legend fed by the wire's legend labels.
  import { T, seriesColorOf } from './legendModel.js';
  let { labels = [], datasets = [], x, y } = $props();
</script>

{#if labels.length}
  <g transform="translate({x}, {y})">
    {#each labels as label, i}
      <g transform="translate(0, {i * 16})">
        <line x1="0" y1="0" x2="18" y2="0"
              stroke={seriesColorOf(datasets?.[i]?.style, i)} stroke-width="1.6" />
        <text x="24" y="3.5" font-size="10" fill={T.plotText}>{label}</text>
      </g>
    {/each}
  </g>
{/if}
