// @vitest-environment jsdom
import { describe, it, expect, afterEach, vi } from 'vitest';
import { render, cleanup, fireEvent } from '@testing-library/react';
import { EditorGutter, EditorMinimap } from './editorChrome';

afterEach(cleanup);
const C = { border: '#444', bg0: '#000', textMuted: '#888' };
const ref = () => ({ current: null });

describe('EditorGutter', () => {
  it('renders the line-number column when shown', () => {
    const { container } = render(
      <EditorGutter show gutterRef={ref()} lineNumbers={'1\n2\n3'} C={C} />);
    expect(container.querySelector('pre').textContent).toBe('1\n2\n3');
  });

  it('renders nothing when hidden', () => {
    const { container } = render(
      <EditorGutter show={false} gutterRef={ref()} lineNumbers={'1'} C={C} />);
    expect(container.firstChild).toBeNull();
  });
});

describe('EditorMinimap', () => {
  it('renders a canvas when shown', () => {
    const { container } = render(
      <EditorMinimap show minimapRef={ref()} onMouseDown={() => {}} C={C} />);
    expect(container.querySelector('canvas')).toBeTruthy();
  });

  it('renders nothing when hidden', () => {
    const { container } = render(
      <EditorMinimap show={false} minimapRef={ref()} onMouseDown={() => {}} C={C} />);
    expect(container.firstChild).toBeNull();
  });

  it('fires onMouseDown when the map is pressed', () => {
    const onMouseDown = vi.fn();
    const { container } = render(
      <EditorMinimap show minimapRef={ref()} onMouseDown={onMouseDown} C={C} />);
    fireEvent.mouseDown(container.querySelector('div'));
    expect(onMouseDown).toHaveBeenCalled();
  });
});
