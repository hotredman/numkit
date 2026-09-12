<script lang="ts">
  // FigureCard (R3): the white canvas card one figure renders into.
  import { T } from '../tokens.js';
  import type { Snippet } from 'svelte';
  let { children }: { children?: Snippet } = $props();
</script>

<div class="card">{#if children}{@render children()}{/if}</div>

<style>
  .card {
    background: var(--nk-plot-bg); border-radius: 8px; margin-bottom: 12px;
    overflow: hidden;
  }
</style>
