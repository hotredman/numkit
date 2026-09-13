<script lang="ts">
  // ScriptCard (R3): one gallery script — on category pages and in
  // search results. Category label optional (search mixes categories).
  interface Props {
    title: string;
    description?: string;
    href: string;
    category?: string;
  }
  let { title, description = '', href, category = '' }: Props = $props();
</script>

<a class="card" {href}>
  {#if category}<span class="cat">{category}</span>{/if}
  <span class="title">{title}</span>
  {#if description}<span class="desc">{description}</span>{/if}
</a>

<style>
  .card {
    display: flex; flex-direction: column; gap: 6px;
    padding: 14px 16px; border-radius: 8px;
    border: 1px solid var(--nk-border);
    background: var(--nk-bg-panel);
    text-decoration: none;
    transition: border-color 0.15s, background 0.15s;
  }
  .card:hover { border-color: var(--nk-accent); background: var(--nk-bg-hover); }
  .cat {
    color: var(--nk-text-faint); font-size: 11px;
    text-transform: uppercase; letter-spacing: 0.05em;
  }
  .title { color: var(--nk-text); font-size: 14px; font-weight: 600; }
  .desc {
    color: var(--nk-text-dim); font-size: 12px; line-height: 1.45;
    display: -webkit-box; -webkit-line-clamp: 2; line-clamp: 2;
    -webkit-box-orient: vertical;
    overflow: hidden;
  }
</style>
