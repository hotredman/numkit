// t1b-stats.spec.js — Tier-1 batch B: cdfplot/ecdf, qqplot, pareto,
// histfit, gscatter. All wrap existing layer kinds.
//
// Tests assert: builtin runs, figure card appears, no console errors.

import { test, expect } from '../../helpers/shared.js';

test.describe('Tier 1B — stat-chart wrappers', () => {
  test('cdfplot(x) — empirical CDF as a step function', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'cdfplot(randn(1, 100));\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  // `ecdf` itself is provided by libs/stats as a computational
  // routine that returns (F, x) — it doesn't draw. Users plot the
  // empirical CDF via cdfplot(x). No graphics-side wrapper needed.

  test('qqplot(x) — normal Q-Q with reference line', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'qqplot(randn(1, 50));\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // qqplot emits 1 line (reference) + 1 scatter (points). Should
    // produce both a path and circles.
    const paths = await ide.figureCards.first().locator('svg path').count();
    const circles = await ide.figureCards.first().locator('svg circle').count();
    expect(paths).toBeGreaterThanOrEqual(1);
    expect(circles).toBeGreaterThanOrEqual(40);   // ~50 points minus boundary clipping
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('pareto(Y) — sorted bars + cumulative line', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'pareto([4 2 8 1 5 3 7 6]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('histfit(x) — histogram + Gaussian fit overlay', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'histfit(randn(1, 200));\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('histfit(x, n) — explicit bin count', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'histfit(randn(1, 200), 30);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('gscatter(x, y, g) — scatter colored by group', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = [1 2 3 4 5 6 7 8 9 10];\n'
      + 'y = [1 4 9 1 4 9 1 4 9 1];\n'
      + 'g = [1 1 1 2 2 2 3 3 3 1];\n'
      + 'gscatter(x, y, g);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // 3 groups × points → 10 scatter circles total.
    const circles = await ide.figureCards.first().locator('svg circle').count();
    expect(circles).toBe(10);
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('cdfplot survives degenerate single-value input', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'cdfplot([42]);\n'
      + 'fprintf(\'survived\\n\');\n'
    );
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
    // Engine recovers for the next plot.
    await ide.runScript('import compat.*;\nplot([1 2 3], [1 2 3]);\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  });
});
