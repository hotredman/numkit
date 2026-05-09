// smoke.spec.js — boot path: app launches, main UI is wired, no
// loud errors during initialisation. If any of these fail the rest
// of the suite is meaningless, so we run this first.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('boot', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
  });

  test.afterEach(async () => {
    await closeIde(app);
  });

  test('main window opens with the expected title', async () => {
    await expect(page).toHaveTitle(/Numkit IDE/);
  });

  test('renderer reaches a ready state (editor + console present)', async () => {
    await ide.waitForReady();
    await expect(ide.editor).toBeVisible();
    await expect(ide.console).toBeVisible();
  });

  test('console banner is printed (engine init completed)', async () => {
    await ide.waitForReady();
    const text = await ide.consoleText();
    expect(text).toMatch(/Numkit IDE v3/);
    expect(text).toMatch(/help/);
  });

  test('no renderer console errors during boot', async () => {
    await ide.waitForReady();
    // Filter out known-benign noise:
    //  - DevTools "Autofill.enable" warning (Electron quirk)
    //  - Vite-HMR errors only seen in dev mode (not packaged)
    const real = ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e)
      && !/\[hmr\]/i.test(e)
    );
    expect(real, `Unexpected console errors:\n${real.join('\n')}`).toEqual([]);
  });

  test('tempFS path is reported in DevTools console', async () => {
    await ide.waitForReady();
    const dev = ide.devLogs();
    // The [tempFS] log line is emitted by ide/src/temporary.js during
    // module load. We want to know one of these landed (boot reached
    // tempFS init) AND that it isn't a bridge construction failure
    // on a runtime where the bridge should have worked.
    expect(dev, 'expected [tempFS] init log').toMatch(/\[tempFS\]/);
    expect(dev).not.toMatch(/bridge construction failed/);
  });
});
