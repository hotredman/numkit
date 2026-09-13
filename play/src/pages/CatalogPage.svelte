<script lang="ts">
  // PAGE (R1/R2): composition only. The gallery catalog — category
  // cards; the search row switches it to a flat results grid.
  import PageShell from '../lib/ui/PageShell.svelte';
  import TopBar from '../lib/ui/TopBar.svelte';
  import SearchInput from '../lib/ui/SearchInput.svelte';
  import CardGrid from '../lib/ui/CardGrid.svelte';
  import CategoryCard from '../lib/ui/CategoryCard.svelte';
  import ScriptCard from '../lib/ui/ScriptCard.svelte';
  import {
    categoriesOf, searchScripts, categoryName, categoryOfEntry,
  } from '../lib/galleryModel.js';
  import { hrefFor } from '../lib/router.js';
  import type { GalleryEntry } from '../lib/galleryModel.js';

  let { gallery }: { gallery: GalleryEntry[] } = $props();

  let query = $state('');
  const categories = $derived(categoriesOf(gallery));
  const results = $derived(searchScripts(gallery, query));
</script>

<PageShell>
  {#snippet topbar()}
    <TopBar crumbs={[{ label: 'Gallery' }]} />
  {/snippet}

  <CardGrid>
    {#snippet header()}
      <SearchInput value={query} placeholder="search by title, description, file name…"
                   oninput={(v: string) => (query = v)} />
    {/snippet}

    {#if query.trim()}
      {#each results as s (s.id)}
        <ScriptCard title={s.title} description={s.description}
                    category={categoryName(categoryOfEntry(s.id))}
                    href={hrefFor({ page: 'script', id: s.id })} />
      {:else}
        <!-- no results -->
      {/each}
    {:else}
      {#each categories as c (c.id)}
        <CategoryCard name={c.name} count={c.count}
                      href={hrefFor({ page: 'category', id: c.id })} />
      {/each}
    {/if}
  </CardGrid>
</PageShell>
