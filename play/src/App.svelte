<script lang="ts">
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
  import Figure from './lib/charts/Figure.svelte';
  import { parseFigureWire, flattenControls } from './lib/charts/wire.js';
  import { getEngine } from './engine.js';
  import type { PlayEngine } from './engine.js';
  import type { WireFigure, PlayControl, PlayControlGroup } from './lib/charts/wire.js';

  interface GalleryEntry { id: string; title: string; description: string; source: string; }
  let { gallery = [] }: { gallery?: GalleryEntry[] } = $props();

  let engine: PlayEngine | null = null;
  let groups = $state<PlayControlGroup[]>([]);
  let editorText = $state('');
  let wireFigures = $state<WireFigure[]>([]);
  let status = $state<string>('');
  let activeId = $state<string | null>(null);

  export async function loadScript(source: string, id: string | null = null) {
    engine ??= await getEngine();
    engine.repl_init();
    const raw: string = engine.playLoadScript(source);
    activeId = id;
    editorText = source;
    if (raw.startsWith('{"__error"')) {
      status = 'ошибка загрузки: ' + JSON.parse(raw).message;
      groups = []; wireFigures = [];
      return;
    }
    status = '';
    groups = flattenControls(raw || null);
    applyWire(engine.playEvalDirty());
  }

  function applyWire(text: string) {
    const { figures: figs } = parseFigureWire(text);
    if (figs.length) wireFigures = figs;
  }

  function onControl(c: PlayControl) {
    if (!engine) return;
    if (c.kind === 'slider') engine.playSetParamDouble(c.var, c.value);
    else engine.playSetParamString(c.var, c.value);
    applyWire(engine.playEvalDirty());
    status = engine.playLastError();
  }
</script>

<AppShell>
  {#snippet sidebar()}
    <GalleryList items={gallery} {activeId}
                 onselect={(item: GalleryEntry) => loadScript(item.source, item.id)} />
    {#each groups as g (g.name)}
      <ControlGroup name={g.name}>
        {#each g.controls as c (c.var)}
          {#if c.kind === 'slider'}
            <SliderControl label={c.label} min={c.min} max={c.max}
                           step={c.step} unit={c.unit} value={c.value}
                           oninput={(v: number) => { (c as { value: number }).value = v; onControl(c); }} />
          {:else}
            <SelectControl label={c.label} options={c.options} value={c.value}
                           onchange={(v: string) => { (c as { value: string }).value = v; onControl(c); }} />
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
                  oninput={(v: string) => (editorText = v)} />
      <StatusBar text={status} />
    </Panel>
  {/snippet}

  {#snippet figures()}
    <Panel title="фигуры">
      {#if wireFigures.length}
        {#each wireFigures as fig (fig.id)}
          <FigureCard><Figure {fig} /></FigureCard>
        {/each}
      {/if}
    </Panel>
  {/snippet}
</AppShell>
