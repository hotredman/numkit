import { useRef, useEffect } from 'react';

/**
 * Resizable handle. `orientation`:
 *   'vertical'   → drags horizontally (between L/R panels)
 *   'horizontal' → drags vertically   (between T/B panels)
 *
 * `onResize(dx, dy)` is called with the cursor delta.
 */
export default function ResizeHandle({ orientation, onResize, onDoubleClick }) {
  const dragging = useRef(false);

  const cleanup = () => {
    if (dragging.current) {
      dragging.current = false;
      document.body.style.cursor = '';
      document.body.style.userSelect = '';
    }
  };

  useEffect(() => {
    const handleGlobalUp = () => cleanup();
    const handleBlur = () => cleanup();
    window.addEventListener('pointerup', handleGlobalUp);
    window.addEventListener('pointercancel', handleGlobalUp);
    window.addEventListener('mouseup', handleGlobalUp);
    window.addEventListener('blur', handleBlur);
    return () => {
      cleanup();
      window.removeEventListener('pointerup', handleGlobalUp);
      window.removeEventListener('pointercancel', handleGlobalUp);
      window.removeEventListener('mouseup', handleGlobalUp);
      window.removeEventListener('blur', handleBlur);
    };
  }, []);

  function onPointerDown(e) {
    dragging.current = true;
    try { e.target.setPointerCapture(e.pointerId); } catch { /* harmless */ }
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
