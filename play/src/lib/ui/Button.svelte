<script lang="ts">
  // Button (R3/R4): one variant prop, tokens inside.
  import { T } from '../tokens.js';
  import type { Snippet } from 'svelte';
  let {
    children, onclick, variant = 'default',
  }: { children?: Snippet; onclick?: () => void; variant?: string } = $props();
</script>

<button class={variant} {onclick}>{@render children?.()}</button>

<style>
  button {
    background: var(--nk-bg-hover); color: var(--nk-text);
    border: none; border-radius: 6px; padding: 4px 12px;
    cursor: pointer; font: inherit;
  }
  button:hover { filter: brightness(1.15); }
  .accent { background: var(--nk-bg-active); color: var(--nk-accent-soft); }
</style>
