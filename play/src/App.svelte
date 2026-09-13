<script lang="ts">
  // PAGE (R1/R2): composition only — the hash-router outlet. Pages
  // live in src/pages/; every visual is a lib/ component.
  import { parseHash } from './lib/router.js';
  import { entryById, type GalleryEntry } from './lib/galleryModel.js';
  import CatalogPage from './pages/CatalogPage.svelte';
  import CategoryPage from './pages/CategoryPage.svelte';
  import ScriptPage from './pages/ScriptPage.svelte';

  let { gallery = [] }: { gallery?: GalleryEntry[] } = $props();

  let hash = $state(location.hash);
  addEventListener('hashchange', () => (hash = location.hash));

  const route = $derived(parseHash(hash));
  const activeEntry = $derived(
    route.page === 'script' ? entryById(gallery, route.id) : undefined);
</script>

{#if route.page === 'catalog'}
  <CatalogPage {gallery} />
{:else if route.page === 'category'}
  <CategoryPage {gallery} id={route.id} />
{:else if activeEntry}
  {#key activeEntry.id}
    <ScriptPage entry={activeEntry} />
  {/key}
{:else}
  <CatalogPage {gallery} />
{/if}
