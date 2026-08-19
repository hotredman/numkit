import { describe, it, expect } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import FiguresPane from './FiguresPane';

describe('FiguresPane controls', () => {
  const dummyFigures = [
    { id: 1, kind: 'composite', datasets: [], xRange: [0, 10], yRange: [0, 10] },
    { id: 2, kind: 'composite', datasets: [], xRange: [0, 10], yRange: [0, 10] },
  ];

  it('renders columns and aspect ratio menu buttons', () => {
    render(
      <FiguresPane
        figures={dummyFigures}
        onExpand={() => {}}
        onCloseFigure={() => {}}
        onCloseAll={() => {}}
      />
    );

    expect(screen.getByTitle(/Number of columns/i)).toBeTruthy();
    expect(screen.getByTitle(/Aspect ratio/i)).toBeTruthy();
    expect(screen.getByText('Figure 1')).toBeTruthy();
    expect(screen.getByText('Figure 2')).toBeTruthy();
  });

  it('changes columns count and updates grid layout', () => {
    const { container } = render(
      <FiguresPane
        figures={dummyFigures}
        onExpand={() => {}}
        onCloseFigure={() => {}}
        onCloseAll={() => {}}
      />
    );

    const colsBtn = screen.getByTitle(/Number of columns/i);
    fireEvent.click(colsBtn);

    const col3Option = screen.getByText('3 columns');
    fireEvent.click(col3Option);

    const stack = container.querySelector('.fp-stack');
    expect(stack.style.gridTemplateColumns).toBe('repeat(3, minmax(0, 1fr))');
  });

  it('changes aspect ratio when selected from dropdown', () => {
    render(
      <FiguresPane
        figures={dummyFigures}
        onExpand={() => {}}
        onCloseFigure={() => {}}
        onCloseAll={() => {}}
      />
    );

    const aspectBtn = screen.getByTitle(/Aspect ratio/i);
    fireEvent.click(aspectBtn);

    const ratio43 = screen.getByText('4:3');
    fireEvent.click(ratio43);

    expect(screen.getByTitle(/Aspect ratio/i).textContent).toContain('4:3');
  });
});
