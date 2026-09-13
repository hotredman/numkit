<script lang="ts">
  // PageShell (R1/R3): the page frame every screen shares — a topbar
  // row above a scrolling content region. Pages never build this.
  import type { Snippet } from 'svelte';
  let {
    topbar, children,
  }: { topbar?: Snippet; children?: Snippet } = $props();
</script>

<div class="page">
  {#if topbar}<header class="top">{@render topbar()}</header>{/if}
  <main class="content">{#if children}{@render children()}{/if}</main>
</div>

<style>
  .page { display: flex; flex-direction: column; height: 100%; }
  .top {
    flex: none;
    border-bottom: 1px solid var(--nk-border);
    background: var(--nk-bg-panel);
  }
  .content { flex: 1; min-height: 0; overflow-y: auto; }
</style>
