<script>
  // Svelte figure renderer (portion D1) — the SUCCESSOR of the IDE's
  // React CompositePlot. Consumes the IDENTICAL engine wire JSON
  // (__FIGURE_DATA__), shares the framework-free helpers (@ide-plot:
  // MATLAB-exact tick ladders, colormap tables). Coverage D1:
  // 2-D cartesian line series + polarplot — the beamforming gallery
  // class. Frame, ticks, labels, title, legend, grid: MATLAB-parity
  // semantics per the engine wire (fontSize 1.1× labels, 10-interval
  // ladder, reverse-YDir for images — later portions).
  import { niceTicks } from '@ide-plot/plotTicks.js';

  let { fig } = $props();

  // Single-axes v1 (gallery scripts plot into one figure).
  const ax = $derived(fig?.axes?.[0] ?? null);
  const cfg = $derived(ax?.config ?? {});
  const polar = $derived(!!cfg.polar);

  const W = 560, H = 420;

  // Cartesian margins (MATLAB-ish: room for tick labels + axis labels).
  const padL = $derived(56 + (cfg.ylabel ? 8 : 0));
  const padT = $derived(24 + (cfg.title ? 16 : 0));
  const padR = 14;
  const padB = $derived(42 + (cfg.xlabel ? 8 : 0));
  const pw = $derived(W - padL - padR);
  const ph = $derived(H - padT - padB);

  // Viewport: the wire carries engine-side lims; fall back to data.
  const lims = $derived.by(() => {
    let [x0, x1] = cfg.xlim ?? [Infinity, -Infinity];
    let [y0, y1] = cfg.ylim ?? [Infinity, -Infinity];
    for (const ds of ax?.datasets ?? []) {
      const xs = ds.x ?? [], ys = ds.y ?? [];
      if (ds.type === 'imagesc' || ds.type === 'text') continue;
      for (let k = 0; k < Math.min(xs.length, ys.length); k++)
        if (Number.isFinite(xs[k]) && Number.isFinite(ys[k])) {
          if (!cfg.xlim) { x0 = Math.min(x0, xs[k]); x1 = Math.max(x1, xs[k]); }
          if (!cfg.ylim) { y0 = Math.min(y0, ys[k]); y1 = Math.max(y1, ys[k]); }
        }
    }
    if (!(x1 > x0)) { x0 = 0; x1 = 1; }
    if (!(y1 > y0)) { y0 = y0 - 0.5; y1 = y0 + 1.5; y0 -= 0.5; }
    const pad = (hi, lo) => (hi - lo) * 0.05;
    if (!cfg.xlim) { x0 -= pad(x1, x0); x1 += pad(x1, x0); }
    if (!cfg.ylim) { y0 -= pad(y1, y0); y1 += pad(y1, y0); }
    return { x0, x1, y0, y1 };
  });

  const X = $derived((v) => padL + ((v - lims.x0) / (lims.x1 - lims.x0)) * pw);
  const Y = $derived((v) => padT + ph - ((v - lims.y0) / (lims.y1 - lims.y0)) * ph);

  const xt = $derived(cfg.xticks?.length ? cfg.xticks : niceTicks(lims.x0, lims.x1, 10));
  const yt = $derived(cfg.yticks?.length ? cfg.yticks : niceTicks(lims.y0, lims.y1, 10));

  // Line series with style parse (r--o dialect) — shared adapter math
  // inlined minimal for D1 (the adapters.linespec.js is importable
  // when richer dialects land).
  function seriesColor(style, idx) {
    if (typeof style === 'string' && style.startsWith('#')) return style;
    const map = { r: '#f07070', g: '#6ee7a0', b: '#60d0f0', c: '#60d0f0',
                  m: '#e070c0', y: '#e8d060', k: '#282830', w: '#ffffff' };
    if (typeof style === 'string' && map[style[0]]) return map[style[0]];
    return ['#7fd99a', '#5fb3d4', '#e9b870', '#9b8cf2', '#e26a6a'][idx % 5];
  }
  function seriesDash(style) {
    if (typeof style !== 'string') return '';
    if (style.includes('--')) return '6 4';
    if (style.includes(':')) return '2 3';
    if (style.includes('-.')) return '8 3 2 3';
    return '';
  }
  function toPath(ds) {
    const xs = ds.x ?? [], ys = ds.y ?? [];
    let d = '';
    let pen = false;
    for (let k = 0; k < Math.min(xs.length, ys.length); k++) {
      if (!Number.isFinite(xs[k]) || !Number.isFinite(ys[k])) { pen = false; continue; }
      d += (pen ? 'L' : 'M') + X(xs[k]).toFixed(1) + ' ' + Y(ys[k]).toFixed(1);
      pen = true;
    }
    return d;
  }

  // Polar: theta(rad)/r grid at 30° spokes + r-rings; series in polar
  // coords map straight onto the cartesian canvas centre.
  const cx = $derived(padL + pw / 2);
  const cy = $derived(padT + ph / 2);
  const rMax = $derived.by(() => {
    let rm = 0;
    for (const ds of ax?.datasets ?? []) {
      const ys = ds.y ?? [];
      for (const v of ys) if (Number.isFinite(v)) rm = Math.max(rm, Math.abs(v));
    }
    return rm > 0 ? rm : 1;
  });
  const R = $derived((r) => (Math.abs(r) / rMax) * (Math.min(pw, ph) / 2 - 6));
  const PX = $derived((th, r) => cx + R(r) * Math.cos(th));
  const PY = $derived((th, r) => cy - R(r) * Math.sin(th));
  function polarPath(ds) {
    const xs = ds.x ?? [], ys = ds.y ?? [];
    let d = '';
    let pen = false;
    for (let k = 0; k < Math.min(xs.length, ys.length); k++) {
      if (!Number.isFinite(xs[k]) || !Number.isFinite(ys[k])) { pen = false; continue; }
      d += (pen ? 'L' : 'M') + PX(xs[k], ys[k]).toFixed(1) + ' ' + PY(xs[k], ys[k]).toFixed(1);
      pen = true;
    }
    return d;
  }
  const rTicks = $derived(niceTicks(0, rMax, 5).filter((v) => v > 0 && v <= rMax));

  const legendLabels = $derived(ax?.legend ?? []);
