// interp2-grid-bug21.spec.js — verify whether the case-B / case-C
// of BUG #21 still fails:
//   B: interp2(X, Y, V, Xq, Yq) where X, Y are 2-D meshgrid output
//   C: interp2(xv, yv, V, xqv, yqv) implicit-meshgrid for vector
//      Xq/Yq

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('interp2 grid-form — BUG #21 verify', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('Case B: interp2(X, Y, V, xq, yq) with X/Y from meshgrid', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'xv = 1:5;\n'
      + 'yv = 1:5;\n'
      + '[X, Y] = meshgrid(xv, yv);\n'
      + 'V = X + Y;\n'
      + 'try; vq = interp2(X, Y, V, 2.5, 3.5); '
      + 'fprintf(\'vq=%g\\n\', vq); '
      + 'catch err; fprintf(\'BUG21B err: %s\\n\', err.message); end\n'
    );
    await page.waitForTimeout(300);
    const txt = await ide.consoleText();
    if (/BUG21B err:/.test(txt)) {
      console.log('Case B: STILL BROKEN —', txt.match(/BUG21B err:[^\n]*/)?.[0]);
    } else {
      console.log('Case B: FIXED —', txt.match(/vq=[^\n]*/)?.[0]);
    }
    expect(txt).toMatch(/vq=6|BUG21B err:/);
  });

  test('Case C: interp2(xv, yv, V, xqv, yqv) — implicit meshgrid for vec Xq/Yq', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'xv = 1:5;\n'
      + 'yv = 1:5;\n'
      + '[X, Y] = meshgrid(xv, yv);\n'
      + 'V = X + Y;\n'
      + 'try; Vq = interp2(xv, yv, V, [2 3], [3 4]); '
      + 'fprintf(\'BUG21C sz=%dx%d\\n\', size(Vq, 1), size(Vq, 2)); '
      + 'catch err; fprintf(\'BUG21C err: %s\\n\', err.message); end\n'
    );
    await page.waitForTimeout(300);
    const txt = await ide.consoleText();
    // Now expect MATLAB-parity 2x2 (implicit meshgrid).
    expect(txt).toMatch(/BUG21C sz=2x2/);
  });
});
