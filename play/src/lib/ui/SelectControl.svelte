<script>
  // SelectControl (R3): dropdown for manifest select-controls.
  import { T } from '../tokens.js';
  let { label, options = [], value = '', onchange } = $props();
</script>

<label class="ctl">
  <span class="lab">{label}</span>
  <select {value} onchange={(e) => onchange?.(e.target.value)}>
    {#each options as opt}
      <option value={opt}>{opt}</option>
    {/each}
  </select>
</label>

<style>
  .ctl { display: block; margin-bottom: 10px; }
  .lab { display: block; font-size: 12px; color: #aab2c0; }
  select {
    width: 100%; background: var(--nk-bg-hover); color: var(--nk-text);
    border: 1px solid #2f3542; border-radius: 4px; padding: 4px;
    font: inherit;
  }
</style>
