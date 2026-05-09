// memory.spec.js — bounds on Tab process memory + on the in-renderer
// state arrays whose growth caused the OOM crashes earlier on this
// branch. Reads native memory via app.getAppMetrics() (main process
// API) so we see ALL of the renderer, not just V8 JS heap.
//
// Numbers (workingSet < 350 MB after boot, drift < 80 MB on idle)
// are intentionally generous: the goal is to catch the class of bug
// that grew the renderer to multi-GB, not to police GC noise.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';
import { tabMemory, describeMetrics } from '../helpers/metrics.js';

test.describe('memory regressions', () => {
  test('Tab process is bounded after boot', async () => {
    const app = await launchIde();
    const page = await app.firstWindow();
    const ide = new IdePage(page);
    await ide.waitForReady();

    const m = await tabMemory(app);
    // Round-numbered ceiling: anything > 350 MB right after boot is
    // a smell. The pre-fix bug pushed us to 4 GB.
    expect(m.workingSet, `Tab WS=${m.workingSet}MB priv=${m.private}MB at boot — ${await describeMetrics(app)}`)
      .toBeLessThan(350);

    await closeIde(app);
  });

  test('Tab process does not drift on idle', async () => {
    const app = await launchIde();
    const page = await app.firstWindow();
    const ide = new IdePage(page);
    await ide.waitForReady();

    const baseline = await tabMemory(app);

    // Sit idle. Pre-fix we observed ~130 MB/sec growth — even 5 s
    // would catch it. 15 s gives slow leaks somewhere to show too.
    await page.waitForTimeout(15_000);

    const after = await tabMemory(app);
    const delta = after.workingSet - baseline.workingSet;
    expect(delta, `idle drift ${delta}MB (${baseline.workingSet} → ${after.workingSet})`)
      .toBeLessThan(80);

    await closeIde(app);
  });

  test('long script output respects OUTPUT_CAP (5000 lines)', async () => {
    const app = await launchIde();
    const page = await app.firstWindow();
    const ide = new IdePage(page);
    await ide.waitForReady();

    // Script emits 8000 disp lines; the in-renderer cap is 5000 so
    // the console pane DOM should never have more than that.
    await ide.runScript(
      'import compat.*;\n'
      + 'for k = 1:8000; disp(k); end\n'
    );
    // Give React time to flush after the run.
    await page.waitForTimeout(500);

    const lineCount = await page.locator('.con-line').count();
    expect(lineCount, `console rendered ${lineCount} lines`)
      .toBeLessThanOrEqual(5050);  // cap + small banner overhead

    // Memory should still be sane after the noisy run.
    const m = await tabMemory(app);
    expect(m.workingSet).toBeLessThan(500);

    await closeIde(app);
  });

  test('many figures respect FIGURE_CAP (50)', async () => {
    const app = await launchIde();
    const page = await app.firstWindow();
    const ide = new IdePage(page);
    await ide.waitForReady();

    // 60 figures → cap=50 should drop the oldest 10. The IDE.jsx
    // setFigures path warns to the DevTools console; we don't
    // assert on the warning text (it's diagnostic, not contract),
    // we just check the count.
    await ide.runScript(
      'import compat.*;\n'
      + 'for k = 1:60; figure; plot(1:5); end\n'
    );
    await page.waitForTimeout(500);

    const cards = await ide.figureCards.count();
    expect(cards, `${cards} figure cards rendered`).toBeLessThanOrEqual(50);

    await closeIde(app);
  });
});
