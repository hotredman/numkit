<script lang="ts">
  // SearchInput (R3): the catalog search box — controlled, reports via
  // oninput (R6: props down, callbacks up).
  interface Props {
    value?: string;
    placeholder?: string;
    oninput?: (v: string) => void;
  }
  let { value = '', placeholder = '', oninput }: Props = $props();
</script>

<input class="search" type="text" {placeholder} {value}
       oninput={(e) => oninput?.((e.target as HTMLInputElement).value)} />

<style>
  .search {
    width: 100%; box-sizing: border-box;
    padding: 8px 12px; border-radius: 6px;
    border: 1px solid var(--nk-border);
    background: var(--nk-bg); color: var(--nk-text);
    font: 13px/1.4 var(--nk-font);
    outline: none;
  }
  .search:focus { border-color: var(--nk-accent); }
  .search::placeholder { color: var(--nk-text-faint); }
</style>
