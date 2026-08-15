// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, cleanup, fireEvent } from '@testing-library/react';
import ModalWindow from './ModalWindow';

describe('ModalWindow component tests', () => {
  beforeEach(() => {
    cleanup();
  });

  afterEach(cleanup);

  it('renders title, tag, subtitle, meta, children and footer', () => {
    const onClose = vi.fn();
    const { container, getByText } = render(
      <ModalWindow
        onClose={onClose}
        tag={{ label: '⚙ test', color: 'blue' }}
        title="Test Modal"
        subtitle="Subtitle Info"
        meta="meta: 123"
        footer={<button>Save</button>}
      >
        <div className="test-body">Hello World</div>
      </ModalWindow>
    );

    expect(getByText('⚙ test')).toBeTruthy();
    expect(getByText('Test Modal')).toBeTruthy();
    expect(getByText('Subtitle Info')).toBeTruthy();
    expect(getByText('meta: 123')).toBeTruthy();
    expect(getByText('Hello World')).toBeTruthy();
    expect(getByText('Save')).toBeTruthy();
    expect(container.querySelector('.modal-window')).toBeTruthy();
  });

  it('handles maximize toggle and restore', () => {
    const { container, getByTitle } = render(
      <ModalWindow onClose={vi.fn()} title="Resizable Window">
        <div>Content</div>
      </ModalWindow>
    );

    const maxBtn = getByTitle('Maximise');
    expect(container.querySelector('.modal-window.is-max')).toBeFalsy();

    fireEvent.click(maxBtn);
    expect(container.querySelector('.modal-window.is-max')).toBeTruthy();

    const restoreBtn = getByTitle('Restore');
    fireEvent.click(restoreBtn);
    expect(container.querySelector('.modal-window.is-max')).toBeFalsy();
  });

  it('calls onClose on backdrop click', () => {
    const onClose = vi.fn();
    const { container } = render(
      <ModalWindow onClose={onClose} title="Backdrop Test">
        <div>Content</div>
      </ModalWindow>
    );

    const overlay = container.querySelector('.modal-overlay');
    fireEvent.click(overlay);
    expect(onClose).toHaveBeenCalledTimes(1);
  });

  it('calls onClose on Escape key press', () => {
    const onClose = vi.fn();
    render(
      <ModalWindow onClose={onClose} title="Escape Test">
        <div>Content</div>
      </ModalWindow>
    );

    fireEvent.keyDown(window, { key: 'Escape' });
    expect(onClose).toHaveBeenCalledTimes(1);
  });
});
