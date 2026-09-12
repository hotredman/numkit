<script>
  // PlotPolar (R3, one chart type per file): polar axes — 30° spokes,
  // r-rings with labels, series in polar coords. MATLAB semantics via
  // the shared model (R7).
  import { T } from '../tokens.js';
  import {
    seriesData, seriesColor, seriesDash, buildPath, polarModel, niceTicks,
  } from './plotModel.js';

  let { ax, box } = $props();

  const pm = $derived(polarModel(ax, box));
  const spokes = Array.from({ length: 12 }, (_, i) => (i * Math.PI) / 6);
  const rTicks = $derived(
    niceTicks(0, pm.rMax, 5).filter((v) => v > 0 && v <= pm.rMax));
</script>

{#key pm.rMax}
  {#each spokes as th}
    <line x1={pm.cx} y1={pm.cy} x2={pm.X(th, pm.rMax)} y2={pm.Y(th, pm.rMax)}
          stroke={T.plotGrid} stroke-width="1" />
  {/each}
  {#each rTicks as rt}
    <circle cx={pm.cx} cy={pm.cy} r={pm.R(rt)} fill="none" stroke={T.plotGrid} />
    <text x={pm.cx + 3} y={pm.cy - pm.R(rt) - 2} font-size="9"
          fill={T.textDim}>{rt}</text>
  {/each}
  {#each seriesData(ax) as ds, i}
    <path d={buildPath(ds.x ?? [], ds.y ?? [],
                       (x, y) => pm.X(x, y).toFixed(1) + ' ' + pm.Y(x, y).toFixed(1))}
          fill="none" stroke={seriesColor(ds.style, i)} stroke-width="1.6"
          stroke-dasharray={seriesDash(ds.style)} />
  {/each}
{/key}
