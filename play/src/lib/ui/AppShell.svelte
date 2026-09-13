<script lang="ts">
  // Three-column app frame (rule R1: pages get layout as a component,
  // never a grid of divs).
  import type { Snippet } from 'svelte';
  import { T } from '../tokens.js';
  let {
    sidebar, editor, figures,
  }: { sidebar: Snippet; editor: Snippet; figures: Snippet } = $props();
</script>

<div class="shell">
  <div class="col side">{@render sidebar()}</div>
  <div class="col mid">{@render editor()}</div>
  <div class="col figs">{@render figures()}</div>
</div>

<style>
  .shell {
    display: grid;
    grid-template-columns: 260px 1fr 1.2fr;
    height: 100%;
  }
  /* min-width: 0 kills the implicit auto minimum of the grid tracks
     (min-content = the longest unwrapped code line): without it a long
     line inflates the editor column and squeezes the figures column.
     CodeMirror's own horizontal scroller takes over instead. */
  .side { min-width: 0; border-right: 1px solid var(--nk-border); background: var(--nk-bg-panel); overflow-y: auto; }
  .mid { min-width: 0; border-right: 1px solid var(--nk-border); display: flex; flex-direction: column; }
  .figs { min-width: 0; display: flex; flex-direction: column; }
</style>
