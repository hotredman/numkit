// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, cleanup, fireEvent, act } from '@testing-library/react';
import FileNavigatorModal from './FileNavigatorModal';

describe('FileNavigatorModal render smoke', () => {
  beforeEach(() => {
    cleanup();
  });

  afterEach(cleanup);

  it('mounts the FileNavigator modal without throwing', async () => {
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

    const { container, getByText, getByPlaceholderText } = rendered;
    expect(container.querySelector('.fw-overlay')).toBeTruthy();
    expect(getByText(/File Navigator/i)).toBeTruthy();
    expect(getByPlaceholderText('Search files…')).toBeTruthy();

    // Close button
    const closeBtn = getByText('✕');
    fireEvent.click(closeBtn);
    expect(onClose).toHaveBeenCalledTimes(1);
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
