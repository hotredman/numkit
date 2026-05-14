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

/** Render a single submenu trigger. The submenu uses position:fixed
 *  with coords from the trigger's bounding rect so it pops over any
 *  ancestor with overflow:auto (which would otherwise clip it). */
function SubmenuItem({ item, onClose }) {
  const [open, setOpen] = useState(false);
  const [coords, setCoords] = useState(null);
  const triggerRef = useRef(null);
  useLayoutEffect(() => {
    if (!open) return;
    const el = triggerRef.current;
    if (!el) return;
    const r = el.getBoundingClientRect();
    setCoords({ left: r.right + 2, top: r.top - 4 });
  }, [open]);
  return (
    <div className={`ctx-sub-wrap ${open ? 'is-open' : ''}`}
         onMouseEnter={() => setOpen(true)}
         onMouseLeave={() => setOpen(false)}>
      <button ref={triggerRef}
              className="ctx-item ctx-sub-trigger"
              onClick={(e) => {
                e.stopPropagation();
                setOpen((o) => !o);
              }}>
        <span>{item.submenu}</span>
        <span className="ctx-sub-arrow">▶</span>
      </button>
      {open && coords && (
        <div className="ctx-menu ctx-submenu"
             style={{ position: 'fixed', left: coords.left, top: coords.top }}>
          <MenuItems items={item.items || []} onClose={onClose} />
        </div>
      )}
    </div>
  );
}

/** Render a single item. Wrapped so the same renderer drives the
 *  top-level menu and any nested submenu. */
function MenuItems({ items, onClose }) {
  return items.map((it, i) => {
    if (it.separator) return <div key={i} className="ctx-sep" />;
    if (it.head)      return <div key={i} className="ctx-head">{it.head}</div>;
    if (it.submenu) {
      return <SubmenuItem key={i} item={it} onClose={onClose} />;
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
    // `masked` = state-preserving dimming for "this row is currently a
    // no-op because another setting masks it" (e.g. `box` while
    // `axis=off`). Distinct from `disabled`: the button stays clickable
    // so the user can pre-set a value that applies once the mask lifts.
    return (
      <button key={i}
        className={`ctx-item${it.masked ? ' is-masked' : ''}`}
        disabled={!!it.disabled}
        title={it.maskedHint || ''}
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
