<script lang="ts">
  // LineChart (R3 — one CHART per file): renders the wire's line
  // series against ANY projection the stage hands in. Cartesian and
  // polar stages share this one component — the D1 duplication this
  // split exists to remove.
  import type { WireAxes } from './wire.js';
  import { lineSeries, seriesColor, seriesDash, buildPath } from './series.js';

  let { ax, project }: { ax: WireAxes | null; project: (x: number, y: number) => string } = $props();
</script>

{#each lineSeries(ax) as ds, i}
  <path d={buildPath(ds.x ?? [], ds.y ?? [], project)}
        fill="none" stroke={seriesColor(ds.style, i)} stroke-width="1.6"
        stroke-dasharray={seriesDash(ds.style)} />
{/each}
