import { useEffect } from 'react';

/**
 * Re-fit a React Flow view whenever its container resizes.
 *
 * React Flow's `fitView` prop only fits ONCE (on the initial layout).
 * When the pane is later resized — toggling a sibling editor pane,
 * opening/closing the Figures panel, or resizing the window — the
 * viewport keeps its stale transform, so the graph no longer fills the
 * pane (content ends up clipped / not flush to the edges). Observe the
 * container and re-fit on every size change, once `enabled` is true.
 *
 * @param {{current: HTMLElement|null}} containerRef  ref on the RF container
 * @param {{ fitView: Function }}       reactFlow     useReactFlow() instance
 * @param {boolean}                     enabled       gate (e.g. `laidOut`)
 */
export function useFitOnResize(containerRef, reactFlow, enabled) {
  useEffect(() => {
    const el = containerRef.current;
    if (!el || !enabled) return undefined;
    let raf = 0;
    const ro = new ResizeObserver(() => {
      // Defer one frame so React Flow's own resize handler has updated
      // its internal width/height before we fit against it.
      cancelAnimationFrame(raf);
      raf = requestAnimationFrame(() => reactFlow.fitView());
    });
    ro.observe(el);
    return () => { cancelAnimationFrame(raf); ro.disconnect(); };
  }, [containerRef, reactFlow, enabled]);
}
