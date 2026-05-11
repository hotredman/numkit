// ide-smoke.spec.js — basic IDE liveness checks.
//
// One-shot smoke that catches regressions in the renderer boot path
// (WASM glue load, examples manifest, REPL banner, run-button wiring).
// Doesn't assert business logic — that's the job of focused specs.

import { test, expect } from '../../helpers/shared.js';

const NON_FATAL = (e) =>
  !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e);

test.describe('IDE smoke', () => {
  test('boots, runs trivial script, no console errors', async ({ ide, page }) => {
    await ide.runScript('import compat.*;\nx = 1 + 2;\n');
    await page.waitForTimeout(150);

    // Three top-level panes mount on boot — Editor (toolbar pill +
    // textarea), Console (banner already asserted by waitForReady),
    // and the dock at the bottom (Workspace tab is one of them).
    await expect(ide.editor).toBeVisible();
    await expect(ide.console).toBeVisible();
    await expect(ide.dockTab('Workspace')).toBeVisible();

    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });

  test('plot creates a figure card; close-all clears the pane', async ({ ide }) => {
    await ide.runScript('import compat.*;\nplot([1 2 3], [1 4 9]);\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });

    await ide.figureCloseAll.click();
    await expect(ide.figureCards).toHaveCount(0, { timeout: 5_000 });

    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });
});
