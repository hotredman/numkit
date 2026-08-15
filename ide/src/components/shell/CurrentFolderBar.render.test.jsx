// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, cleanup, fireEvent } from '@testing-library/react';
import CurrentFolderBar from './CurrentFolderBar';

describe('CurrentFolderBar render tests', () => {
  beforeEach(() => {
    cleanup();
  });

  afterEach(cleanup);

  it('renders correctly in Virtual FS mode', () => {
    const onFsModeChange = vi.fn();
    const onCwdChange = vi.fn();
    const onNavigateUp = vi.fn();
    const onOpenNavigator = vi.fn();

    const { getByDisplayValue, getByTitle } = render(
      <CurrentFolderBar
        fsMode="virtual"
        onFsModeChange={onFsModeChange}
        cwd="/numkit_ide/examples/audio_io"
        onCwdChange={onCwdChange}
        onNavigateUp={onNavigateUp}
        onOpenNavigator={onOpenNavigator}
      />
    );

    const input = getByDisplayValue('/numkit_ide/examples/audio_io');
    expect(input).toBeTruthy();

    const upBtn = getByTitle('Up One Level (..)');
    fireEvent.click(upBtn);
    expect(onNavigateUp).toHaveBeenCalledTimes(1);

    const browseBtn = getByTitle('Explorer');
    fireEvent.click(browseBtn);
    expect(onOpenNavigator).toHaveBeenCalledTimes(1);
  });

  it('triggers onCwdChange when Enter key is pressed in input field', () => {
    const onCwdChange = vi.fn();

    const { getByDisplayValue } = render(
      <CurrentFolderBar
        fsMode="local"
        cwd="C:\Users\User"
        onCwdChange={onCwdChange}
      />
    );

    const input = getByDisplayValue('C:\\Users\\User');
    fireEvent.change(input, { target: { value: 'C:\\Projects' } });
    fireEvent.keyDown(input, { key: 'Enter' });

    expect(onCwdChange).toHaveBeenCalledWith('C:\\Projects');
  });

  it('triggers onFsModeChange when dropdown selection changes', () => {
    const onFsModeChange = vi.fn();

    const { container } = render(
      <CurrentFolderBar
        fsMode="virtual"
        onFsModeChange={onFsModeChange}
        cwd="/"
      />
    );

    const select = container.querySelector('select');
    expect(select).toBeTruthy();
    fireEvent.change(select, { target: { value: 'local' } });

    expect(onFsModeChange).toHaveBeenCalledWith('local');
  });
});
