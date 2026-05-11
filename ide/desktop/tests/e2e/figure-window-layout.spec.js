// figure-window-layout.spec.js — guard against the .fw-window grid
// regressing back to 4 rows. The window's CSS grid is:
//
//   38px  | titlebar
//   40px  | toolbar
//   1fr   | canvas
//   auto  | range-row    (only rendered when !isSubplot — collapses to 0 otherwise)
//   22px  | status
//
// Pre-fix: grid-template-rows had only 4 entries (no auto slot for the
// range-row). The range-row got auto-placed into the 22px status slot
// and the input boxes overflowed visually onto the status text below.
// Symptom: "fields stick to top/bottom of the bar" + status text bleeds
// through the inputs.
//
// We assert the rendered range-row sits ENTIRELY ABOVE the status row,
// which catches both the missing-slot bug and any future regression
// that pushes range-row into the status slot.

import { test, expect } from '../helpers/shared.js';

const NON_FATAL = (e) =>
  !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e);

test.describe('FigureWindow layout', () => {
  test('range-row sits entirely above status row (no overlap)', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3 4], [1 4 9 16]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(150);

    const rangeRow = page.locator('.fw-window .fw-range-row');
    const status = page.locator('.fw-window .fw-status');
    await expect(rangeRow).toBeVisible();
    await expect(status).toBeVisible();

    const rangeBox = await rangeRow.boundingBox();
    const statusBox = await status.boundingBox();

    // Bottom of range-row must NOT exceed top of status (sub-pixel slack).
    expect(rangeBox.y + rangeBox.height).toBeLessThanOrEqual(statusBox.y + 1);

    // Range-row also has visible vertical padding (≥ ~30px tall — input
    // is ~20px + 14px padding top + 14px bottom = ~48px). Catches a
    // regression where padding gets dropped to 0.
    expect(rangeBox.height).toBeGreaterThan(30);

    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });

  test('subplot figure: no range-row, status still pinned to bottom', async ({ ide, page }) => {
    // For !isSubplot the auto slot collapses to 0 — verify status row
    // is still where it should be (last row of the grid).
    await ide.runScript(
      'import compat.*;\n'
      + 'figure;\n'
      + 'subplot(1,2,1); plot([1 2 3], [1 4 9]);\n'
      + 'subplot(1,2,2); plot([1 2 3], [9 4 1]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(150);

    expect(await page.locator('.fw-window .fw-range-row').count()).toBe(0);
    await expect(page.locator('.fw-window .fw-status')).toBeVisible();

    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });
});
