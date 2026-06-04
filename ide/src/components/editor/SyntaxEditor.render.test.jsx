// @vitest-environment jsdom
import { describe, it, expect, afterEach } from 'vitest';
import { render, cleanup, fireEvent } from '@testing-library/react';
import SyntaxEditor from './SyntaxEditor';

// jsdom lacks ResizeObserver and a real canvas 2-D context (the editor's
// minimap uses both). Stub them so the component's effects run — these
// are environment gaps, not component bugs.
globalThis.ResizeObserver = globalThis.ResizeObserver
  || class { observe() {} unobserve() {} disconnect() {} };
const NOOP_CTX = new Proxy({}, { get: () => () => {}, set: () => true });
HTMLCanvasElement.prototype.getContext = () => NOOP_CTX;

afterEach(cleanup);

// Behaviour tests pinning the editor's render surfaces (gutter, syntax
// highlight, find bar, minimap) BEFORE decomposing the 1331-line component,
// so the extractions can be verified for zero behavioural drift. The
// completion popup is covered directly as a component in CompletionPopup.test.

// Both the gutter and the highlight layer are aria-hidden <pre>s. The gutter
// holds ONLY the line-number column (digits + newlines); the highlight holds
// the code with child <span>s. Disambiguate on that.
function gutterPre(container) {
  return [...container.querySelectorAll('pre[aria-hidden="true"]')]
    .find((pre) => /^[\d\n]+$/.test(pre.textContent || ''));
}
function highlightPre(container) {
  return [...container.querySelectorAll('pre[aria-hidden="true"]')]
    .find((pre) => pre.querySelector('span[style]'));
}

describe('SyntaxEditor — gutter', () => {
  it('renders one line number per line by default', () => {
    const { container } = render(
      <SyntaxEditor value={'a = 1\nb = 2\nc = 3'} onChange={() => {}} />);
    const g = gutterPre(container);
    expect(g).toBeTruthy();
    expect(g.textContent).toBe('1\n2\n3');
  });

  it('omits the gutter when showGutter is false', () => {
    const { container } = render(
      <SyntaxEditor value={'a = 1\nb = 2'} onChange={() => {}} showGutter={false} />);
    expect(gutterPre(container)).toBeUndefined();
  });
});

describe('SyntaxEditor — syntax highlight', () => {
  it('bolds keywords (font-weight:600)', () => {
    const { container } = render(
      <SyntaxEditor value={'for i = 1:3'} onChange={() => {}}
                    showGutter={false} showMinimap={false} />);
    expect(highlightPre(container).innerHTML).toContain('font-weight:600');
  });

  it('italicises comments (font-style:italic)', () => {
    const { container } = render(
      <SyntaxEditor value={'% a comment'} onChange={() => {}}
                    showGutter={false} showMinimap={false} />);
    expect(highlightPre(container).innerHTML).toContain('font-style:italic');
  });

  it('escapes HTML-special characters in the source', () => {
    const { container } = render(
      <SyntaxEditor value={'x < y & z'} onChange={() => {}}
                    showGutter={false} showMinimap={false} />);
    const html = highlightPre(container).innerHTML;
    expect(html).toContain('&lt;');
    expect(html).toContain('&amp;');
  });
});

describe('SyntaxEditor — find / replace / goto bar', () => {
  it('opens the find bar on Ctrl+F', () => {
    const { container } = render(
      <SyntaxEditor value={'alpha beta alpha'} onChange={() => {}} />);
    fireEvent.keyDown(container.querySelector('textarea'), { ctrlKey: true, key: 'f' });
    expect(container.querySelector('input[placeholder="find"]')).toBeTruthy();
  });

  it('opens the replace row on Ctrl+H', () => {
    const { container } = render(
      <SyntaxEditor value={'alpha'} onChange={() => {}} />);
    fireEvent.keyDown(container.querySelector('textarea'), { ctrlKey: true, key: 'h' });
    expect(container.querySelector('input[placeholder="replace"]')).toBeTruthy();
  });

  it('opens the goto bar on Ctrl+G', () => {
    const { container } = render(
      <SyntaxEditor value={'x'} onChange={() => {}} />);
    fireEvent.keyDown(container.querySelector('textarea'), { ctrlKey: true, key: 'g' });
    expect(container.textContent).toContain('line:');
  });
});

describe('SyntaxEditor — minimap', () => {
  it('renders a canvas by default', () => {
    const { container } = render(<SyntaxEditor value={'x'} onChange={() => {}} />);
    expect(container.querySelector('canvas')).toBeTruthy();
  });

  it('omits the canvas when showMinimap is false', () => {
    const { container } = render(
      <SyntaxEditor value={'x'} onChange={() => {}} showMinimap={false} />);
    expect(container.querySelector('canvas')).toBeNull();
  });
});
