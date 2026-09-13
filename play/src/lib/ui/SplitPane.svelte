<script lang="ts">
  // SplitPane (R3): a draggable horizontal split between two panes.
  // Pointer-capture drag on the divider; double-click resets to the
  // default. The fraction persists across page remounts (module state).
  import type { Snippet } from 'svelte';
  import { splitFrac } from './splitFrac.svelte.js';

  const DIVIDER_PX = 6;
  const DEFAULT_FRAC = 0.45;

  let {
    first, second,
    minFrac = 0.15, maxFrac = 0.85,
  }: {
    first: Snippet; second: Snippet; minFrac?: number; maxFrac?: number;
  } = $props();

  let container = $state<HTMLElement | null>(null);
  let dragging = $state(false);

  function onPointerDown(e: PointerEvent) {
    dragging = true;
    (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
  }
  function onPointerMove(e: PointerEvent) {
    if (!dragging || !container) return;
    const r = container.getBoundingClientRect();
    const inner = Math.max(1, r.width - DIVIDER_PX);
    splitFrac.value =
      Math.min(maxFrac, Math.max(minFrac, (e.clientX - r.left) / inner));
  }
  function onPointerUp(e: PointerEvent) {
    dragging = false;
    (e.currentTarget as HTMLElement).releasePointerCapture(e.pointerId);
  }
</script>

<div class="split" bind:this={container}>
  <div class="pane first" style:flex-basis={`calc((100% - ${DIVIDER_PX}px) * ${splitFrac.value})`}>
    {@render first()}
  </div>
  <div class="divider" role="separator" aria-orientation="vertical"
       class:dragging
       onpointerdown={onPointerDown}
       onpointermove={onPointerMove}
       onpointerup={onPointerUp}
       onpointercancel={onPointerUp}
       ondblclick={() => (splitFrac.value = DEFAULT_FRAC)}></div>
  <div class="pane second">{@render second()}</div>
</div>

<style>
  .split { display: flex; height: 100%; min-width: 0; }
  .pane { min-width: 0; min-height: 0; overflow: hidden; }
  .first { flex: 0 0 auto; }
  .second { flex: 1 1 0; }
  .divider {
    flex: 0 0 6px; cursor: col-resize;
    background: var(--nk-border);
    transition: background 0.15s;
  }
  /* Wider invisible hit area around the visual bar. */
  .divider::after { content: ''; position: absolute; }
  .divider { position: relative; }
  .divider::after { inset: 0 -4px; }
  .divider:hover, .divider.dragging { background: var(--nk-accent); }
</style>