</script>

<svg viewBox="0 0 {W} {H}" width="100%" xmlns="http://www.w3.org/2000/svg">
  <rect width="100%" height="100%" fill="#ffffff" />

  {#if polar}
    <!-- polar grid: 30° spokes + r rings -->
    {#each Array.from({ length: 12 }, (_, i) => (i * Math.PI) / 6) as th}
      <line x1={cx} y1={cy} x2={PX(th, rMax)} y2={PY(th, rMax)}
            stroke="#d8d8dc" stroke-width="1" />
    {/each}
    {#each rTicks as rt}
      <circle cx={cx} cy={cy} r={R(rt)} fill="none" stroke="#d8d8dc" />
      <text x={cx + 3} y={cy - R(rt) - 2} font-size="9" fill="#8b93a3">{rt}</text>
    {/each}
    {:else}
    <!-- cartesian grid (behind data, MATLAB: off unless cfg.grid) -->
    {#if cfg.grid === 'on'}
      {#each xt as v}
        <line x1={X(v)} y1={padT} x2={X(v)} y2={padT + ph} stroke="#e6e6ea" />
      {/each}
      {#each yt as v}
        <line x1={padL} y1={Y(v)} x2={padL + pw} y2={Y(v)} stroke="#e6e6ea" />
      {/each}
    {/if}
    <!-- frame (axesLineWidth from the wire, default 0.5) -->
    <rect x={padL} y={padT} width={pw} height={ph} fill="none"
          stroke="#404044" stroke-width={cfg.axesLineWidth ?? 0.5} />
    <!-- ticks -->
    {#each xt as v}
      <line x1={X(v)} y1={padT + ph} x2={X(v)} y2={padT + ph + 4}
            stroke="#404044" stroke-width="0.5" />
      <text x={X(v)} y={padT + ph + 15} font-size="10" text-anchor="middle"
            fill="#282830">{cfg.xticklabels?.[xt.indexOf(v)] ?? v}</text>
    {/each}
    {#each yt as v}
      <line x1={padL - 4} y1={Y(v)} x2={padL} y2={Y(v)}
            stroke="#404044" stroke-width="0.5" />
      <text x={padL - 7} y={Y(v) + 3} font-size="10" text-anchor="end"
            fill="#282830">{v}</text>
    {/each}
    <!-- axis labels + title (labels 1.1× tick size, MATLAB probe) -->
    {#if cfg.xlabel}
      <text x={padL + pw / 2} y={H - 8} font-size="11" text-anchor="middle"
            fill="#282830">{cfg.xlabel}</text>
    {/if}
    {#if cfg.ylabel}
      <text x={14} y={padT + ph / 2} font-size="11" text-anchor="middle"
            fill="#282830" transform="rotate(-90 14 {padT + ph / 2})">{cfg.ylabel}</text>
    {/if}
  {/if}
  {#if cfg.title}
    <text x={W / 2} y={16} font-size="13" text-anchor="middle"
          fill="#282830">{cfg.title}</text>
  {/if}

  <!-- series -->
  {#each ax?.datasets ?? [] as ds, i}
    {#if ds.visible === false}
      {:else if ds.type === 'line' || ds.type === undefined}
      <path d={polar ? polarPath(ds) : toPath(ds)} fill="none"
            stroke={seriesColor(ds.style, i)}
            stroke-width="1.6"
            stroke-dasharray={seriesDash(ds.style)} />
    {/if}
  {/each}

  <!-- legend (simple MATLAB corner box, D1) -->
  {#if legendLabels.length}
    {#each legendLabels as label, i}
      <g transform="translate({padL + pw - 110}, {padT + 10 + i * 16})">
        <line x1="0" y1="0" x2="18" y2="0"
              stroke={seriesColor(ax.datasets?.[i]?.style, i)} stroke-width="1.6" />
        <text x="24" y="3.5" font-size="10" fill="#282830">{label}</text>
      </g>
    {/each}
  {/if}
</svg>
