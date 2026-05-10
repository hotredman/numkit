// mldivide-bug28.spec.js — verify mldivide/mrdivide/mpower named-fn forms.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('mldivide / mrdivide / mpower — BUG #28', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('mldivide(A, b) === A \\ b for 2×2 linear system', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'A = [1 2; 3 4];\n'
      + 'b = [5; 11];\n'
      + 'x1 = A \\ b;\n'
      + 'x2 = mldivide(A, b);\n'
      + 'fprintf(\'%g %g %g %g\\n\', x1(1), x1(2), x2(1), x2(2));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    // x = [1; 2] for the system [1 2; 3 4] * x = [5; 11].
    expect(txt).toMatch(/1 2 1 2/);
  });

  test('mrdivide(b, A) === b / A', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'A = 2;\n'
      + 'b = 10;\n'
      + 'q1 = b / A;\n'
      + 'q2 = mrdivide(b, A);\n'
      + 'fprintf(\'%g %g\\n\', q1, q2);\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/5 5/);
  });

  test('mpower(A, n) === A^n', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'A = [2 0; 0 3];\n'
      + 'P1 = A ^ 3;\n'
      + 'P2 = mpower(A, 3);\n'
      + 'fprintf(\'%g %g %g %g\\n\', P1(1,1), P1(2,2), P2(1,1), P2(2,2));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    // 2^3 = 8, 3^3 = 27 — both copies.
    expect(txt).toMatch(/8 27 8 27/);
  });
});
