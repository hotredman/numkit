/**
 * chooser.jsx — the one column/stat chooser shared by all three contexts
 * (Workspace toolbar, struct/matrix table header, matrix StatsBar). A
 * single button + menu and a single persisted-set hook, so the "choose
 * which columns / statistics" UX isn't reimplemented per surface.
 */
import { useState, useEffect } from 'react';
import ContextMenu from '../ui/ContextMenu';
import { buildChooserItems } from './valueColumns';

/** Persisted on/off set. `load`/`save` are the column- or stat-specific
 *  serializers from valueColumns (so the key's known-key filtering is
 *  correct). Returns [visible, setVisible]. */
export function useChooser(storageKey, load, save) {
  const [visible, setVisible] = useState(() => load(storageKey));
  useEffect(() => { save(storageKey, visible); }, [storageKey, visible]); // eslint-disable-line react-hooks/exhaustive-deps
  return [visible, setVisible];
}

/** A button that opens the shared chooser menu. `label` is the button
 *  content (e.g. "columns ▾" or "Σ ▾"); `defs` is the [{key,label}] list;
 *  `lockedLabel` renders a disabled always-on row. */
export function ChooserButton({
  className = 've-btn', label, title = 'choose columns',
  defs, visible, setVisible, lockedLabel,
}) {
  const [menu, setMenu] = useState(null);
  return (
    <>
      <button className={className} title={title}
        onClick={(e) => setMenu({ x: e.clientX, y: e.clientY })}>
        {label}
      </button>
      {menu && (
        <ContextMenu x={menu.x} y={menu.y} onClose={() => setMenu(null)}
          items={buildChooserItems({ defs, visible, setVisible, lockedLabel })} />
      )}
    </>
  );
}
