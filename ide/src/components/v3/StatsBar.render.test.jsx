// @vitest-environment jsdom
//
// Render smoke for the matrix StatsBar: default stats render, the chooser
// toggles a stat on/off and persists, and a null-stats value collapses
// the bar (empty modifier, stable grid row).

import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import { render, cleanup, fireEvent, within } from '@testing-library/react';
import StatsBar from './StatsBar';

beforeEach(() => { try { localStorage.clear(); } catch { /* none */ } });
afterEach(cleanup);

const stats = { min: 2, max: 8, mean: 4.6667, median: 4, mode: 4, var: 4.2667, std: 2.0656, n: 6 };

describe('StatsBar', () => {
  it('renders the default stats (min/max/mean/n)', () => {
    const { container } = render(<StatsBar stats={stats} />);
    const bar = container.querySelector('.ve-statsbar');
    expect(bar).toBeTruthy();
    expect(within(bar).getByText('min')).toBeTruthy();
    expect(within(bar).getByText('n')).toBeTruthy();
    expect(within(bar).queryByText('std')).toBeNull();   // off by default
  });

  it('chooser toggles a stat on and persists it', () => {
    const { container } = render(<StatsBar stats={stats} />);
    fireEvent.contextMenu(container.querySelector('.ve-statsbar'));
    const stdItem = [...document.querySelectorAll('.ctx-item')].find((b) => /\bstd\b/.test(b.textContent));
    fireEvent.click(stdItem);
    expect(within(container.querySelector('.ve-statsbar')).getByText('std')).toBeTruthy();
    expect(JSON.parse(localStorage.getItem('numkit.ide.matrixstats'))).toContain('std');
  });

  it('collapses (empty modifier, still in the DOM) when there are no stats', () => {
    const { container } = render(<StatsBar stats={null} />);
    const bar = container.querySelector('.ve-statsbar');
    expect(bar).toBeTruthy();                      // present → grid row stable
    expect(bar.classList.contains('ve-statsbar--empty')).toBe(true);
    expect(bar.textContent).toBe('');
  });
});
