// workspace.spec.js — regression tests for the Workspace pane UX:
//
//   1. A single click on a variable card opens the Variable Editor
//      (matches the Figures pane interaction model)
//   2. Typing Enter in the editor / console / anywhere does NOT open
//      the Variable Editor — the previous "selected" + window-level
//      keydown listener swallowed Enters meant for the editor
//   3. The card has only a hover style, no persistent selection
//
// Fixed in 7fb111d3.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('Workspace pane', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
    // Seed a workspace variable so we have a card to click.
    await ide.repl('xtest = 42');
    // Workspace pane lives under a dock tab — Console is active by
    // default. Switch over so the cards mount in the DOM.
    await ide.openWorkspaceTab();
    // At least one card present; we don't lock the count because a
    // boot script or implicit `ans` may also be present.
    await expect(ide.workspaceCards.first()).toBeVisible({ timeout: 5_000 });
  });

  test.afterEach(async () => {
    await closeIde(app);
  });

  test('single click on a workspace card opens the Variable Editor', async () => {
    // Click the card we just created (its name is the unique seed).
    const card = ide.workspaceCards.filter({ hasText: 'xtest' });
    await expect(card).toHaveCount(1);
    await card.click();
    await expect(ide.variableEditor).toBeVisible({ timeout: 5_000 });
  });

  test('Enter in the editor textarea does NOT open the Variable Editor', async () => {
    // The bug was: clicking a card set "selected" state in WorkspacePanel,
    // which installed a window-level keydown handler that opened the
    // Variable Editor on Enter. Enter pressed anywhere — even the
    // editor pane — popped the modal. After 7fb111d3 there's no
    // selection state and no global keydown.
    const card = ide.workspaceCards.filter({ hasText: 'xtest' });
    await card.click();
    await page.keyboard.press('Escape');
    await expect(ide.variableEditor).not.toBeVisible();

    // Now click into the editor textarea and press Enter (newline).
    await ide.editor.click();
    await ide.editor.press('Enter');
    // Variable editor must NOT have re-opened from that Enter.
    await expect(ide.variableEditor).not.toBeVisible();
  });

  test('workspace card has no persistent selection state', async () => {
    const card = ide.workspaceCards.filter({ hasText: 'xtest' });
    // Pre-7fb111d3 the card got class .is-selected on click and kept
    // it. We removed both the state and the .is-selected style; the
    // class should never appear on hover.
    await card.hover();
    const cls = await card.getAttribute('class');
    expect(cls).not.toMatch(/is-selected/);
  });
});
