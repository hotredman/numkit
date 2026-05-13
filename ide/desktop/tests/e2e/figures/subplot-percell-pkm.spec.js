// subplot-percell-pkm.spec.js — ПКМ Display toggles inside a subplot
// cell affect ONLY that cell. Toolbar Reset clears every per-cell
// override so the grid follows the global state again.

import { test, expect } from '../../helpers/shared.js';

async function rightClickCellSvg(page, cellIdx) {
  // The subplot SVGs render in document order matching cell index.
  const svg = page.locator('.fw-window .fw-canvas-wrap svg').nth(cellIdx);
  const box = await svg.boundingBox();
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down({ button: 'right' });
  await page.mouse.up({ button: 'right' });
  await expect(page.locator('.ctx-menu')).toBeVisible({ timeout: 2_000 });
}

async function gridLineCount(page, cellIdx) {
  // Major grid lines have stroke linking to var(--plot-grid). Count per cell.
  return await page.locator('.fw-window .fw-canvas-wrap svg').nth(cellIdx)
    .locator('line[stroke*="--plot-grid"]:not([stroke*="--plot-grid-min"])').count();
}

test('ПКМ grid toggle in cell A does not enable grid in cell B', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'subplot(1,2,1); plot(1:10);\n'
    + 'subplot(1,2,2); plot(1:10);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(120);

  // Pre: both cells have NO grid (default).
  const beforeA = await gridLineCount(page, 0);
  const beforeB = await gridLineCount(page, 1);
  expect(beforeA).toBe(0);
  expect(beforeB).toBe(0);

  // Right-click cell A (index 0), open Display submenu, click 'grid'.
  await rightClickCellSvg(page, 0);
  await page.locator('.ctx-sub-trigger', { hasText: /Display/ }).hover();
  await page.waitForTimeout(60);
  await page.locator('.ctx-submenu button', { hasText: /^(✓ )?grid$/ }).click();
  await page.waitForTimeout(120);

  // After: cell A has grid lines, cell B still has none.
  const afterA = await gridLineCount(page, 0);
  const afterB = await gridLineCount(page, 1);
  expect(afterA, `cell A grid count after toggle: ${afterA}`).toBeGreaterThan(0);
  expect(afterB, `cell B grid count must stay 0, got ${afterB}`).toBe(0);
});

test('toolbar Reset clears per-cell overrides', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'subplot(1,2,1); plot(1:10);\n'
    + 'subplot(1,2,2); plot(1:10);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(120);

  // Toggle grid on for cell A only.
  await rightClickCellSvg(page, 0);
  await page.locator('.ctx-sub-trigger', { hasText: /Display/ }).hover();
  await page.waitForTimeout(60);
  await page.locator('.ctx-submenu button', { hasText: /^(✓ )?grid$/ }).click();
  await page.waitForTimeout(120);
  expect(await gridLineCount(page, 0)).toBeGreaterThan(0);

  // Now click toolbar Reset → cell A grid override is cleared, both
  // cells back to figure-wide value (off).
  await page.locator('.fw-toolbar [data-fw-reset="all"]').click();
  await page.waitForTimeout(120);

  expect(await gridLineCount(page, 0)).toBe(0);
  expect(await gridLineCount(page, 1)).toBe(0);
});
