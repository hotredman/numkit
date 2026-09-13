<script lang="ts">
  // Three-column app frame (rule R1: pages get layout as a component,
  // never a grid of divs). Fixed controls sidebar + a DRAGGABLE split
  // between the editor and figures columns (SplitPane).
  import type { Snippet } from 'svelte';
  import SplitPane from './SplitPane.svelte';
  let {
    sidebar, editor, figures,
  }: { sidebar: Snippet; editor: Snippet; figures: Snippet } = $props();
</script>

<div class="shell">
  <div class="col side">{@render sidebar()}</div>
  <SplitPane>
    {#snippet first()}{@render editor()}{/snippet}
    {#snippet second()}{@render figures()}{/snippet}
  </SplitPane>
</div>

<style>
  .shell {
    display: grid;
    grid-template-columns: 260px 1fr;
    height: 100%;
  }
  /* min-width: 0 kills the implicit auto minimum of the grid track —
     without it long code lines inflate the editor and crush figures. */
  .side { min-width: 0; border-right: 1px solid var(--nk-border); background: var(--nk-bg-panel); overflow-y: auto; }
</style>
