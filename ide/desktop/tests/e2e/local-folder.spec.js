// local-folder.spec.js — Local Folder mount path. Reproducing the
// real "click Open Folder dialog → pick a directory" flow in tests
// would require driving a native OS dialog; instead we bypass the
// picker by writing the persisted root path to localStorage and
// reloading. nativeFS.reconnect() (in ide/src/fs/local.js) picks it
// up on boot and behaves exactly as if the user had picked it.
//
// The fixture tests/fixtures/sample-folder/ contains a node_modules
// subdir on purpose so we can verify TREE_SKIP_DIRS still filters
// it out — that filter was the round-3 fix for the OOM caused by
// listTree walking 14 000+ entries on a populated folder.

import { test, expect } from '@playwright/test';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';
import { tabMemory } from '../helpers/metrics.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const FIXTURE_ROOT = path.resolve(__dirname, '..', 'fixtures', 'sample-folder');

test.describe('Local Folder mount', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();

    // Pre-seed: persist a "mounted folder" + sidebar source so the
    // next reload triggers nativeFS.reconnect() against the fixture.
    await page.evaluate((root) => {
      localStorage.setItem('numkit.ide.native.root', root);
      localStorage.setItem('numkit.ide.sidebar.source', '"localFolder"');
    }, FIXTURE_ROOT);
    await page.reload();
    await ide.waitForReady();
  });

  test.afterEach(async () => {
    await closeIde(app);
  });

  test('Sidebar shows the fixture tree', async () => {
    // Wait for the tree to actually populate after reconnect.
    await expect(page.locator('.tree-row', { hasText: 'hello.m' }))
      .toBeVisible({ timeout: 10_000 });
    await expect(page.locator('.tree-row', { hasText: 'data.csv' })).toBeVisible();
    await expect(page.locator('.tree-row', { hasText: 'subdir' })).toBeVisible();
  });

  test('node_modules is filtered out by TREE_SKIP_DIRS', async () => {
    await expect(page.locator('.tree-row', { hasText: 'hello.m' }))
      .toBeVisible({ timeout: 10_000 });
    // The fixture has a node_modules/junk.txt; if the skip list
    // regressed we'd see "node_modules" or "junk.txt" in the tree.
    const tree = page.locator('.tree-row');
    const all = await tree.allInnerTexts();
    expect(all.some((t) => /node_modules/i.test(t))).toBe(false);
    expect(all.some((t) => /junk\.txt/i.test(t))).toBe(false);
  });

  test('mounting does not blow Tab process memory', async () => {
    // Wait for the tree (mount complete).
    await expect(page.locator('.tree-row', { hasText: 'hello.m' }))
      .toBeVisible({ timeout: 10_000 });

    const m = await tabMemory(app);
    // Pre-fix this exact path (mount populated folder, listTree
    // recurse, seed all contents into a JS Map) was the proven cause
    // of the 4 GB OOM. Sane upper bound on a small fixture: 350 MB.
    expect(m.workingSet, `Tab WS=${m.workingSet}MB after mount`)
      .toBeLessThan(350);
  });

  test('opening a fixture file populates the editor', async () => {
    const helloRow = page.locator('.tree-row', { hasText: 'hello.m' });
    await expect(helloRow).toBeVisible({ timeout: 10_000 });
    await helloRow.dblclick();

    await expect.poll(
      async () => (await ide.editor.inputValue()),
      { timeout: 10_000 },
    ).toMatch(/hello from sample-folder/);
  });
});
