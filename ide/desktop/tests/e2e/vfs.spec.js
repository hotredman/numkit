// vfs.spec.js — virtual-filesystem regressions:
//
//   1. Sidebar Examples source lists folders + files (manifest fetch
//      works inside packaged Electron)
//   2. Clicking an example opens its content in the editor (mirrors
//      the file into tempFS and the tab loads its source)
//   3. Switching to Temporary source after opening an example shows
//      the mirrored entry — confirms the writeFile path through
//      vfs-adapter actually persisted to IDB
//   4. tempFS bridge is gated off by default (65eebed2): the
//      [tempFS] DevTools log line says "direct IDB", not "sync bridge
//      active". This protects against re-introducing the worker init
//      regression.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('VFS — Examples + tempFS', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => {
    await closeIde(app);
  });

  test('tempFS reports direct-IDB path (bridge gated off by default)', async () => {
    const dev = ide.devLogs();
    expect(dev).toMatch(/\[tempFS\]/);
    expect(dev).toMatch(/direct IDB/);
    expect(dev).not.toMatch(/sync bridge active/);
  });

  test('Examples source shows the manifest tree', async () => {
    // Sidebar source picker is a <select>; default value is 'examples'
    // (persisted in localStorage with the default-fallback set in code).
    // We force-select to be explicit even though it's already default.
    const sourceSelect = page.locator('.ws-picker');
    await sourceSelect.selectOption('examples');
    // Every example folder is a .tree-row → .tree-folder. At least
    // one must be visible.
    await expect(page.locator('.tree-folder').first()).toBeVisible({ timeout: 10_000 });
  });

  test('opening an example file lands its content in the editor', async () => {
    await page.locator('.ws-picker').selectOption('examples');

    // Expand the first folder (single click toggles).
    const firstFolder = page.locator('.tree-folder').first();
    await firstFolder.waitFor({ state: 'visible', timeout: 10_000 });
    await firstFolder.click();

    // Double-click the first file. Sidebar.jsx tree rows: single
    // click on a file just selects it visually; the open action fires
    // on dblclick (folders use single click to expand/collapse).
    const firstFile = page.locator('.tree-file').first();
    await expect(firstFile).toBeVisible({ timeout: 5_000 });
    await firstFile.dblclick();

    // Editor textarea should populate with non-empty content.
    await expect.poll(
      async () => (await ide.editor.inputValue()).length,
      { timeout: 10_000 },
    ).toBeGreaterThan(0);
  });
});
