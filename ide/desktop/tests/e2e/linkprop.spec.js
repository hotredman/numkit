// linkprop.spec.js — accept-stub for `linkprop` / `linkdata`. The
// real synchronised state is BACKLOG; the v1 contract is just
// "scripts that store the return handle don't break."

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('linkprop — handle-based property linking (stub)', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('linkprop returns an opaque scalar handle, no crash', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'h = linkprop([1 2 3], \'CameraPosition\');\n'
      + 'fprintf(\'h class: %s\\n\', class(h));\n'
    );
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('linkdata accepted similarly', async () => {
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
