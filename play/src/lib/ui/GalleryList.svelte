<script lang="ts">
  // Gallery index (R3): the example list, one entry per @play script.
  import { T } from '../tokens.js';
  interface Props {
  items: any;
  activeId: any;
  onselect: any;
}
let { items = [], activeId = null, onselect }: Props = $props();
</script>

<nav class="gallery">
  <h2 class="brand">nk play</h2>
  <ul>
    {#each items as item (item.id)}
      <li>
        <button
          class:active={activeId === item.id}
          onclick={() => onselect?.(item)}
          title={item.description}>
          {item.title}
        </button>
      </li>
    {/each}
  </ul>
</nav>

<style>
  .brand { margin: 0 0 10px; font-size: 15px; color: var(--nk-accent); }
  ul { list-style: none; margin: 0; padding: 0; }
  button {
    display: block; width: 100%; text-align: left;
    background: none; border: none; color: var(--nk-text);
    opacity: 0.82; padding: 5px 8px; border-radius: 6px;
    cursor: pointer; font: inherit;
  }
  button:hover { background: var(--nk-bg-hover); }
  button.active { background: var(--nk-bg-active); color: var(--nk-accent-soft); opacity: 1; }
</style>
