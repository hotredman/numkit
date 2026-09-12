<script>
  // CodeEditor (R3): the plain monospace editor surface (highlighting
  // lands later as a wrapper, not an edit here).
  import { T } from '../tokens.js';
  let { value = '', oninput } = $props();
</script>

<textarea class="code" spellcheck="false" {value}
          oninput={(e) => oninput?.(e.target.value)}></textarea>

<style>
  .code {
    flex: 1; width: 100%; resize: none; border: none; outline: none;
    background: var(--nk-bg); color: #dfe3ec; padding: 12px;
    font: 12.5px/1.5 var(--font-mono, var(--nk-font-mono));
  }
</style>
