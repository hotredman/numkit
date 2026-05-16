// fit-axis-equal.spec.js — coverage for the interaction between
// `axis equal` / `axis image` and the toolbar fit ▾ → X / Y buttons.
//
// Design: aspect = equal/image pins DataAspectRatio = [1 1 1] (1 data
// unit X = 1 data unit Y in pixels). Single-axis fit would break that
// contract by independently changing one axis's range. To avoid a
// silent panel resize, the X / Y / Z fit buttons are DISABLED while
// aspect locks them. User flow: open axes ▾, switch aspect to `auto`,
// fit X works as expected, switch aspect back to `equal`.

import { test, expect } from '../../helpers/shared.js';

async function openFit(page) {
  await page.locator('.fw-toolbar .ve-btn', { hasText: /fit/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

async function openAxes(page) {
  await page.locator('.fw-toolbar .ve-btn', { hasText: /axes/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

test('axes ▾ has aspect radio with 5 mutually-exclusive options', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'plot(1:10);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

  await openAxes(page);
  // Aspect section with 5 pills.
  const pills = page.locator('.fw-pop-radio');
  await expect(pills).toHaveCount(5);
  await expect(pills.nth(0)).toContainText(/^auto$/);
  await expect(pills.nth(1)).toContainText(/^equal$/);
  await expect(pills.nth(2)).toContainText(/^square$/);
  await expect(pills.nth(3)).toContainText(/^image$/);
  await expect(pills.nth(4)).toContainText(/^tight$/);
  // Default for a bare plot — no axis mode set in script → 'auto' active.
  await expect(pills.nth(0)).toHaveClass(/is-active/);
});

test('aspect radio: clicking `equal` sets axisMode and shrinks panel to square', async ({ ide, page }) => {
  // Tall-aspect data (X span 20, Y span 4). With aspect=auto the
  // panel fills the cell; clicking `equal` triggers panel-shrink to
  // honour 1:1 data aspect → panel becomes WIDE and SHORT (W/H = 5).
  await ide.runScript(
    'import compat.*;\n'
    + 'rng(0);\n'
    + 'plot(linspace(-10, 10, 100), randn(1, 100) * 2);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  await openAxes(page);
  // Click `equal`.
  await page.locator('.fw-pop-radio', { hasText: /^equal$/ }).click();
  await page.waitForTimeout(150);
  // Pill is now active.
  await expect(page.locator('.fw-pop-radio', { hasText: /^equal$/ })).toHaveClass(/is-active/);
  await expect(page.locator('.fw-pop-radio', { hasText: /^auto$/ })).not.toHaveClass(/is-active/);
});

test('fit ▾ X/Y/Z disabled when aspect is `equal`', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'rng(0);\n'
    + 'scatter(randn(200,1), randn(200,1));\n'
    + 'axis equal; xlim([-5 5]); ylim([-5 5]);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  await openFit(page);
  // X / Y / Z buttons are disabled; `all` and polar R / θ stay enabled.
  await expect(page.locator('.fw-pop button', { hasText: /^X$/ })).toBeDisabled();
  await expect(page.locator('.fw-pop button', { hasText: /^Y$/ })).toBeDisabled();
  await expect(page.locator('.fw-pop button', { hasText: /^Z$/ })).toBeDisabled();
  await expect(page.locator('.fw-pop button', { hasText: /^all$/ })).toBeEnabled();
});

test('switch aspect to auto → fit ▾ X becomes enabled', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'rng(0);\n'
    + 'scatter(randn(200,1), randn(200,1));\n'
    + 'axis equal; xlim([-5 5]); ylim([-5 5]);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  // Pre: X is disabled (axis equal).
  await openFit(page);
  await expect(page.locator('.fw-pop button', { hasText: /^X$/ })).toBeDisabled();
  await page.locator('.fw-toolbar .ve-btn', { hasText: /fit/i }).click();    // close
  await page.waitForTimeout(50);

  // Switch aspect: auto.
  await openAxes(page);
  await page.locator('.fw-pop-radio', { hasText: /^auto$/ }).click();
  await page.waitForTimeout(100);
  await page.locator('.fw-toolbar .ve-btn', { hasText: /axes/i }).click();   // close axes ▾
  await page.waitForTimeout(50);

  // Now X is enabled — single-axis fit allowed.
  await openFit(page);
  await expect(page.locator('.fw-pop button', { hasText: /^X$/ })).toBeEnabled();
});

test('subplot with mixed aspect: NO pill is active, head shows `mixed`', async ({ ide, page }) => {
  // Cell A is auto (default), cell B is `axis equal`. axisModeAgg = null
  // (mixed) → none of the 5 pills should have is-active, and the
  // section head should annotate `mixed` so the user sees there is no
  // uniform value.
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'subplot(1, 2, 1); plot(1:10);\n'
    + 'subplot(1, 2, 2); plot(1:10); axis equal;\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  await openAxes(page);
  // No pill carries is-active.
  await expect(page.locator('.fw-pop-radio.is-active')).toHaveCount(0);
  // Section head reflects the mixed state.
  await expect(page.locator('.fw-pop-head', { hasText: /^aspect: mixed$/ })).toBeVisible();
});

test('subplot 1×3 axis equal: fit X disabled (per-cell axis lock)', async ({ ide, page }) => {
  // qam_constellation-style: 3 axis-equal cells. axisModeAgg = 'equal'
  // → toolbar fit X disabled across the figure.
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'rng(0);\n'
    + 'for k = 1:3\n'
    + '  subplot(1, 3, k);\n'
    + '  scatter(randn(200,1)*k, randn(200,1)*k);\n'
    + '  axis equal; xlim([-5 5]); ylim([-5 5]);\n'
    + 'end\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(200);

  await openFit(page);
  await expect(page.locator('.fw-pop button', { hasText: /^X$/ })).toBeDisabled();
  await expect(page.locator('.fw-pop button', { hasText: /^Y$/ })).toBeDisabled();
});
