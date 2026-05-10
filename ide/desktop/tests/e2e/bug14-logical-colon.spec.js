// bug14-logical-colon.spec.js — verify (:) on logical scalar.
//
// BUG #14: y = true; z = y(:) used to segfault. Test whether the
// IDE / WASM engine survives the call now.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('BUG #14 — (:) on logical scalar', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  // Each script below MUST run to completion. Previously these
  // segfaulted the engine. Now they survive but the resulting
  // logical value is dropped → 0 (the colon-flatten doesn't
  // preserve the bit). That's a P1 wrong-result regression of the
  // original P0 crash, separately tracked. These specs pin the
  // no-crash improvement and document the residual value bug.

  test('true(:) no longer crashes (value bug remains)', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'y = true;\n'
      + 'z = y(:);\n'
      + 'fprintf(\'len=%d val=%d\\n\', length(z), z(1));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    // No-crash assertion: console contains "len=" output at all.
    expect(txt).toMatch(/len=1 val=\d/);
    // BUG status: value is currently 0 (wrong) — TODO core fix.
  });

  test('strcmp scalar result + (:) survives end-to-end', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'r = strcmp(\'hello\', \'hello\');\n'
      + 'flat = r(:);\n'
      + 'fprintf(\'sum=%d\\n\', sum(flat));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/sum=\d/);
  });

  test('false(:) — value 0 is correct here', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'y = false;\n'
      + 'z = y(:);\n'
      + 'fprintf(\'val=%d\\n\', z(1));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/val=0/);
  });
});
