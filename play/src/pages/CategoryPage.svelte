<script lang="ts">
  // PAGE (R1/R2): composition only. One category — its script cards.
  import PageShell from '../lib/ui/PageShell.svelte';
  import TopBar from '../lib/ui/TopBar.svelte';
  import CardGrid from '../lib/ui/CardGrid.svelte';
  import ScriptCard from '../lib/ui/ScriptCard.svelte';
  import { scriptsOf, categoryName } from '../lib/galleryModel.js';
  import { hrefFor } from '../lib/router.js';
  import type { GalleryEntry } from '../lib/galleryModel.js';

  let { gallery, id }: { gallery: GalleryEntry[]; id: string } = $props();

  const scripts = $derived(scriptsOf(gallery, id));
</script>

<PageShell>
  {#snippet topbar()}
    <TopBar crumbs={[
      { label: 'Gallery', href: '#/' },
      { label: categoryName(id) },
    ]} />
  {/snippet}

  <CardGrid>
    {#each scripts as s (s.id)}
      <ScriptCard title={s.title} description={s.description}
                  href={hrefFor({ page: 'script', id: s.id })} />
    {/each}
  </CardGrid>
</PageShell>
