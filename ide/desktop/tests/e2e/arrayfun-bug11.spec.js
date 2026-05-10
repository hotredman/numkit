// arrayfun-bug11.spec.js — BUG #11 regression guard.
//
// arrayfun used to be a stub that returned its input array verbatim,
// silently dropping the lambda body. Now it actually invokes the
// handle once per element and packs the results.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('arrayfun — BUG #11', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('arrayfun(@(x) x*2, 1:5) → [2 4 6 8 10] (not [1..5])', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'r = arrayfun(@(x) x*2, 1:5);\n'
      + 'fprintf(\'%g %g %g %g %g\\n\', r(1), r(2), r(3), r(4), r(5));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/2 4 6 8 10/);
  });

  test('arrayfun(@(x) x^2, 1:4) → [1 4 9 16]', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'r = arrayfun(@(x) x^2, 1:4);\n'
      + 'fprintf(\'%g %g %g %g\\n\', r(1), r(2), r(3), r(4));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/1 4 9 16/);
  });

  test('arrayfun with multiple input arrays', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'r = arrayfun(@(a, b) a + b, [1 2 3], [10 20 30]);\n'
      + 'fprintf(\'%g %g %g\\n\', r(1), r(2), r(3));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/11 22 33/);
  });

  test('arrayfun + UniformOutput=false → cell array', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'c = arrayfun(@(x) x*3, [1 2 3], \'UniformOutput\', false);\n'
      + 'fprintf(\'class=%s\\n\', class(c));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/class=cell/);
  });
});
