<script>
  // ControlGroup (R3): one @play manifest group — heading + controls.
  let { name = '', children } = $props();
</script>

<section class="group">
  {#if name}<h3 class="name">{name}</h3>{/if}
  {@render children?.()}
</section>

<style>
  .group { margin-top: 6px; }
  .name {
    margin: 4px 0 8px; font-size: 12px;
    color: #8b93a3; text-transform: uppercase; letter-spacing: 0.05em;
  }
</style>
