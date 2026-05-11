// groups.spec.js — findgroups / splitapply / groupcounts.

import { test, expect } from '../../helpers/shared.js';

test.describe('findgroups / splitapply / groupcounts', () => {
  test('findgroups assigns 1-based IDs in sorted-unique order', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + '[G, ID] = findgroups([3 1 2 1 3 2]);\n'
      + 'fprintf(\'G=%d %d %d %d %d %d ID=%d %d %d\\n\', '
      + 'G(1), G(2), G(3), G(4), G(5), G(6), ID(1), ID(2), ID(3));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    // Sorted unique = [1, 2, 3]; group ids: 3→3, 1→1, 2→2, 1→1, 3→3, 2→2
    expect(txt).toMatch(/G=3 1 2 1 3 2 ID=1 2 3/);
  });

  test('splitapply(@sum) per group', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'g = [1 1 2 2 3];\n'
      + 'x = [10 20 30 40 50];\n'
      + 'r = splitapply(@sum, x, g);\n'
      + 'fprintf(\'r=%g %g %g\\n\', r(1), r(2), r(3));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    expect(txt).toMatch(/r=30 70 50/);
  });

  test('groupcounts — count per unique group', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'c = groupcounts([1 2 1 3 2 1]);\n'
      + 'fprintf(\'c=%d %d %d\\n\', c(1), c(2), c(3));\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    // Sorted unique: 1 (3 times), 2 (2 times), 3 (1 time).
    expect(txt).toMatch(/c=3 2 1/);
  });
});
