<script lang="ts">
  // PAGE (R1/R2): composition only. The three-column script screen —
  // controls (from the @play manifest) | MATLAB editor | figures. The
  // reactive loop: control → playSetParam* → playEvalDirty → wire.
  import { onMount } from 'svelte';
  import PageShell from '../lib/ui/PageShell.svelte';
  import TopBar from '../lib/ui/TopBar.svelte';
  import AppShell from '../lib/ui/AppShell.svelte';
  import Panel from '../lib/ui/Panel.svelte';
  import Button from '../lib/ui/Button.svelte';
  import ControlGroup from '../lib/ui/ControlGroup.svelte';
  import SliderControl from '../lib/ui/SliderControl.svelte';
  import SelectControl from '../lib/ui/SelectControl.svelte';
  import StatusBar from '../lib/ui/StatusBar.svelte';
  import FigureCard from '../lib/ui/FigureCard.svelte';
  import MatlabEditor from '../lib/editor/MatlabEditor.svelte';
  import Figure from '../lib/charts/Figure.svelte';
  import { mergeWireState, flattenControls } from '../lib/charts/wire.js';
  import { getEngine } from '../engine.js';
  import type { PlayEngine } from '../engine.js';
  import type {
    WireFigure, PlayControl, PlayControlGroup,
  } from '../lib/charts/wire.js';
  import {
    categoryName, categoryOfEntry, type GalleryEntry,
  } from '../lib/galleryModel.js';
  import { hrefFor } from '../lib/router.js';

  let { entry }: { entry: GalleryEntry } = $props();

  let engine: PlayEngine | null = null;
  let groups = $state<PlayControlGroup[]>([]);
  let editorText = $state('');
  let wireFigures = $state<WireFigure[]>([]);
  let status = $state('');

  const crumbs = $derived([
    { label: 'Gallery', href: '#/' },
    { label: categoryName(categoryOfEntry(entry.id)),
      href: hrefFor({ page: 'category', id: categoryOfEntry(entry.id) }) },
    { label: entry.title },
  ]);

  async function run(source: string) {
    engine ??= await getEngine();
    engine.repl_init();
    const raw: string = engine.playLoadScript(source);
    editorText = source;
    if (raw.startsWith('{"__error"')) {
      status = 'load error: ' + JSON.parse(raw).message;
      groups = []; wireFigures = [];
      return;
    }
    status = '';
    groups = flattenControls(raw || null);
    applyWire(engine.playEvalDirty());
  }

  function applyWire(text: string) {
    wireFigures = mergeWireState(wireFigures, text);
  }

  function onControl(c: PlayControl) {
    if (!engine) return;
    if (c.kind === 'slider') engine.playSetParamDouble(c.var, c.value);
    else engine.playSetParamString(c.var, c.value);
    applyWire(engine.playEvalDirty());
    status = engine.playLastError();
  }

  onMount(() => run(entry.source));
</script>

<PageShell>
  {#snippet topbar()}
    <TopBar crumbs={crumbs} />
  {/snippet}

  <AppShell>
    {#snippet sidebar()}
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
      <Panel title="editor">
        {#snippet actions()}
          <Button variant="accent" onclick={() => run(editorText)}>▶ Run</Button>
          <Button onclick={() => run(entry.source)}>↺ Reset</Button>
        {/snippet}
        <MatlabEditor value={editorText}
                      oninput={(v: string) => (editorText = v)} />
        <StatusBar text={status} />
      </Panel>
    {/snippet}

    {#snippet figures()}
      <Panel title="figures">
        {#if wireFigures.length}
          {#each wireFigures as fig (fig.id)}
            <FigureCard><Figure {fig} /></FigureCard>
          {/each}
        {/if}
      </Panel>
    {/snippet}
  </AppShell>
</PageShell>
