/**
 * Floating context menu, anchored at viewport (clientX/clientY) coords.
 * Caller manages open/close state. Items are `{ label, onClick, disabled? }`
 * or `{ separator: true }` to render a divider.
 *
 * The menu auto-clamps to the viewport so it doesn't spill past the right
 * edge / bottom edge — useful when ПКМ lands near the modal corner.
 */
import { useEffect, useLayoutEffect, useRef, useState } from 'react';

export default function ContextMenu({ x, y, items, onClose }) {
  const ref = useRef(null);
  const [pos, setPos] = useState({ x, y });

  useLayoutEffect(() => {
    const el = ref.current;
    if (!el) return;
    const r = el.getBoundingClientRect();
    const vw = window.innerWidth;
    const vh = window.innerHeight;
    let nx = x, ny = y;
    if (nx + r.width  > vw - 4) nx = Math.max(4, vw - r.width  - 4);
    if (ny + r.height > vh - 4) ny = Math.max(4, vh - r.height - 4);
    setPos({ x: nx, y: ny });
  }, [x, y]);

  useEffect(() => {
    function onDocDown(e) {
      if (ref.current && !ref.current.contains(e.target)) onClose();
    }
    function onKey(e) { if (e.key === 'Escape') onClose(); }
    function onScroll() { onClose(); }
    document.addEventListener('mousedown', onDocDown);
    document.addEventListener('keydown', onKey);
    window.addEventListener('scroll', onScroll, true);
    window.addEventListener('resize', onScroll);
    return () => {
      document.removeEventListener('mousedown', onDocDown);
      document.removeEventListener('keydown', onKey);
      window.removeEventListener('scroll', onScroll, true);
      window.removeEventListener('resize', onScroll);
    };
  }, [onClose]);

  return (
    <div ref={ref} className="ctx-menu"
      style={{ position: 'fixed', left: pos.x, top: pos.y, zIndex: 2000 }}
      onContextMenu={(e) => e.preventDefault()}
    >
      {items.map((it, i) => {
        if (it.separator) return <div key={i} className="ctx-sep" />;
        return (
          <button key={i} className="ctx-item" disabled={!!it.disabled}
            onClick={(e) => {
              e.stopPropagation();
              it.onClick?.();
              onClose();
            }}
          >
            {it.label}
          </button>
        );
      })}
    </div>
  );
}
