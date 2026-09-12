<script>
  // PAGE (rules R1/R2 apply HERE): composition only — every visual is
  // a lib/ component; zero raw div/style in this file. The reactive
  // loop: control → playSetParam* → playEvalDirty → wire → figures.
  import AppShell from './lib/ui/AppShell.svelte';
  import Panel from './lib/ui/Panel.svelte';
  import Button from './lib/ui/Button.svelte';
  import GalleryList from './lib/ui/GalleryList.svelte';
  import ControlGroup from './lib/ui/ControlGroup.svelte';
  import SliderControl from './lib/ui/SliderControl.svelte';
  import SelectControl from './lib/ui/SelectControl.svelte';
  import CodeEditor from './lib/ui/CodeEditor.svelte';
  import StatusBar from './lib/ui/StatusBar.svelte';
  import FigureCard from './lib/ui/FigureCard.svelte';
  import Figure from './lib/plot/Figure.svelte';
  import { parseFigureWire, flattenControls } from './lib/plot/wire.js';
  import { getEngine } from './engine.js';

  let { gallery = [] } = $props();

  let engine = null;
  let groups = $state([]);        // [{name, controls:[…]}]
  let editorText = $state('');
  let figures = $state([]);
  let status = $state('');
  let activeId = $state(null);

  export async function loadScript(source, id = null) {
    engine ??= await getEngine();
    engine.repl_init?.();
    const raw = engine.playLoadScript(source);
    activeId = id;
    editorText = source;
    if (raw.startsWith('{"__error"')) {
      status = 'ошибка загрузки: ' + JSON.parse(raw).message;
      groups = []; figures = [];
      return;
    }
    status = '';
    groups = flattenControls(raw || null);
    applyWire(engine.playEvalDirty());
  }

  function applyWire(text) {
    const { figures: figs } = parseFigureWire(text);
    if (figs.length) figures = figs;
  }

  function onControl(c) {
    if (c.kind === 'slider') engine.playSetParamDouble(c.var, Number(c.value));
    else engine.playSetParamString(c.var, String(c.value));
    applyWire(engine.playEvalDirty());
    status = engine.playLastError?.() ?? '';
  }
</script>

<AppShell>
  {#snippet sidebar()}
    <GalleryList items={gallery} {activeId}
                 onselect={(item) => loadScript(item.source, item.id)} />
    {#each groups as g (g.name)}
      <ControlGroup name={g.name}>
        {#each g.controls as c (c.var)}
          {#if c.kind === 'slider'}
            <SliderControl label={c.label} min={c.min} max={c.max}
                           step={c.step} unit={c.unit} value={c.value}
                           oninput={(v) => { c.value = v; onControl(c); }} />
          {:else}
            <SelectControl label={c.label} options={c.options} value={c.value}
                           onchange={(v) => { c.value = v; onControl(c); }} />
          {/if}
        {/each}
      </ControlGroup>
    {/each}
  {/snippet}

  {#snippet editor()}
    <Panel title="редактор">
      {#snippet actions()}
        <Button variant="accent" onclick={() => loadScript(editorText, activeId)}>
          ▶ запустить
        </Button>
      {/snippet}
      <CodeEditor value={editorText}
                  oninput={(v) => (editorText = v)} />
      <StatusBar text={status} />
    </Panel>
  {/snippet}

  {#snippet figures()}
    <Panel title="фигуры">
      {#if figures.length}
        {#each figures as fig (fig.id)}
          <FigureCard><Figure {fig} /></FigureCard>
        {/each}
      {/if}
    </Panel>
  {/snippet}
</AppShell>
