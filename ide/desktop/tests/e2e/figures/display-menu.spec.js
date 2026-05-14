// display-menu.spec.js — exercise the toolbar popovers that host
// grid / scale / title / xlabel / ylabel / zlabel / legend toggles.
// Two buttons since the Apr-2026 split: axes ▾ (Visible / Box / Dir /
// Scale) and decoration ▾ (grid / labels / legend / colorbar).

import { test, expect } from '../../helpers/shared.js';

async function openDecorationMenu(page) {
  // decoration ▾ — grid / labels / legend / colorbar. Regex without ^
  // anchor (button text starts with whitespace around the SVG icon).
  await page.locator('.fw-toolbar .ve-btn', { hasText: /decoration/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

async function openAxesMenu(page) {
  await page.locator('.fw-toolbar .ve-btn', { hasText: /axes/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

test.describe('display ▾ menu — toggle visibility', () => {
  test('axes ▾ and decoration ▾ buttons always present (incl. for subplot)', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'figure;\n'
      + 'subplot(1,2,1); plot(1:10);\n'
      + 'subplot(1,2,2); plot(1:5);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await expect(page.locator('.fw-toolbar .ve-btn',
      { hasText: /axes/i })).toBeVisible({ timeout: 2_000 });
    await expect(page.locator('.fw-toolbar .ve-btn',
      { hasText: /decoration/i })).toBeVisible({ timeout: 2_000 });
  });

  test('toggle title hides the figure title text in the SVG', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot(1:10);\n'
      + 'title("Hello visibility");\n'
      + 'xlabel("X axis"); ylabel("Y axis");\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    // Title text is in the SVG before toggling.
    const titleLoc = page.locator('.fw-window svg text', { hasText: 'Hello visibility' });
    await expect(titleLoc).toBeVisible({ timeout: 2_000 });

    await openDecorationMenu(page);
    await page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'title' }) }).click();

    // Title gone after toggle.
    await expect(titleLoc).toHaveCount(0, { timeout: 2_000 });

    // Re-toggle restores it.
    await page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'title' }) }).click();
    await expect(titleLoc).toBeVisible({ timeout: 2_000 });
  });

  test('xlabel toggle hides only x-axis label, not y', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot(1:10);\n'
      + 'xlabel("the x"); ylabel("the y");\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    await openDecorationMenu(page);
    await page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'X label' }) }).click();

    await expect(page.locator('.fw-window svg text', { hasText: 'the x' })).toHaveCount(0);
    await expect(page.locator('.fw-window svg text', { hasText: 'the y' })).toBeVisible();
  });

  test('grid toggle removes major-grid lines', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot(1:10);\n'
      + 'grid on;\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(150);

    const beforeCount = await page.locator('.fw-window svg line[stroke*="--plot-grid"]').count();
    expect(beforeCount).toBeGreaterThan(0);

    // XGrid / YGrid are properties of the HG2 Axes object → axes ▾.
    await openAxesMenu(page);
    await page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: /^grid$/ }) }).click();
    await page.waitForTimeout(100);

    const afterCount = await page.locator('.fw-window svg line[stroke*="--plot-grid"]').count();
    expect(afterCount).toBe(0);
  });

  test('Z log and zlabel stay enabled even on 2-D figures', async ({ ide, page }) => {
    // Per latest UX spec: toolbar doesn't gate Z controls behind a
    // 3-D check. Toggling them on a 2-D figure is a no-op, but the
    // buttons are clickable. After the axes/decoration split: Z log
    // lives in axes ▾ (scale section), zlabel lives in decoration ▾
    // (labels section).
    await ide.runScript(
      'import compat.*;\n'
      + 'plot(1:10);\n'
      + 'title("2D"); xlabel("x"); ylabel("y");\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    await openAxesMenu(page);
    const zlog = page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'Z log' }) });
    await expect(zlog).toBeEnabled();
    // Close axes ▾ by clicking its trigger again, then open decoration ▾.
    await page.locator('.fw-toolbar .ve-btn', { hasText: /axes/i }).click();
    await page.waitForTimeout(50);

    await openDecorationMenu(page);
    const zlabel = page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'Z label' }) });
    await expect(zlabel).toBeEnabled();
  });

  test('xlabel ✓ off when script never set xlabel; on when set', async ({ ide, page }) => {
    // Script with no xlabel — toolbar xlabel ✓ should be empty.
    await ide.runScript('import compat.*;\nplot(1:10);\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await openDecorationMenu(page);
    const xlabelToggle = page.locator('.fw-pop-toggle',
      { has: page.locator('span', { hasText: 'X label' }) });
    expect((await xlabelToggle.locator('.fw-pop-check').textContent()).trim()).toBe('');
  });

  test('xlabel ✓ on when script DID call xlabel', async ({ ide, page }) => {
    await ide.runScript('import compat.*;\nplot(1:10);\nxlabel("x axis");\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await openDecorationMenu(page);
    const xlabelToggle = page.locator('.fw-pop-toggle',
      { has: page.locator('span', { hasText: 'X label' }) });
    expect((await xlabelToggle.locator('.fw-pop-check').textContent()).trim()).toBe('✓');
  });

  test('toolbar axes ▾ / decoration ▾ — no toggle is ever disabled', async ({ ide, page }) => {
    // Cover the worst-case figure that previously tripped disabled rules:
    // bare `plot(1:10)` (no title, no xlabel, no ylabel, xRange[0] < 0
    // due to padding so xlog used to be disabled). After the
    // disabled-rule deletion every toggle should be clickable. Walk
    // BOTH popovers since the split moved scale toggles to axes ▾.
    await ide.runScript('import compat.*;\nplot(1:10);\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    for (const [open, label] of [[openAxesMenu, /axes/i],
                                  [openDecorationMenu, /decoration/i]]) {
      await open(page);
      const buttons = await page.locator('.fw-pop .fw-pop-toggle').all();
      expect(buttons.length).toBeGreaterThan(0);
      for (const btn of buttons) {
        await expect(btn).toBeEnabled();
      }
      // Toggle the trigger to close the popover (Escape would close the
      // whole modal — onKey listener in FigureWindow).
      await page.locator('.fw-toolbar .ve-btn', { hasText: label }).click();
      await page.waitForTimeout(50);
    }
  });

  test('title and xlabel toggles stay enabled even when text is unset', async ({ ide, page }) => {
    // Latest UX rule: toolbar display ▾ NEVER disables anything — the
    // toolbar is a figure-wide brush. Toggling a label that was never
    // set is a no-op visually but the cell-state flag still flips.
    await ide.runScript(
      'import compat.*;\n'
      + 'figure;\n'
      + 'plot(1:10);\n'
      + 'title(""); xlabel(""); ylabel("");\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    await openDecorationMenu(page);
    const titleBtn = page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'title' }) });
    const xlabelBtn = page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'X label' }) });
    await expect(titleBtn).toBeEnabled();
    await expect(xlabelBtn).toBeEnabled();
  });

  test('subplot — title toggle applies to ALL cells', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'figure;\n'
      + 'subplot(1,2,1); plot(1:10); title("Cell A");\n'
      + 'subplot(1,2,2); plot(1:5);  title("Cell B");\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    // Both titles visible.
    await expect(page.locator('.fw-window svg text', { hasText: 'Cell A' })).toBeVisible();
    await expect(page.locator('.fw-window svg text', { hasText: 'Cell B' })).toBeVisible();

    await openDecorationMenu(page);
    await page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'title' }) }).click();
    await page.waitForTimeout(100);

    // Both gone.
    await expect(page.locator('.fw-window svg text', { hasText: 'Cell A' })).toHaveCount(0);
    await expect(page.locator('.fw-window svg text', { hasText: 'Cell B' })).toHaveCount(0);
  });
});
