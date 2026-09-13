<script lang="ts">
  // CardGrid (R1/R3): responsive card layout for catalog pages —
  // pages never write a CSS grid themselves. Optional `header` renders
  // above the grid (the catalog's search row).
  import type { Snippet } from 'svelte';
  let { header, children }: { header?: Snippet; children?: Snippet } = $props();
</script>

<div class="wrap">
  {#if header}<div class="head">{@render header()}</div>{/if}
  <div class="grid">{#if children}{@render children()}{/if}</div>
</div>

<style>
  .wrap { display: flex; flex-direction: column; }
  .head { padding: 16px 16px 0; max-width: 480px; }
  .grid {
    display: grid; gap: 12px; padding: 16px;
    grid-template-columns: repeat(auto-fill, minmax(220px, 1fr));
    align-content: start;
  }
</style>
