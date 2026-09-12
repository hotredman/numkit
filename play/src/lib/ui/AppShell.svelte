<script>
  // Three-column app frame (rule R1: pages get layout as a component,
  // never a grid of divs).
  import { T } from '../tokens.js';
</script>

<div class="shell">
  <div class="col side"><slot name="sidebar" /></div>
  <div class="col mid"><slot name="editor" /></div>
  <div class="col figs"><slot name="figures" /></div>
</div>

<style>
  .shell {
    display: grid;
    grid-template-columns: 260px 1fr 1.2fr;
    height: 100%;
  }
  .side { border-right: 1px solid var(--nk-border); background: var(--nk-bg-panel); overflow-y: auto; }
  .mid { border-right: 1px solid var(--nk-border); display: flex; flex-direction: column; }
  .figs { display: flex; flex-direction: column; }
</style>
