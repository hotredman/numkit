<script>
  // StatusBar (R3): engine/error line under the editor.
  import { T } from '../tokens.js';
  let { text = '' } = $props();
</script>

{#if text}
  <div class="status">{text}</div>
{/if}

<style>
  .status {
    padding: 6px 12px; color: var(--nk-danger); background: var(--nk-danger-bg);
    border-top: 1px solid var(--nk-danger-border); font-size: 12px;
  }
</style>
