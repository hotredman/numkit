// @vitest-environment jsdom
import { describe, it, expect, afterEach, vi } from 'vitest';
import { render, cleanup, fireEvent } from '@testing-library/react';
import CompletionPopup from './CompletionPopup';

afterEach(cleanup);

const C = { bg0: '#000', bg2: '#222', border: '#444', accent: '#08f', text: '#eee', textMuted: '#888' };
const anchor = { line: 0, col: 0 };
// Non-builtin tokens → no BUILTIN_INFO description, so each item's only text
// node is its name (getByText stays unambiguous for the interaction tests).
const props = (over) => ({ items: ['aaa', 'bbb'], anchor, activeIdx: 0, onAccept: () => {}, onHover: () => {}, C, ...over });

describe('CompletionPopup', () => {
  it('renders nothing when there are no items', () => {
    const { container } = render(<CompletionPopup {...props({ items: [] })} />);
    expect(container.firstChild).toBeNull();
  });

  it('renders one row per item', () => {
    const { getByText } = render(<CompletionPopup {...props()} />);
    expect(getByText('aaa')).toBeTruthy();
    expect(getByText('bbb')).toBeTruthy();
  });

  it('calls onAccept with the item on mousedown', () => {
    const onAccept = vi.fn();
    const { getByText } = render(<CompletionPopup {...props({ onAccept })} />);
    fireEvent.mouseDown(getByText('bbb'));
    expect(onAccept).toHaveBeenCalledWith('bbb');
  });

  it('calls onHover with the row index on mouseenter', () => {
    const onHover = vi.fn();
    const { getByText } = render(<CompletionPopup {...props({ onHover })} />);
    fireEvent.mouseEnter(getByText('bbb').closest('div'));
    expect(onHover).toHaveBeenCalledWith(1);
  });

  it('shows the description tail for a known builtin', () => {
    // `plot` carries a BUILTIN_INFO entry "plot — ..."; the popup renders the
    // text after the em-dash as a second span next to the name.
    const { container } = render(<CompletionPopup {...props({ items: ['plot'] })} />);
    const spans = container.querySelectorAll('div > div > span');
    expect(spans.length).toBe(2);   // name + description
  });
});
