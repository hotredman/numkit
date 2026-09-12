<script>
  // FigureCard (R3): the white canvas card one figure renders into.
  import { T } from '../tokens.js';
  let { children } = $props();
</script>

<div class="card"><slot /></div>

<style>
  .card {
    background: var(--nk-plot-bg); border-radius: 8px; margin-bottom: 12px;
    overflow: hidden;
  }
</style>
