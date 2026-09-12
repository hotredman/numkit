<script>
  // App shell — the three-column layout (user spec):
  //   sidebar (controls from the @play manifest) | editor | figures.
  // Reactive loop: slider → playSetParam* → playEvalDirty → the
  // returned wire updates the plot panes. All engine math is wasm-side.
  let { gallery = [] } = $props();

  let engine = null;
  let manifest = null;         // parsed @play JSON
  let controls = $state([]);   // [{var,label,min,max,step,default,type,options,unit,value}]
  let editorText = $state('');
  let figures = $state([]);    // [{id, axes:[...]}, ...] from the wire
  let status = $state('');     // load errors / lastError
  let activeId = $state(null); // selected gallery entry

  const CONTROL_TYPES = {
    slider: (c) => ({
      kind: 'slider', var: c.var, label: c.label ?? c.var,
      min: c.min ?? 0, max: c.max ?? 100, step: c.step ?? 1,
      unit: c.unit ?? '', value: c.default ?? c.min ?? 0,
    }),
    select: (c) => ({
      kind: 'select', var: c.var, label: c.label ?? c.var,
      options: c.options ?? [], value: c.default ?? (c.options?.[0] ?? ''),
    }),
  };

  function flattenControls(manifestJson) {
    if (!manifestJson) return [];
    const m = typeof manifestJson === 'string'
      ? JSON.parse(manifestJson) : manifestJson;
    const out = [];
    for (const g of m.groups ?? [])
      for (const c of g.controls ?? []) {
        const build = c.type === 'select' ? CONTROL_TYPES.select
                                        : CONTROL_TYPES.slider;
        out.push(build(c));
      }
    return out;
  }

  export async function loadScript(source, id = null) {
    engine ??= await getEngine();
    engine.repl_init?.();
    const raw = engine.playLoadScript(source);
    activeId = id;
    editorText = source;
    if (raw.startsWith('{"__error"')) {
      status = 'ошибка загрузки: ' + JSON.parse(raw).message;
      manifest = null; controls = []; figures = [];
      return;
    }
    status = '';
    manifest = raw || null;
    controls = flattenControls(raw);
    // Initial run: figures already emitted during load — drain.
    const wire = engine.playEvalDirty();
    applyWire(wire);
  }

  function applyWire(text) {
    const { figures: figs } = parseFigureWire(text ?? '');
    if (!figs.length) return;
    figures = figs;
  }

  async function onControl(ctrl) {
    if (ctrl.kind === 'slider')
      engine.playSetParamDouble(ctrl.var, Number(ctrl.value));
    else
      engine.playSetParamString(ctrl.var, String(ctrl.value));
    applyWire(engine.playEvalDirty());
    const err = engine.playLastError?.() ?? '';
    if (err) status = err;
    else status = '';
  }

  async function runFromEditor() {
    await loadScript(editorText, activeId);
  }
</script>

<div class="play-app">
  <aside class="sidebar">
    <h2>nk play</h2>
    {#if gallery.length}
      <ul class="gallery">
        {#each gallery as item (item.id)}
          <li>
            <button
              class:active={activeId === item.id}
              onclick={() => loadScript(item.source, item.id)}>
              {item.title}
            </button>
          </li>
        {/each}
      </ul>
    {/if}
    {#if controls.length}
      <div class="controls">
        <h3>Параметры</h3>
        {#each controls as c (c.var)}
          <label class="control">
            <span class="ctl-label">{c.label}</span>
            {#if c.kind === 'slider'}
              <input type="range" min={c.min} max={c.max} step={c.step}
                     bind:value={c.value}
                     oninput={() => onControl(c)} />
              <span class="ctl-value">{c.value}{c.unit}</span>
            {:else}
              <select bind:value={c.value} onchange={() => onControl(c)}>
                {#each c.options as opt}
                  <option value={opt}>{opt}</option>
                {/each}
              </select>
            {/if}
          </label>
        {/each}
      </div>
    {:else if activeId}
      <p class="no-controls">манифест @play не найден</p>
    {/if}
  </aside>

  <section class="editor-pane">
    <div class="pane-head">
      <span>редактор</span>
      <button class="run" onclick={runFromEditor}>▶ запустить</button>
    </div>
    <textarea class="code" spellcheck="false" bind:value={editorText}></textarea>
    {#if status}
      <div class="status">{status}</div>
    {/if}
  </section>

  <section class="figures-pane">
    <div class="pane-head"><span>фигуры</span></div>
    <div class="figures">
      {#if figures.length}
        {#each figures as fig (fig.id)}
          <div class="figure-card">
            <PlotFigure {fig} />
          </div>
        {/each}
      {:else}
        <div class="empty">нет фигур</div>
      {/if}
    </div>
  </section>
</div>

<style>
  .play-app {
    display: grid;
    grid-template-columns: 260px 1fr 1.2fr;
    height: 100%;
  }
  .sidebar {
    border-right: 1px solid #262a32;
    padding: 12px;
    overflow-y: auto;
    background: #191c22;
  }
  .sidebar h2 { margin: 0 0 10px; font-size: 15px; color: #7fd99a; }
  .gallery { list-style: none; margin: 0 0 14px; padding: 0; }
  .gallery button {
    display: block; width: 100%; text-align: left;
    background: none; border: none; color: #c8cdd8;
    padding: 5px 8px; border-radius: 6px; cursor: pointer;
  }
  .gallery button:hover { background: #232732; }
  .gallery button.active { background: #243b2f; color: #a4e3b8; }
  .controls h3 { margin: 4px 0 8px; font-size: 12px; color: #8b93a3; }
  .control { display: block; margin-bottom: 10px; }
  .ctl-label { display: block; font-size: 12px; color: #aab2c0; }
  .control input[type='range'] { width: 100%; }
  .ctl-value { font-size: 12px; color: #7fd99a; }
  .control select {
    width: 100%; background: #232732; color: #e8eaf0;
    border: 1px solid #2f3542; border-radius: 4px; padding: 4px;
  }
  .no-controls { color: #6b7280; font-size: 12px; }

  .editor-pane {
    border-right: 1px solid #262a32;
    display: flex; flex-direction: column;
  }
  .pane-head {
    display: flex; justify-content: space-between; align-items: center;
    padding: 6px 12px; border-bottom: 1px solid #262a32;
    color: #8b93a3; font-size: 12px; text-transform: uppercase;
    letter-spacing: 0.06em;
  }
  .run {
    background: #243b2f; color: #a4e3b8; border: none;
    border-radius: 6px; padding: 4px 12px; cursor: pointer;
  }
  .code {
    flex: 1; width: 100%; resize: none; border: none; outline: none;
    background: #14161a; color: #dfe3ec; padding: 12px;
    font: 12.5px/1.5 'JetBrains Mono', 'Cascadia Code', monospace;
  }
  .status {
    padding: 6px 12px; color: #e26a6a; background: #2a1d1d;
    border-top: 1px solid #3a2626; font-size: 12px;
  }
  .figures-pane { display: flex; flex-direction: column; }
  .figures { flex: 1; overflow-y: auto; padding: 10px; }
  .figure-card {
    background: #fff; border-radius: 8px; margin-bottom: 12px;
    overflow: hidden;
  }
  .empty {
    display: grid; place-items: center; height: 100%;
    color: #555c68; font-size: 13px;
  }
</style>
