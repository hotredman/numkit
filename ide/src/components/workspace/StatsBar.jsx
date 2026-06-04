/**
 * Matrix StatsBar — the third "choose which statistics" context (with the
 * Workspace list and struct inspector). The chooser TRIGGER lives in the
 * toolbar (StatChooserButton, via the shared ChooserButton) while the
 * values render on their own row below (StatsBar). Both run off one
 * persisted set (useStatChooser).
 */
import {
  STAT_BAR, loadStatBar, saveStatBar, statBarValue, fmtStat,
} from './valueColumns';
import { useChooser, ChooserButton } from './chooser';

const STORAGE_KEY = 'numkit.ide.matrixstats';

/** Persisted active-stats set for the matrix viewer. */
export function useStatChooser() {
  return useChooser(STORAGE_KEY, loadStatBar, saveStatBar);
}

/** Toolbar button (Σ ▾) opening the shared stat chooser. */
export function StatChooserButton({ visible, setVisible }) {
  return (
    <ChooserButton
      label={<>Σ <span className="ve-caret">▾</span></>}
      title="choose statistics"
      defs={STAT_BAR} visible={visible} setVisible={setVisible} />
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
