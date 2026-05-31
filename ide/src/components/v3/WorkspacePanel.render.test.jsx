// @vitest-environment jsdom
//
// Render smoke for the Workspace panel's list view + toolbar column
// chooser button (the shared ChooserButton, same as the matrix Σ▾ and the
// table header). Verifies the button appears in list view, toggles a
// column, persists, and is absent in cards view.

import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import { render, cleanup, fireEvent } from '@testing-library/react';
import { WorkspacePanel } from './Workspace';

beforeEach(() => {
  try {
    localStorage.clear();
    localStorage.setItem('numkit.ide.workspace.view', 'list');
  } catch { /* none */ }
});
afterEach(cleanup);

const vars = [
  { name: 'A', type: 'double', kind: 'scalar', size: '1×1', bytes: 8, preview: '3',
    stats: { min: 3, max: 3, mean: 3, median: 3, mode: 3, var: 0, std: 0 } },
  { name: 'B', type: 'double', kind: 'matrix', size: '1×3', bytes: 24, preview: '[1 2 3]',
    stats: { min: 1, max: 3, mean: 2, median: 2, mode: 1, var: 1, std: 1 } },
];
const heads = (c) => [...c.querySelectorAll('.vt-table thead th')].map((t) => t.textContent.trim());

describe('WorkspacePanel — toolbar column chooser', () => {
  it('list view shows a columns ▾ button; toggling Mean adds the column and persists', () => {
    const { container } = render(<WorkspacePanel variables={vars} onOpen={() => {}} />);
    const btn = container.querySelector('.ws-cols-btn');
    expect(btn).toBeTruthy();
    expect(heads(container)).toEqual(['Name', 'Value', 'Size', 'Class']);  // default
    fireEvent.click(btn);
    const meanItem = [...document.querySelectorAll('.ctx-item')].find((b) => /\bMean\b/.test(b.textContent));
    fireEvent.click(meanItem);
    expect(heads(container)).toContain('Mean');
    expect(JSON.parse(localStorage.getItem('numkit.ide.valuecols'))).toContain('mean');
  });

  it('cards view does not show the columns button', () => {
    localStorage.setItem('numkit.ide.workspace.view', 'cards');
    const { container } = render(<WorkspacePanel variables={vars} onOpen={() => {}} />);
    expect(container.querySelector('.ws-cols-btn')).toBeNull();
  });

  it('Select all in the toolbar chooser shows every column', () => {
    const { container } = render(<WorkspacePanel variables={vars} onOpen={() => {}} />);
    fireEvent.click(container.querySelector('.ws-cols-btn'));
    fireEvent.click([...document.querySelectorAll('.ctx-item')].find((b) => /Select all/.test(b.textContent)));
    expect(heads(container)).toEqual(
      ['Name', 'Value', 'Size', 'Class', 'Min', 'Max', 'Range', 'Mean', 'Median', 'Mode', 'Var', 'Std']);
  });
});
