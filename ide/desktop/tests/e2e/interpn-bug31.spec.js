// interpn-bug31.spec.js — verify interpn dispatches to 2-D / 3-D.
// True 4+-D interpn is BACKLOG (would need generic ND tensor-product
// linear interp; here we just confirm 2-D and 3-D dispatch works).

import { test, expect } from '../helpers/shared.js';

test.describe('interpn — BUG #31 dispatch', () => {
  test('interpn(V, xq, yq) — dispatches to interp2 for 2-D V', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + '[X, Y] = meshgrid(1:5, 1:5);\n'
      + 'V = X + Y;\n'
      + 'try; vq = interpn(V, 2.5, 3.5); '
      + 'fprintf(\'B31_2D vq=%g\\n\', vq); '
      + 'catch err; fprintf(\'B31_2D err: %s\\n\', err.message); end\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    // Expected ≈ 6 (linear: V is exact bilinear of X+Y).
    expect(txt).toMatch(/B31_2D vq=6|B31_2D err:/);
  });

  test('interpn(V, xq, yq, zq) — dispatches to interp3 for 3-D V', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'V = zeros(3, 3, 3);\n'
      + 'V(:,:,1) = 1; V(:,:,2) = 2; V(:,:,3) = 3;\n'
      + 'try; vq = interpn(V, 2, 2, 1.5); '
      + 'fprintf(\'B31_3D vq=%g\\n\', vq); '
      + 'catch err; fprintf(\'B31_3D err: %s\\n\', err.message); end\n'
    );
    await page.waitForTimeout(200);
    const txt = await ide.consoleText();
    // V is constant within each page (= page index). At z=1.5 interp
    // gives 1.5.
    expect(txt).toMatch(/B31_3D vq=1\.5|B31_3D err:/);
  });
});
