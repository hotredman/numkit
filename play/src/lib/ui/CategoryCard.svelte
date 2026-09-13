<script lang="ts">
  // CategoryCard (R3): one gallery category on the catalog page.
  interface Props { name: string; count: number; href: string }
  let { name, count, href }: Props = $props();
</script>

<a class="card" {href}>
  <span class="name">{name}</span>
  <span class="count">{count} script{count === 1 ? '' : 's'}</span>
</a>

<style>
  .card {
    display: flex; flex-direction: column; gap: 6px;
    padding: 18px 16px; border-radius: 8px;
    border: 1px solid var(--nk-border);
    background: var(--nk-bg-panel);
    text-decoration: none;
    transition: border-color 0.15s, background 0.15s;
  }
  .card:hover { border-color: var(--nk-accent); background: var(--nk-bg-hover); }
  .name { color: var(--nk-text); font-size: 15px; font-weight: 600; }
  .count { color: var(--nk-text-dim); font-size: 12px; }
</style>
