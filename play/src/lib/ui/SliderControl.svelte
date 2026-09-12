<script>
  // SliderControl (R3): range control driven by a manifest entry; the
  // owning page owns the VALUE (R6) — we report, not store.
  import { T } from '../tokens.js';
  let { label, min = 0, max = 100, step = 1, unit = '', value = 0,
        oninput } = $props();
</script>

<label class="ctl">
  <span class="lab">{label}</span>
  <input type="range" {min} {max} {step} {value}
         oninput={(e) => oninput?.(Number(e.target.value))} />
  <span class="val">{value}{unit}</span>
</label>

<style>
  .ctl { display: block; margin-bottom: 10px; }
  .lab { display: block; font-size: 12px; color: #aab2c0; }
  input[type='range'] { width: 100%; }
  .val { font-size: 12px; color: var(--nk-accent); }
</style>
