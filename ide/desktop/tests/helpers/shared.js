// helpers/shared.js — shared-Electron test fixture.
//
// The default per-test launch/teardown costs ~2s per test (Electron boot
// + WASM init + waitForReady). Across 500 tests that's ~16 min just on
// boilerplate. This fixture opens Electron ONCE per worker and resets
// engine + UI state between tests via `clear; close all` plus zeroing
// the captured console-message buffers.
//
// Usage in a spec:
//
//   import { test, expect } from '../helpers/shared.js';
//
//   test('does something', async ({ ide, page }) => {
//     await ide.runScript(...);
//     ...
//   });
//
// Fixtures provided:
//   ide   — IdePage with reset state (logs cleared, workspace cleared)
//   page  — the underlying Playwright Page (same as ide.page)
//
// Caveats: tests must avoid leaking persistent UI state that the reset
// can't undo (e.g. mutating localStorage keys without restoring). For
// such tests use the original launchIde/closeIde from launch.js instead.

import { test as base, expect } from '@playwright/test';
import { launchIde, closeIde } from './launch.js';
import { IdePage } from './ide.js';

export const test = base.extend({
  // Worker-scoped: one Electron per worker for the lifetime of the run.
  // `auto: false` means the fixture only initialises when a test actually
  // depends on `ide` or `page` — keeps cold-start out of unused workers.
  electron: [async ({}, use) => {
    const app = await launchIde();
    const page = await app.firstWindow();
    const ide = new IdePage(page);
    await ide.waitForReady();
    await use({ app, page, ide });
    await closeIde(app);
  }, { scope: 'worker', auto: false }],

  // Test-scoped: thin wrapper that resets state between tests.
  ide: async ({ electron }, use) => {
    electron.ide.devMessages.length = 0;
    electron.ide.pageErrors.length = 0;
    // Close any modal overlay left open by the prior test (figure window
    // or variable editor). Modal blocks clicks on the editor / Run button.
    // Esc handler is bound at IDE.jsx top level. Loop in case nested.
    for (let i = 0; i < 4; i++) {
      const overlay = electron.page.locator('.fw-overlay, .ve-overlay');
      if (await overlay.count() === 0) break;
      await electron.page.keyboard.press('Escape');
      await electron.page.waitForTimeout(50);
    }
    // Restore the dock to Console — workspace.spec.js / similar leave it
    // on the Workspace tab, which hides the .console-input element and
    // breaks any subsequent test that calls ide.repl().
    const consoleTab = electron.page.locator('.dock-tab', { hasText: /console/i });
    if (await consoleTab.count() > 0) {
      const isActive = await consoleTab.first().evaluate((el) => el.classList.contains('is-active'));
      if (!isActive) await consoleTab.first().click();
    }
    // `clear` empties the workspace; `close all` closes any figure
    // windows + cards left over from a prior test. Both cheap.
    await electron.ide.runScript('clear\nclose all\n');
    await electron.page.waitForTimeout(80);
    await use(electron.ide);
  },

  // Convenience accessor so specs can keep `async ({ ide, page }) => {}`
  // signature without reaching through electron.
  page: async ({ electron }, use) => {
    await use(electron.page);
  },
});

export { expect };
