// figures.spec.js — Figures pane regression tests:
//
//   1. Running a script that creates a figure shows a card in the pane
//   2. Clicking a card opens the FigureWindow modal (whole card is the
//      button — title bar + body)
//   3. The × icon closes the figure (consumes engine result so the
//      card disappears) — bug fix bf8c587e
//   4. Hover style applies (border-color → accent) — fix 5b811000
//   5. The expand-icon button is gone (61d71d05)

import { test, expect } from '../../helpers/shared.js';

// Seed helper — every test in this describe needs one figure card.
// Shared fixture resets engine state before each test, so seed inline.
async function seedOneFigure(ide) {
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'plot(1:10, (1:10).^2);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
}

test.describe('Figures pane', () => {
  test('clicking the card body opens the figure window', async ({ ide }) => {
    await seedOneFigure(ide);
    await ide.figureCards.first().locator('.fp-card-body').click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  });

  test('clicking the card title bar (not on a button) opens the figure window', async ({ ide }) => {
    await seedOneFigure(ide);
    // After 5b811000 the whole card is clickable, including the head
    // bar. We click the title text specifically — anywhere outside
    // the × button should open.
    await ide.figureCards.first().locator('.fp-card-title').click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  });

  test('× button removes the card and does not open the figure window', async ({ ide }) => {
    await seedOneFigure(ide);
    const closeX = ide.figureCards.first().locator('button[title="Close"]');
    await closeX.click();
    // Card gone — fix bf8c587e: handlers consume result.closedFigureIds
    await expect(ide.figureCards).toHaveCount(0, { timeout: 5_000 });
    // × stops propagation — modal must NOT have opened.
    await expect(ide.figureWindow).not.toBeVisible();
  });

  test('redundant expand-icon button was removed (only × in title bar)', async ({ ide }) => {
    await seedOneFigure(ide);
    // Pre-61d71d05 the title bar had two icons (expand + close); the
    // expand-icon was redundant once the whole card became a button.
    const icons = ide.figureCards.first().locator('.fp-card-icon');
    await expect(icons).toHaveCount(1);
    await expect(icons.first()).toHaveAttribute('title', 'Close');
  });

  test('Close all clears every figure card', async ({ ide }) => {
    await seedOneFigure(ide);
    // Add a second figure so "all" is meaningful.
    await ide.runScript('figure; plot(1:5);');
    await expect(ide.figureCards).toHaveCount(2, { timeout: 5_000 });
    await ide.figureCloseAll.click();
    await expect(ide.figureCards).toHaveCount(0, { timeout: 5_000 });
  });
});
