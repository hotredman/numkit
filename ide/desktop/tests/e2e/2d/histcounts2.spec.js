// histcounts2.spec.js — 2-D histogram bin counts.

import { test, expect } from '../../helpers/shared.js';

test.describe('histcounts2', () => {
  test('histcounts2 default 10×10 bins on uniform cloud', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(0, 1, 100);\n'
      + 'y = linspace(0, 1, 100);\n'
      + 'N = histcounts2(x, y);\n'
      + 'fprintf(\'sz=%dx%d total=%d\\n\', size(N, 1), size(N, 2), sum(N(:)));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/sz=10x10 total=100/);
  });

  test('histcounts2 with [nx ny] explicit bin count', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(0, 1, 50);\n'
      + 'y = linspace(0, 1, 50);\n'
      + 'N = histcounts2(x, y, [4 5]);\n'
      + 'fprintf(\'sz=%dx%d total=%d\\n\', size(N, 1), size(N, 2), sum(N(:)));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/sz=4x5 total=50/);
  });

  test('histcounts2 with explicit edges', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = [0.5 1.5 2.5];\n'
      + 'y = [0.5 1.5 2.5];\n'
      + 'N = histcounts2(x, y, [0 1 2 3], [0 1 2 3]);\n'
      + 'fprintf(\'sz=%dx%d total=%d diag=%d %d %d\\n\', '
      + 'size(N, 1), size(N, 2), sum(N(:)), N(1,1), N(2,2), N(3,3));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/sz=3x3 total=3 diag=1 1 1/);
  });
});
