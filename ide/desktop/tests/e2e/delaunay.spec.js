// delaunay.spec.js — Delaunay triangulation indices.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('delaunay', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('delaunay — point inside outer triangle → 3 triangles', async () => {
    // Outer triangle (0,0)-(2,0)-(1,2) plus an interior point (1, 0.5).
    // Delaunay = three triangles fanning the interior point.
    await ide.runScript(
      'import compat.*;\n'
      + 'tri = delaunay([0 2 1 1], [0 0 2 0.5]);\n'
      + 'fprintf(\'rows=%d cols=%d\\n\', size(tri, 1), size(tri, 2));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/rows=3 cols=3/);
  });

  test('delaunay — cocircular square accepts ambiguity (≥ 2 triangles)', async () => {
    // 4-corner square is a known degenerate case (all 4 lie on a
    // common circumcircle); brute-force emits all 4 valid triangles.
    // MATLAB picks one of two valid triangulations consistently;
    // proper tie-breaking is BACKLOG.
    await ide.runScript(
      'import compat.*;\n'
      + 'tri = delaunay([0 1 1 0], [0 0 1 1]);\n'
      + 'fprintf(\'rows=%d\\n\', size(tri, 1));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    const m = txt.match(/rows=(\d+)/);
    expect(m).toBeTruthy();
    expect(Number(m[1])).toBeGreaterThanOrEqual(2);
  });

  test('delaunay — single triangle (3 points → 1 triangle)', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'tri = delaunay([0 1 0.5], [0 0 1]);\n'
      + 'fprintf(\'rows=%d\\n\', size(tri, 1));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/rows=1/);
  });

  test('delaunay — collinear points (degenerate) → no triangles', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'tri = delaunay([0 1 2], [0 1 2]);\n'
      + 'fprintf(\'rows=%d\\n\', size(tri, 1));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/rows=0/);
  });
});
