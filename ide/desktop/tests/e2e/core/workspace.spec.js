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

import { test, expect } from '../../helpers/shared.js';

async function seedWorkspaceVar(ide) {
  await ide.repl('xtest = 42');
  await ide.openWorkspaceTab();
  await expect(ide.workspaceCards.first()).toBeVisible({ timeout: 5_000 });
}

test.describe('Workspace pane', () => {
  test('single click on a workspace card opens the Variable Editor', async ({ ide }) => {
    await seedWorkspaceVar(ide);
    // Click the card we just created (its name is the unique seed).
    const card = ide.workspaceCards.filter({ hasText: 'xtest' });
    await expect(card).toHaveCount(1);
    await card.click();
    await expect(ide.variableEditor).toBeVisible({ timeout: 5_000 });
  });

  test('Enter in the editor textarea does NOT open the Variable Editor', async ({ ide, page }) => {
    await seedWorkspaceVar(ide);
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

  test('workspace card has no persistent selection state', async ({ ide }) => {
    await seedWorkspaceVar(ide);
    const card = ide.workspaceCards.filter({ hasText: 'xtest' });
    // Pre-7fb111d3 the card got class .is-selected on click and kept
    // it. We removed both the state and the .is-selected style; the
    // class should never appear on hover.
    await card.hover();
    const cls = await card.getAttribute('class');
    expect(cls).not.toMatch(/is-selected/);
  });
});
