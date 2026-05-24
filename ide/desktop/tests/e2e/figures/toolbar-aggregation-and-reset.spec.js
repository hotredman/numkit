// toolbar-aggregation-and-reset.spec.js
// 1. Toolbar display ▾ ✓ shows only when EVERY cell has the option set
//    (after applying per-cell ПКМ overrides). Toolbar colormap ▾ same.
// 2. Reset row is at the TOP of every popover.
// 3. ПКМ Display submenu and Colormap submenu both have a top reset row;
//    in subplot mode it clears only this cell's override.

import { test, expect } from '../../helpers/shared.js';

async function rightClickCell(page, idx) {
  const svg = page.locator('.fw-window .fw-canvas-wrap svg').nth(idx);
  const box = await svg.boundingBox();
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down({ button: 'right' });
  await page.mouse.up({ button: 'right' });
  await expect(page.locator('.ctx-menu')).toBeVisible({ timeout: 2_000 });
}

async function openToolbarMenu(page, label) {
  await page.locator('.fw-toolbar .ve-btn', { hasText: new RegExp(label, 'i') }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

test('toolbar `default` rows are at TOP of popovers (axes ▾, grid ▾, decoration ▾, colormap ▾)', async ({ ide, page }) => {
  await ide.runScript('import compat.*;\nimagesc(rand(8,8));\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(100);

  for (const name of ['axes', 'grid', 'decoration', 'colormap']) {
    await openToolbarMenu(page, name);
    const first = page.locator('.fw-pop > .fw-pop-section').first()
      .locator('button').first();
    await expect(first, `${name} ▾ first row should be "default"`).toContainText(/^default$/);
    // Close before opening the next one — Escape would dismiss the modal.
    await page.locator('.fw-toolbar .ve-btn', { hasText: new RegExp(name, 'i') }).click();
    await page.waitForTimeout(50);
  }
});

test('ПКМ Axes ▶ and Decoration ▶ each have `default` row at TOP', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'subplot(1,2,1); plot(1:10);\n'
    + 'subplot(1,2,2); plot(1:10);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(100);

  await rightClickCell(page, 0);

  // Each ПКМ submenu starts with a `default` row. After the
  // grid-out-of-axes split there are three: Axes ▶, Grid ▶,
  // Decoration ▶. Hover-switch through them all.
  for (const trigger of [/Axes/, /Grid/, /Decoration/]) {
    await page.locator('.ctx-sub-trigger', { hasText: trigger }).hover();
    await page.waitForTimeout(80);
    await expect(page.locator('.ctx-submenu > .ctx-item').first()).toContainText(/^default$/);
  }
});

test('ПКМ Colormap submenu has `default` row at TOP', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'subplot(1,2,1); imagesc(rand(8,8));\n'
    + 'subplot(1,2,2); imagesc(rand(8,8));\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(100);

  await rightClickCell(page, 0);
  await page.locator('.ctx-sub-trigger', { hasText: /Colormap/ }).hover();
  await page.waitForTimeout(60);
  const firstBtn = page.locator('.ctx-submenu > .ctx-item').first();
  await expect(firstBtn).toContainText(/^default$/);
});

test('toolbar grid ▾ ✓ aggregates: only set when ALL cells have it', async ({ ide, page }) => {
  // XGrid is an Axes property; the toolbar master row for grid lives
  // in the dedicated `grid ▾` popover (split from axes ▾). ПКМ still
  // exposes `grid` inside Grid ▶ as a per-cell toggle.
  // After the matrix-layout refactor: the master `all` row carries
  // two state buttons (major/minor); we drive the major button [0]
  // and read its `is-active` class for ✓-equivalent state.
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

  // Master `all` row's MAJOR button (column 0) drives the figure-wide
  // major-grid bit; we read `is-active` on it as ✓-equivalent.
  const allMajorBtn = () => page.locator('.fw-pop-matrix .fw-pop-mrow', {
    has: page.locator('.fw-pop-mrow-label', { hasText: /^all$/ }),
  }).locator('.fw-pop-mbtn').nth(0);
  const isActive = async (btn) => /\bis-active\b/.test((await btn.getAttribute('class')) || '');

  // ПКМ Grid ▶ → `all` row's MAJOR button. The submenu uses the same
  // matrix layout: row[has .ctx-name='all'] → .ctx-row-btn nth(0).
  const pkmAllRow = () => page.locator('.ctx-submenu .ctx-row', {
    has: page.locator('.ctx-name', { hasText: /^all$/ }),
  });
  const pkmAllMajor = () => pkmAllRow().locator('.ctx-row-btn').nth(0);

  // Pre: nothing on. Master row's major button must NOT be active.
  await openToolbarMenu(page, 'grid');
  expect(await isActive(allMajorBtn()),
    'master `all` major should start inactive').toBe(false);
  await page.locator('.fw-toolbar .ve-btn', { hasText: /grid/i }).click();  // close
  await page.waitForTimeout(50);

  // Toggle grid on cell A only via ПКМ Grid ▶ (per-cell).
  await rightClickCell(page, 0);
  await page.locator('.ctx-sub-trigger', { hasText: /Grid/ }).hover();
  await page.waitForTimeout(60);
  await pkmAllMajor().click();
  await page.waitForTimeout(120);

  // Toolbar grid ▾ → master `all` major still inactive (only one cell
  // on, not ALL).
  await openToolbarMenu(page, 'grid');
  expect(await isActive(allMajorBtn()),
    'partial-state should keep master inactive (1 of 2 cells)').toBe(false);
  await page.locator('.fw-toolbar .ve-btn', { hasText: /grid/i }).click();
  await page.waitForTimeout(50);

  // Now toggle grid on cell B too.
  await rightClickCell(page, 1);
  await page.locator('.ctx-sub-trigger', { hasText: /Grid/ }).hover();
  await page.waitForTimeout(60);
  await pkmAllMajor().click();
  await page.waitForTimeout(120);

  // Now master shows active.
  await openToolbarMenu(page, 'grid');
  expect(await isActive(allMajorBtn()),
    'all-on master should be active (both cells on)').toBe(true);
});

test('toolbar colormap ▾ ✓ aggregates: only set when ALL cells use that palette', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'subplot(1,2,1); imagesc(rand(8,8));\n'
    + 'subplot(1,2,2); imagesc(rand(8,8));\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(120);

  // Default: both parula → ✓ on parula, no ✓ on jet.
  await openToolbarMenu(page, 'colormap');
  const parulaCheck = await page.locator('.fw-pop button',
    { has: page.locator('span', { hasText: /^parula$/ }) }).first()
    .locator('.fw-pop-check').textContent();
  expect(parulaCheck.trim()).toBe('✓');
  await page.locator('.fw-toolbar .ve-btn', { hasText: /^colormap/i }).click();  // close
  await page.waitForTimeout(50);

  // Per-cell A → jet via ПКМ.
  await rightClickCell(page, 0);
  await page.locator('.ctx-sub-trigger', { hasText: /Colormap/ }).hover();
  await page.waitForTimeout(60);
  await page.locator('.ctx-submenu button', { hasText: /^(✓ )?jet$/ }).click();
  await page.waitForTimeout(120);

  // Now NEITHER parula nor jet has ✓ in toolbar (mixed).
  await openToolbarMenu(page, 'colormap');
  const parulaMix = await page.locator('.fw-pop button',
    { has: page.locator('span', { hasText: /^parula$/ }) }).first()
    .locator('.fw-pop-check').textContent();
  const jetMix = await page.locator('.fw-pop button',
    { has: page.locator('span', { hasText: /^jet$/ }) }).first()
    .locator('.fw-pop-check').textContent();
  expect(parulaMix.trim(), `parula in mixed: ${parulaMix}`).toBe('');
  expect(jetMix.trim(), `jet in mixed: ${jetMix}`).toBe('');
});
