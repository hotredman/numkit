// webgl-3d-lighting.spec.js — Etap 4 lighting / material / camlight /
// surfl. Replaces noop builtins; render path picks the matching
// THREE material and toggles the cam-attached light.

import { test, expect } from '../../helpers/shared.js';

test.describe('WebGL 3-D — lighting / material / camlight / surfl', () => {
  test.describe('lighting modes', () => {
    for (const mode of ['flat', 'gouraud', 'phong', 'none']) {
      test(`lighting('${mode}') — surf renders without errors`, async ({ ide, page }) => {
        await ide.runScript(
          'import compat.*;\n'
          + 'surf([1 2 3; 2 3 4; 3 4 5]);\n'
          + `lighting('${mode}');\n`
        );
        await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
        await expect(ide.figureCards.first().locator('canvas[data-numkit-3d]'))
          .toBeVisible({ timeout: 10_000 });
        expect(ide.devErrors().filter((e) =>
          !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
        )).toEqual([]);
      });
    }
  });

  test.describe('material presets (with phong)', () => {
    for (const m of ['shiny', 'metal', 'dull']) {
      test(`material('${m}') — phong + ${m} renders`, async ({ ide, page }) => {
        await ide.runScript(
          'import compat.*;\n'
          + 'surf([1 2 3; 2 3 4; 3 4 5]);\n'
          + 'lighting(\'phong\');\n'
          + `material('${m}');\n`
        );
        await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
        expect(ide.devErrors().filter((e) =>
          !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
        )).toEqual([]);
      });
    }
  });

  test.describe('camlight positions', () => {
    for (const pos of ['left', 'right', 'headlight']) {
      test(`camlight('${pos}') — adds a directional light`, async ({ ide, page }) => {
        await ide.runScript(
          'import compat.*;\n'
          + 'surf([1 2 3; 2 3 4; 3 4 5]);\n'
          + `camlight('${pos}');\n`
        );
        await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
        expect(ide.devErrors().filter((e) =>
          !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
        )).toEqual([]);
      });
    }
  });

  test('camlight() default — headlight applied', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'surf([1 2 3; 2 3 4; 3 4 5]);\n'
      + 'camlight();\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('surfl(Z) — surf + auto camlight + lighting gouraud', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'surfl([1 2 3 4; 2 3 4 5; 3 4 5 6]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await expect(ide.figureCards.first().locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
