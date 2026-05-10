// impzlength-bug32.spec.js — confirm BUG #32 stayed fixed.
//
// MATLAB:  impzlength([1 -0.5], [1 -0.99]) = 985
// numkit:  used to return 1146 (cap at 50, then 1146 with conservative
//          decay tolerance). Commit 67e8a8a1 swapped to the canonical
//          floor(log(5e-5)/log(rho)) formula. This test pins the post-
//          fix value so a future regression is loud.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('impzlength — BUG #32 regression guard', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('impzlength([1 -0.5], [1 -0.99]) === 985 (MATLAB parity)', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'n = impzlength([1 -0.5], [1 -0.99]);\n'
      + 'fprintf(\'n=%d\\n\', n);\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/n=985/);
  });

  test('impzlength FIR filter (a trivial) returns numel(b)', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'n = impzlength([1 2 3 4 5], 1);\n'
      + 'fprintf(\'n=%d\\n\', n);\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/n=5/);
  });

  test('impzlength varies with pole magnitude', async () => {
    // Per MATLAB: rho = 0.5 → 14, rho = 0.9 → 93.
    await ide.runScript(
      'import compat.*;\n'
      + 'n1 = impzlength([1], [1 -0.5]);\n'
      + 'n2 = impzlength([1], [1 -0.9]);\n'
      + 'fprintf(\'n1=%d n2=%d\\n\', n1, n2);\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/n1=14 n2=93/);
  });
});
