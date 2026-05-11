// inpolygon-convhull.spec.js — computational-geometry primitives.

import { test, expect } from '../../helpers/shared.js';

test.describe('inpolygon / convhull', () => {
  test('inpolygon — square polygon, points inside / outside', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      // Unit square with vertices (0,0)(1,0)(1,1)(0,1).
      + 'xv = [0 1 1 0];\n'
      + 'yv = [0 0 1 1];\n'
      + 'xq = [0.5 0.5 1.5 -0.1];\n'
      + 'yq = [0.5 0.9 0.5 0.5];\n'
      + 'in = inpolygon(xq, yq, xv, yv);\n'
      + 'fprintf(\'%d %d %d %d\\n\', in(1), in(2), in(3), in(4));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    // Expected: 1 1 0 0 (first two inside, last two outside).
    expect(txt).toMatch(/1 1 0 0/);
  });

  test('inpolygon — triangle polygon', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      // Triangle (0,0)-(2,0)-(1,2).
      + 'xv = [0 2 1];\n'
      + 'yv = [0 0 2];\n'
      + 'in = inpolygon([1 1 0.5 1.5], [1 0.5 1.5 1.5], xv, yv);\n'
      + 'fprintf(\'%d %d %d %d\\n\', in(1), in(2), in(3), in(4));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    // (1,1) inside; (1, 0.5) inside; (0.5, 1.5) outside; (1.5, 1.5) outside
    expect(txt).toMatch(/1 1 0 0/);
  });

  test('convhull — square cloud → 5-vertex polygon (closed)', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      // 4 corners of a unit square + 1 interior point.
      + 'x = [0 1 1 0 0.5];\n'
      + 'y = [0 0 1 1 0.5];\n'
      + 'k = convhull(x, y);\n'
      + 'fprintf(\'len=%d first=%d last=%d\\n\', length(k), k(1), k(end));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    // 4 hull vertices + repeat first → length 5. First == last (closed).
    expect(txt).toMatch(/len=5/);
    const m = txt.match(/first=(\d+) last=(\d+)/);
    expect(m).toBeTruthy();
    expect(m[1]).toBe(m[2]);
  });

  test('polyarea — unit square area = 1', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'a = polyarea([0 1 1 0], [0 0 1 1]);\n'
      + 'fprintf(\'a=%g\\n\', a);\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/a=1\b/);
  });

  test('polyarea — triangle area = 0.5 * base * height', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      // (0,0)-(2,0)-(1,2) — base 2, height 2, area 2.
      + 'a = polyarea([0 2 1], [0 0 2]);\n'
      + 'fprintf(\'a=%g\\n\', a);\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/a=2\b/);
  });

  test('boundary — equivalent to convhull (v1: shrink no-op)', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = [0 1 1 0 0.5];\n'
      + 'y = [0 0 1 1 0.5];\n'
      + 'k = boundary(x, y);\n'
      + 'fprintf(\'len=%d\\n\', length(k));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/len=5/);
  });

  test('boundary(x, y, shrink) — concave envelope on C-shape', async ({ ide, page }) => {
    // Points along a C-shape — concave boundary should follow the curve.
    await ide.runScript(
      'import compat.*;\n'
      // Outer arc plus inner indent (cloud with a notch).
      + 'theta = linspace(0, 2*pi, 24);\n'
      + 'x = [cos(theta), 0.3*cos(theta)];\n'
      + 'y = [sin(theta), 0.3*sin(theta)];\n'
      + 'k0 = boundary(x, y, 0);\n'   // convex hull
      + 'k1 = boundary(x, y, 0.9);\n' // tight
      + 'fprintf(\'k0=%d k1=%d\\n\', length(k0), length(k1));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    // Tighter shrink should usually produce more boundary vertices than
    // the convex hull (more wraparound through interior points).
    const m = txt.match(/k0=(\d+) k1=(\d+)/);
    expect(m).toBeTruthy();
    expect(Number(m[2])).toBeGreaterThanOrEqual(Number(m[1]));
  });

  test('convhull — collinear points (degenerate)', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = [0 1 2];\n'
      + 'y = [0 1 2];\n'
      + 'try; k = convhull(x, y); fprintf(\'ok len=%d\\n\', length(k)); '
      + 'catch err; fprintf(\'err: %s\\n\', err.message); end\n'
    );
    await page.waitForTimeout(200);
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
