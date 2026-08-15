// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, cleanup, fireEvent, act } from '@testing-library/react';
import FileNavigatorModal from './FileNavigatorModal';

describe('FileNavigatorModal render smoke', () => {
  beforeEach(() => {
    cleanup();
  });

  afterEach(cleanup);

  it('mounts the FileNavigator modal with unified ModalWindow chrome', async () => {
    const onClose = vi.fn();
    const onSetCurrentFolder = vi.fn();

    let rendered;
    await act(async () => {
      rendered = render(
        <FileNavigatorModal
          onClose={onClose}
          fsMode="virtual"
          currentCwd="/numkit_ide/examples"
          onSetCurrentFolder={onSetCurrentFolder}
        />
      );
      await Promise.resolve();
    });

    const { container, getByText, getByPlaceholderText, getByTitle } = rendered;
    expect(container.querySelector('.modal-overlay')).toBeTruthy();
    expect(container.querySelector('.modal-window')).toBeTruthy();
    expect(getByText('Explorer')).toBeTruthy();
    expect(getByPlaceholderText('Search files…')).toBeTruthy();

    // Maximize button
    const maxBtn = getByTitle('Maximise');
    expect(maxBtn).toBeTruthy();
    fireEvent.click(maxBtn);
    expect(container.querySelector('.modal-window.is-max')).toBeTruthy();

    // Close button
    const closeBtn = getByTitle('Close (Esc)');
    fireEvent.click(closeBtn);
    expect(onClose).toHaveBeenCalledTimes(1);
  });

  it('handles column header click sorting', async () => {
    let rendered;
    await act(async () => {
      rendered = render(
        <FileNavigatorModal
          onClose={vi.fn()}
          fsMode="virtual"
          currentCwd="/numkit_ide/examples"
        />
      );
      await Promise.resolve();
    });

    const { getByTitle } = rendered;
    const nameHeader = getByTitle('Sort by Name');
    const sizeHeader = getByTitle('Sort by Size');
    const modHeader = getByTitle('Sort by Modified Date');

    expect(nameHeader.textContent).toContain('▲');

    // Click Name to toggle desc
    fireEvent.click(nameHeader);
    expect(nameHeader.textContent).toContain('▼');

    // Click Size to sort by size
    fireEvent.click(sizeHeader);
    expect(sizeHeader.textContent).toContain('▲');

    // Click Modified to sort by date
    fireEvent.click(modHeader);
    expect(modHeader.textContent).toContain('▲');
  });

  it('closes on Escape key press', async () => {
    const onClose = vi.fn();

    await act(async () => {
      render(
        <FileNavigatorModal
          onClose={onClose}
          fsMode="virtual"
          currentCwd="/"
        />
      );
      await Promise.resolve();
    });

    fireEvent.keyDown(window, { key: 'Escape' });
    expect(onClose).toHaveBeenCalledTimes(1);
  });
});
