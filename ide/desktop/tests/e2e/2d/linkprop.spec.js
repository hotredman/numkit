// linkprop.spec.js — accept-stub for `linkprop` / `linkdata`. The
// real synchronised state is BACKLOG; the v1 contract is just
// "scripts that store the return handle don't break."

import { test, expect } from '../../helpers/shared.js';

test.describe('linkprop — handle-based property linking (stub)', () => {
  test('linkprop returns an opaque scalar handle, no crash', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'h = linkprop([1 2 3], \'CameraPosition\');\n'
      + 'fprintf(\'h class: %s\\n\', class(h));\n'
    );
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('linkdata accepted similarly', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'linkdata(\'on\');\n'
      + 'fprintf(\'survived\\n\');\n'
    );
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
