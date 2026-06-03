// @vitest-environment jsdom
//
// Render smoke for the shared ValueTable: default columns, the header
// right-click column chooser (toggle a stat column on/off), persistence
// of the visible set, and that stat cells populate from row.stats.

import { describe, it, expect, beforeEach, afterEach, vi } from 'vitest';
import { render, cleanup, fireEvent, within } from '@testing-library/react';
import ValueTable from './ValueTable';

beforeEach(() => { try { localStorage.clear(); } catch { /* none */ } });
afterEach(cleanup);

const rows = [
  { key: 'Area', name: 'Area', value: '8489', size: '1x1', klass: 'double',
    stats: { min: 8489, max: 8489, mean: 8489, median: 8489, mode: 8489, var: 0, std: 0 } },
  { key: 'Centroid', name: 'Centroid', value: '[1x2 double]', size: '1x2', klass: 'double',
    stats: { min: 119.55, max: 129.37, mean: 124.46, median: 124.46, mode: 119.55, var: 48.2, std: 6.94 }, drill: true },
];

const heads = (c) => [...c.querySelectorAll('.vt-table thead th')].map((th) => th.textContent.trim());

describe('ValueTable', () => {
  it('renders the default columns (name + Value/Size/Bytes/Class)', () => {
    const { container } = render(<ValueTable rows={rows} nameHeader="Field" storageKey="t1" />);
    expect(heads(container)).toEqual(['Field', 'Value', 'Size', 'Bytes', 'Class']);
  });

  it('column chooser toggles a stat column on and persists it', () => {
    const { container } = render(<ValueTable rows={rows} nameHeader="Field" storageKey="t2" />);
    expect(heads(container)).not.toContain('Mean');
    // Right-click the header → chooser, click "Mean".
    fireEvent.contextMenu(container.querySelector('.vt-table thead tr'));
    const meanItem = [...document.querySelectorAll('.ctx-item')].find((b) => /Mean/.test(b.textContent));
    expect(meanItem).toBeTruthy();
    fireEvent.click(meanItem);
    expect(heads(container)).toContain('Mean');
    // Persisted to localStorage.
    expect(JSON.parse(localStorage.getItem('t2'))).toContain('mean');
  });

  it('restores the persisted column set on mount', () => {
    localStorage.setItem('t3', JSON.stringify(['value', 'std']));
    const { container } = render(<ValueTable rows={rows} nameHeader="Field" storageKey="t3" />);
    expect(heads(container)).toEqual(['Field', 'Value', 'Std']);
    // The Std cell is populated from row.stats.
    const table = container.querySelector('.vt-table');
    expect(within(table).getByText('6.94')).toBeTruthy();
  });

  it('Select all shows every column; Clear all leaves only the name column', () => {
    const { container } = render(<ValueTable rows={rows} nameHeader="Field" storageKey="t5" />);
    const openChooser = () => fireEvent.contextMenu(container.querySelector('.vt-table thead tr'));
    const clickItem = (re) => {
      openChooser();
      fireEvent.click([...document.querySelectorAll('.ctx-item')].find((b) => re.test(b.textContent)));
    };
    clickItem(/Select all/);
    expect(heads(container)).toEqual(
      ['Field', 'Value', 'Size', 'Bytes', 'Class', 'Min', 'Max', 'Range', 'Mean', 'Median', 'Mode', 'Var', 'Std']);
    clickItem(/Clear all/);
    expect(heads(container)).toEqual(['Field']);   // name column is locked-on
  });

  it('fires onRowClick only for drillable rows', () => {
    const onRowClick = vi.fn();
    const { container } = render(
      <ValueTable rows={rows} nameHeader="Field" storageKey="t4" onRowClick={onRowClick} />);
    const trs = container.querySelectorAll('.vt-table tbody tr');
    fireEvent.click(trs[0]);  // Area — not drillable
    fireEvent.click(trs[1]);  // Centroid — drillable
    expect(onRowClick).toHaveBeenCalledTimes(1);
    expect(onRowClick.mock.calls[0][0].name).toBe('Centroid');
  });
});
