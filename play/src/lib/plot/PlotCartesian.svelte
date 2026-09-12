<script>
  // PlotCartesian (R3, one chart type per file): frame + grid + ticks
  // + labels + line series. MATLAB-parity semantics from the wire
  // (10-interval tick ladder, labels 1.1×, axesLineWidth) via the
  // shared model (R7).
  import { T } from '../tokens.js';
  import {
    viewport, seriesData, seriesColor, seriesDash, buildPath, niceTicks,
  } from './plotModel.js';

  let { ax, box } = $props();
  const cfg = $derived(ax?.config ?? {});
  const vp = $derived(viewport(ax));

  const X = $derived((v) => box.x + ((v - vp.x0) / (vp.x1 - vp.x0)) * box.w);
  const Y = $derived((v) => box.y + box.h - ((v - vp.y0) / (vp.y1 - vp.y0)) * box.h);
  const xt = $derived(cfg.xticks?.length ? cfg.xticks : niceTicks(vp.x0, vp.x1, 10));
  const yt = $derived(cfg.yticks?.length ? cfg.yticks : niceTicks(vp.y0, vp.y1, 10));
</script>

{#if cfg.grid === 'on'}
  {#each xt as v}
    <line x1={X(v)} y1={box.y} x2={X(v)} y2={box.y + box.h} stroke={T.plotGrid} />
  {/each}
  {#each yt as v}
    <line x1={box.x} y1={Y(v)} x2={box.x + box.w} y2={Y(v)} stroke={T.plotGrid} />
  {/each}
{/if}
<rect x={box.x} y={box.y} width={box.w} height={box.h} fill="none"
      stroke={T.plotFrame} stroke-width={cfg.axesLineWidth ?? 0.5} />
{#each xt as v, i}
  <line x1={X(v)} y1={box.y + box.h} x2={X(v)} y2={box.y + box.h + 4}
        stroke={T.plotFrame} stroke-width="0.5" />
  <text x={X(v)} y={box.y + box.h + 15} font-size="10" text-anchor="middle"
        fill={T.plotText}>{cfg.xticklabels?.[i] ?? v}</text>
{/each}
{#each yt as v}
  <line x1={box.x - 4} y1={Y(v)} x2={box.x} y2={Y(v)}
        stroke={T.plotFrame} stroke-width="0.5" />
  <text x={box.x - 7} y={Y(v) + 3} font-size="10" text-anchor="end"
        fill={T.plotText}>{v}</text>
{/each}
{#if cfg.xlabel}
  <text x={box.x + box.w / 2} y={box.y + box.h + 34} font-size="11"
        text-anchor="middle" fill={T.plotText}>{cfg.xlabel}</text>
{/if}
{#if cfg.ylabel}
  <text x={box.x - 46} y={box.y + box.h / 2} font-size="11"
        text-anchor="middle" fill={T.plotText}
        transform="rotate(-90 {box.x - 46} {box.y + box.h / 2})">{cfg.ylabel}</text>
{/if}
{#each seriesData(ax) as ds, i}
  <path d={buildPath(ds.x ?? [], ds.y ?? [],
                     (x, y) => X(x).toFixed(1) + ' ' + Y(y).toFixed(1))}
        fill="none" stroke={seriesColor(ds.style, i)} stroke-width="1.6"
        stroke-dasharray={seriesDash(ds.style)} />
{/each}
