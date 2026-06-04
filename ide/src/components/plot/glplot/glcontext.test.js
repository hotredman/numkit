import { describe, it, expect } from 'vitest';
import { glViewportRect, isWebGL2Available } from './glcontext';

describe('glViewportRect', () => {
  it('maps the plot rect to a y-flipped GL viewport in buffer pixels', () => {
    // plot rect (50,10,400,300) in a 500×400 viewBox; canvas 1000×800 (dpr 2)
    const vp = glViewportRect({ x: 50, y: 10, w: 400, h: 300 },
                              { w: 500, h: 400 }, { w: 1000, h: 800 });
    // x = 50*2 = 100, w = 400*2 = 800, h = 300*2 = 600,
    // top = 10*2 = 20 → bottom-origin y = 800 - (20+600) = 180
    expect(vp).toEqual({ x: 100, y: 180, w: 800, h: 600 });
  });

  it('fills the canvas when the plot rect equals the viewBox', () => {
    const vp = glViewportRect({ x: 0, y: 0, w: 200, h: 100 },
                              { w: 200, h: 100 }, { w: 200, h: 100 });
    expect(vp).toEqual({ x: 0, y: 0, w: 200, h: 100 });
  });
});

describe('isWebGL2Available', () => {
  it('returns a boolean (false under jsdom)', () => {
    expect(typeof isWebGL2Available()).toBe('boolean');
  });
});
