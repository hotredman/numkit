// griddata.spec.js — scattered-data interpolation via Delaunay
// barycentric.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('griddata', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('griddata — linear v(x,y) is exact (bilinear barycentric)', async () => {
    await ide.runScript(
      'import compat.*;\n'
      // Triangle (0,0)-(2,0)-(1,2) with v = x + y at vertices.
      + 'x = [0 2 1 1];\n'
      + 'y = [0 0 2 0.5];\n'
      + 'v = x + y;\n'
      + 'vq = griddata(x, y, v, 1, 0.5);\n'
      + 'fprintf(\'vq=%g\\n\', vq);\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    // v(1, 0.5) = 1 + 0.5 = 1.5 (exact for linear field, exact bary).
    expect(txt).toMatch(/vq=1\.5/);
  });

  test('griddata — query outside hull → NaN', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = [0 1 0.5];\n'
      + 'y = [0 0 1];\n'
      + 'v = [10 20 30];\n'
      + 'vq = griddata(x, y, v, 5, 5);\n'
      + 'fprintf(\'vq_isnan=%d\\n\', isnan(vq));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/vq_isnan=1/);
  });

  test('griddata — vector queries return same shape', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = [0 2 1 1];\n'
      + 'y = [0 0 2 0.5];\n'
      + 'v = x + y;\n'
      + 'vq = griddata(x, y, v, [0.5 1 1.5], [0.2 0.5 0.8]);\n'
      + 'fprintf(\'len=%d\\n\', length(vq));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/len=3/);
  });
});
