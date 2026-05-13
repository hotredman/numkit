/**
 * Floating context menu, anchored at viewport (clientX/clientY) coords.
 * Caller manages open/close state. Item kinds:
 *
 *   { label, onClick, disabled? }                — regular menu item
 *   { separator: true }                          — divider line
 *   { head: 'Section' }                          — section heading
 *   { row: true, name, color, buttons: [...] }   — series row (name + 3 buttons)
 *   { submenu: 'Label', items: [...] }           — nested submenu
 *
 * Nested submenus open to the side on hover; the parent closes when the
 * submenu fires a callback. Submenu items use the same shape as the
 * top-level items array.
 *
 * The menu auto-clamps to the viewport so it doesn't spill past the right
 * edge / bottom edge — useful when ПКМ lands near the modal corner.
 */
import { useEffect, useLayoutEffect, useRef, useState } from 'react';

/** Render a single item. Wrapped so the same renderer drives the
 *  top-level menu and any nested submenu. */
function MenuItems({ items, onClose }) {
  // Track which submenu is currently open by index. null = none.
  const [openSubIdx, setOpenSubIdx] = useState(null);
  return items.map((it, i) => {
    if (it.separator) return <div key={i} className="ctx-sep" />;
    if (it.head)      return <div key={i} className="ctx-head">{it.head}</div>;
    if (it.submenu) {
      // Nested submenu — opens on hover or click. The submenu sits to
      // the right of its parent row; positioned absolutely relative to
      // the row container.
      const isOpen = openSubIdx === i;
      return (
        <div key={i}
             className={`ctx-sub-wrap ${isOpen ? 'is-open' : ''}`}
             onMouseEnter={() => setOpenSubIdx(i)}
             onMouseLeave={() => setOpenSubIdx((cur) => (cur === i ? null : cur))}>
          <button className="ctx-item ctx-sub-trigger"
                  onClick={(e) => {
                    e.stopPropagation();
                    setOpenSubIdx(isOpen ? null : i);
                  }}>
            <span>{it.submenu}</span>
            <span className="ctx-sub-arrow">▶</span>
          </button>
          {isOpen && (
            <div className="ctx-menu ctx-submenu">
              <MenuItems items={it.items || []} onClose={onClose} />
            </div>
          )}
        </div>
      );
    }
    if (it.row) {
      return (
        <div key={i} className="ctx-row">
          <span className="ctx-name">
            {it.color && <i style={{ background: it.color }} />}
            <span>{it.name}</span>
          </span>
          {it.buttons.map((b, j) => (
            <button key={j} className="ctx-row-btn" disabled={!!b.disabled}
              onClick={(e) => { e.stopPropagation(); b.onClick?.(); onClose(); }}
            >
              {b.label}
            </button>
          ))}
        </div>
      );
    }
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
  });
}

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
    function onOuterScroll(e) {
      // Internal scroll inside the menu (overflow-y) bubbles to window; only
      // close on scrolls that originate outside.
      if (ref.current && ref.current.contains(e.target)) return;
      onClose();
    }
    document.addEventListener('mousedown', onDocDown);
    document.addEventListener('keydown', onKey);
    window.addEventListener('scroll', onOuterScroll, true);
    window.addEventListener('resize', onClose);
    return () => {
      document.removeEventListener('mousedown', onDocDown);
      document.removeEventListener('keydown', onKey);
      window.removeEventListener('scroll', onOuterScroll, true);
      window.removeEventListener('resize', onClose);
    };
  }, [onClose]);

  return (
    <div ref={ref} className="ctx-menu"
      style={{ position: 'fixed', left: pos.x, top: pos.y, zIndex: 2000 }}
      onContextMenu={(e) => e.preventDefault()}
    >
      <MenuItems items={items} onClose={onClose} />
    </div>
  );
}

/**
 * Helper — fold a flat list of `row`-style items into a single submenu
 * when there are more than `threshold` of them. Useful for per-series
 * menus where N can grow large (delaunay-style for-loops, big plot
 * stacks). Returns either the original array (≤ threshold) or
 * `[{ submenu: title, items: rows }]` for caller to splice in.
 */
export function foldRowsToSubmenu(rows, title, threshold = 5) {
  if (!Array.isArray(rows) || rows.length <= threshold) return rows;
  return [{ submenu: title, items: rows }];
}
