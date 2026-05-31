/**
 * Matrix StatsBar — the third "choose which statistics" context (with the
 * Workspace list and struct inspector). Split into three pieces so the
 * chooser TRIGGER can live in the toolbar (next to heatmap/csv/plot) while
 * the values render on their own row below:
 *
 *   useStatChooser()           — persisted "which stats" set (shared state)
 *   <StatChooserButton/>       — the Σ ▾ toolbar button + chooser menu
 *   <StatsBar stats visible/>  — the values row (collapses when none shown)
 */
import { useState, useEffect } from 'react';
import ContextMenu from './ContextMenu';
import {
  STAT_BAR, loadStatBar, saveStatBar, toggleColumn, statBarValue, fmtStat,
} from './valueColumns';

const STORAGE_KEY = 'numkit.ide.matrixstats';

/** Persisted active-stats set for the matrix viewer. */
export function useStatChooser() {
  const [visible, setVisible] = useState(() => loadStatBar(STORAGE_KEY));
  useEffect(() => { saveStatBar(STORAGE_KEY, visible); }, [visible]);
  return [visible, setVisible];
}

/** Toolbar button (Σ ▾) opening the checkbox stat chooser. */
export function StatChooserButton({ visible, setVisible }) {
  const [menu, setMenu] = useState(null);
  return (
    <>
      <button className="ve-btn" title="choose statistics"
        onClick={(e) => setMenu({ x: e.clientX, y: e.clientY })}>
        Σ <span className="ve-caret">▾</span>
      </button>
      {menu && (
        <ContextMenu x={menu.x} y={menu.y} onClose={() => setMenu(null)} items={[
          { label: 'Select all', keepOpen: true,
            onClick: () => setVisible(new Set(STAT_BAR.map((d) => d.key))) },
          { label: 'Clear all', keepOpen: true,
            onClick: () => setVisible(new Set()) },
          { separator: true },
          ...STAT_BAR.map((d) => ({
            label: `${visible.has(d.key) ? '✓' : ' '} ${d.label}`,
            keepOpen: true,
            onClick: () => setVisible((prev) => toggleColumn(prev, d.key)),
          })),
        ]} />
      )}
    </>
  );
}

/** The aggregate-stats values row. Renders nothing visible (collapsed,
 *  but still in the DOM so the parent grid's row count is stable) when the
 *  value is non-numeric OR no statistic is selected. */
export default function StatsBar({ stats, visible }) {
  const items = stats ? STAT_BAR.filter((d) => visible.has(d.key)) : [];
  if (items.length === 0) return <div className="ve-statsbar ve-statsbar--empty" />;
  return (
    <div className="ve-statsbar">
      <div className="ve-statsbar-items">
        {items.map((d) => (
          <span key={d.key} className="ve-stat">
            <b>{d.label}</b> {fmtStat(statBarValue(stats, d.key))}
          </span>
        ))}
      </div>
    </div>
  );
}
