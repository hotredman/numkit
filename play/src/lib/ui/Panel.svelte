<script>
  // Panel: titled column body with an actions row (rule R3 — the
  // chrome every pane shares, one file).
  import { T } from '../tokens.js';
  let { title = '' } = $props();
</script>

<section class="panel">
  <header class="head">
    <span class="title">{title}</span>
    <span class="actions"><slot name="actions" /></span>
  </header>
  <div class="body"><slot /></div>
</section>

<style>
  .panel { display: flex; flex-direction: column; min-height: 0; flex: 1; }
  .head {
    display: flex; justify-content: space-between; align-items: center;
    padding: 6px 12px; border-bottom: 1px solid var(--nk-border);
    color: var(--nk-text-dim); font-size: 12px; text-transform: uppercase;
    letter-spacing: 0.06em;
  }
  .actions { display: flex; gap: 8px; }
  .body { flex: 1; min-height: 0; overflow-y: auto; }
</style>
