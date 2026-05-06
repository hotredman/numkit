import { useRef } from 'react';

/**
 * Resizable handle. `orientation`:
 *   'vertical'   → drags horizontally (between L/R panels)
 *   'horizontal' → drags vertically   (between T/B panels)
 *
 * `onResize(dx, dy)` is called with the cursor delta.
 */
export default function ResizeHandle({ orientation, onResize, onDoubleClick }) {
  const dragging = useRef(false);

  function onPointerDown(e) {
    dragging.current = true;
    e.target.setPointerCapture(e.pointerId);
    document.body.style.cursor = orientation === 'vertical' ? 'col-resize' : 'row-resize';
    document.body.style.userSelect = 'none';
  }
  function onPointerMove(e) {
    if (!dragging.current) return;
    onResize(e.movementX, e.movementY);
  }
  function onPointerUp(e) {
    if (dragging.current) {
      dragging.current = false;
      try { e.target.releasePointerCapture(e.pointerId); } catch { /* harmless */ }
      document.body.style.cursor = '';
      document.body.style.userSelect = '';
    }
  }

  return (
    <div
      className={`rs-handle rs-${orientation}`}
      onPointerDown={onPointerDown}
      onPointerMove={onPointerMove}
      onPointerUp={onPointerUp}
      onPointerCancel={onPointerUp}
      onDoubleClick={onDoubleClick}
    />
  );
}
