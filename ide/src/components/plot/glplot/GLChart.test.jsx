// @vitest-environment jsdom
import { describe, it, expect, afterEach } from 'vitest';
import { render, cleanup } from '@testing-library/react';
import GLChart from './GLChart';

afterEach(cleanup);

describe('GLChart — render smoke', () => {
  it('mounts a pointer-events-none canvas and stays inert without WebGL', () => {
    // jsdom has no WebGL2 → createGL returns null → the component renders the
    // canvas but draws nothing and never throws (parent keeps SVG for these
    // layers). This guards the graceful-fallback path.
    const { container } = render(
      <GLChart
        series={[{
          data: new Float32Array([0, 0, 1, 1, 2, 0]),
          segments: [{ offset: 0, count: 3 }],
          color: [1, 0, 0, 1],
        }]}
        proj={{ ax: 2, bx: -1, ay: 2, by: -1, xLog: false, yLog: false }}
        plotRect={{ x: 10, y: 10, w: 100, h: 80 }}
        width={120}
        height={100}
        dpr={1}
      />,
    );
    const canvas = container.querySelector('canvas');
    expect(canvas).toBeTruthy();
    expect(canvas.style.pointerEvents).toBe('none');
    expect(canvas.style.position).toBe('absolute');
  });
});
