<script lang="ts">
  // TopBar (R3): breadcrumb navigation — the "return to catalog" panel.
  // The last crumb is the current location (emphasised, not a link).
  interface Crumb { label: string; href?: string }
  let { crumbs }: { crumbs: Crumb[] } = $props();
</script>

<nav class="bar">
  {#each crumbs as c, i (i)}
    {#if i > 0}<span class="sep">/</span>{/if}
    {#if c.href && i < crumbs.length - 1}
      <a class="crumb link" href={c.href}>{c.label}</a>
    {:else}
      <span class="crumb current">{c.label}</span>
    {/if}
  {/each}
</nav>

<style>
  .bar {
    display: flex; align-items: center; gap: 8px;
    padding: 8px 16px; font-size: 13px;
  }
  .sep { color: var(--nk-text-faint); }
  .link {
    color: var(--nk-text-dim); text-decoration: none;
    cursor: pointer;
  }
  .link:hover { color: var(--nk-accent); }
  .current { color: var(--nk-text); font-weight: 600; }
</style>
