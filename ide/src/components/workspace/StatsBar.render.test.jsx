// @vitest-environment jsdom
//
// Render smoke for the matrix StatsBar split: the values row is purely
// presentational (driven by a `visible` set) and collapses when empty,
// while the toolbar Σ▾ chooser button toggles + persists the set.

import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import { render, cleanup, fireEvent, within } from '@testing-library/react';
import StatsBar, { useStatChooser, StatChooserButton } from './StatsBar';

beforeEach(() => { try { localStorage.clear(); } catch { /* none */ } });
afterEach(cleanup);

const stats = { min: 2, max: 8, mean: 4.6667, median: 4, mode: 4, var: 4.2667, std: 2.0656, n: 6 };
const isEmpty = (c) => c.querySelector('.ve-statsbar').classList.contains('ve-statsbar--empty');

describe('StatsBar (values row)', () => {
  it('renders the selected stats', () => {
    const { container } = render(<StatsBar stats={stats} visible={new Set(['min', 'max', 'n'])} />);
    expect(isEmpty(container)).toBe(false);
    const bar = container.querySelector('.ve-statsbar');
    expect(within(bar).getByText('min')).toBeTruthy();
    expect(within(bar).queryByText('std')).toBeNull();
  });
  it('collapses (empty modifier) when nothing is selected', () => {
    const { container } = render(<StatsBar stats={stats} visible={new Set()} />);
    expect(isEmpty(container)).toBe(true);
  });
  it('collapses when the value is non-numeric (no stats)', () => {
    const { container } = render(<StatsBar stats={null} visible={new Set(['min'])} />);
    expect(isEmpty(container)).toBe(true);
  });
});

// Harness wiring the toolbar button + row through the shared hook, as
// MatrixPanel does.
function Harness({ stats: s }) {
  const [visible, setVisible] = useStatChooser();
  return (
    <div>
      <StatChooserButton visible={visible} setVisible={setVisible} />
      <StatsBar stats={s} visible={visible} />
    </div>
  );
}

describe('StatChooserButton (toolbar) + persistence', () => {
  it('toggles a stat on via the chooser, shows it in the row, and persists', () => {
    const { container } = render(<Harness stats={stats} />);
    // Defaults (min/max/mean/n) are shown.
    expect(within(container.querySelector('.ve-statsbar')).getByText('mean')).toBeTruthy();
    fireEvent.click(container.querySelector('.ve-btn'));          // Σ ▾
    const stdItem = [...document.querySelectorAll('.ctx-item')].find((b) => /\bstd\b/.test(b.textContent));
    fireEvent.click(stdItem);
    expect(within(container.querySelector('.ve-statsbar')).getByText('std')).toBeTruthy();
    expect(JSON.parse(localStorage.getItem('numkit.ide.matrixstats'))).toContain('std');
  });

  it('hides the row entirely when every stat is unchecked', () => {
    localStorage.setItem('numkit.ide.matrixstats', JSON.stringify([]));
    const { container } = render(<Harness stats={stats} />);
    expect(isEmpty(container)).toBe(true);
  });

  it('Select all shows every stat; Clear all hides the row', () => {
    const { container } = render(<Harness stats={stats} />);
    const clickItem = (re) => {
      fireEvent.click(container.querySelector('.ve-btn'));
      fireEvent.click([...document.querySelectorAll('.ctx-item')].find((b) => re.test(b.textContent)));
    };
    clickItem(/Select all/);
    const bar = container.querySelector('.ve-statsbar');
    for (const lbl of ['min', 'max', 'range', 'mean', 'median', 'mode', 'var', 'std', 'n']) {
      expect(within(bar).getByText(lbl)).toBeTruthy();
    }
    clickItem(/Clear all/);
    expect(isEmpty(container)).toBe(true);
  });
});
