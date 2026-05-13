// var-editor-divider.spec.js — drag the divider between the matrix
// table and the inline plot in the Variable Editor modal. Asserts:
//   - divider element exists when plot pane is open
//   - dragging it left widens the plot pane and shrinks the table
//   - double-click resets to default 520px

import { test, expect } from '../../helpers/shared.js';

async function openMatrixVar(ide, page) {
  await ide.runScript(
    'import compat.*;\n'
    + 'A = reshape(1:64, 8, 8);\n'
  );
  // Switch to Workspace tab so the variable card is reachable.
  await ide.openWorkspaceTab();
  // Click the var card to open the editor.
  await page.locator('.var-card', { hasText: 'A' }).first().click();
  await expect(page.locator('.ve-overlay')).toBeVisible({ timeout: 5_000 });
  // Toggle the inline plot pane on. Plot button is in the toolbar
  // — its text is "plot" (or has-plot class on body afterwards).
  const plotBtn = page.locator('.ve-overlay .ve-tools-group button', { hasText: /^plot$/i }).first();
  if (await plotBtn.count() > 0) await plotBtn.click();
  await expect(page.locator('.ve-body.has-plot')).toBeVisible({ timeout: 2_000 });
}

async function gridCols(page) {
  return await page.locator('.ve-body.has-plot').first().evaluate((el) =>
    getComputedStyle(el).gridTemplateColumns);
}

test('var editor divider — drag widens plot pane', async ({ ide, page }) => {
  await openMatrixVar(ide, page);

  const divider = page.locator('.ve-divider');
  await expect(divider).toBeVisible();
  const before = await gridCols(page);
  // grid cols is e.g. "640px 6px 520px" — record so we can compare.

  const box = await divider.boundingBox();
  // Drag 100 px to the LEFT to make the plot wider.
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down();
  await page.mouse.move(box.x + box.width / 2 - 100, box.y + box.height / 2,
                        { steps: 10 });
  await page.mouse.up();
  await page.waitForTimeout(60);

  const after = await gridCols(page);
  // Plot column (the third track) should be wider after the drag.
  const plotPxBefore = parseFloat(before.split(' ').pop());
  const plotPxAfter  = parseFloat(after.split(' ').pop());
  expect(plotPxAfter, `before=${before} after=${after}`).toBeGreaterThan(plotPxBefore + 50);
});

test('var editor divider — double-click resets to default', async ({ ide, page }) => {
  await openMatrixVar(ide, page);

  const divider = page.locator('.ve-divider');
  // First drag to a non-default size.
  const box = await divider.boundingBox();
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down();
  await page.mouse.move(box.x + box.width / 2 - 80, box.y + box.height / 2,
                        { steps: 8 });
  await page.mouse.up();
  await page.waitForTimeout(60);

  // Double-click resets.
  await divider.dblclick();
  await page.waitForTimeout(60);
  const after = await gridCols(page);
  const plotPxAfter = parseFloat(after.split(' ').pop());
  expect(plotPxAfter).toBe(520);
});
