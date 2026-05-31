/**
 * StatsBar — a one-row aggregate-statistics strip for the matrix viewer,
 * the third context (alongside the Workspace list and struct inspector)
 * with a "choose which statistics" menu. Shows the active stats over the
 * whole matrix (min/max/range/mean/median/mode/var/std/n); the active set
 * is chosen from a right-click (or ▾ button) menu and persisted.
 *
 * Always renders its wrapper so the parent's row count is stable; the
 * wrapper collapses to zero height when there are no stats (non-numeric
 * value), since the inner bar simply isn't rendered.
 */
import { useState, useEffect } from 'react';
import ContextMenu from './ContextMenu';
import {
  STAT_BAR, loadStatBar, saveStatBar, toggleColumn, statBarValue, fmtStat,
} from './valueColumns';

const STORAGE_KEY = 'numkit.ide.matrixstats';

export default function StatsBar({ stats }) {
  const [visible, setVisible] = useState(() => loadStatBar(STORAGE_KEY));
  const [menu, setMenu] = useState(null);
  useEffect(() => { saveStatBar(STORAGE_KEY, visible); }, [visible]);

  // No stats (non-numeric) → collapsed wrapper: still rendered so the
  // parent grid's row count stays stable, but zero height.
  if (!stats) return <div className="ve-statsbar ve-statsbar--empty" />;

  const items = STAT_BAR.filter((d) => visible.has(d.key));
  const openMenu = (e) => { e.preventDefault(); setMenu({ x: e.clientX, y: e.clientY }); };

  return (
    <div className="ve-statsbar" onContextMenu={openMenu}>
      <button className="ve-statsbar-pick" onClick={openMenu}
        title="choose statistics">Σ ▾</button>
      <div className="ve-statsbar-items">
        {items.map((d) => (
          <span key={d.key} className="ve-stat">
            <b>{d.label}</b> {fmtStat(statBarValue(stats, d.key))}
          </span>
        ))}
        {items.length === 0 && <span className="ve-statsbar-hint">no statistics — Σ ▾ to choose</span>}
      </div>
      {menu && (
        <ContextMenu x={menu.x} y={menu.y} onClose={() => setMenu(null)} items={
          STAT_BAR.map((d) => ({
            label: `${visible.has(d.key) ? '✓' : ' '} ${d.label}`,
            keepOpen: true,
            onClick: () => setVisible((prev) => toggleColumn(prev, d.key)),
          }))
        } />
      )}
    </div>
  );
}
