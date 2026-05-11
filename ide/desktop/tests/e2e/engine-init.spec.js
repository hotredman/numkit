// engine-init.spec.js — guards the WASM engine boots cleanly. Two
// classes of failure used to slip through silently:
//
//   1. Library install() throws (e.g. duplicate compat function
//      registration) → JS catches with `err.message === undefined`
//      because Emscripten's binding bridge drops std::exception::what().
//      App.jsx falls back to the JS interpreter; user sees [fallback]
//      output but no clear reason. This was the round-3 plot3 / pcolor
//      class of bug.
//
//   2. WASM glue loads but binding lookup returns undefined → engine
//      object is half-functional, behaviour is silently weird.
//
// We assert via [engine] WASM bindings that ALL expected entry points
// are typed `function`, AND that no `[REPL] Using fallback engine`
// log line appears during boot (which is the canonical signal that
// engine init failed for any reason).

// Boot-log assertions need a fresh launch per test — the shared fixture
// clears devMessages between tests, so these specs use the legacy
// launchIde / closeIde path explicitly.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('engine init', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('WASM engine is active (not fallback)', async () => {
    const dev = ide.devLogs();
    expect(dev, 'WASM init failed and IDE fell back to JS interpreter')
      .not.toMatch(/\[REPL\] Using fallback engine/);
  });

  test('no [repl_init] FATAL message in stderr', async () => {
    const dev = ide.devLogs();
    expect(dev).not.toMatch(/\[repl_init\] FATAL/);
  });

  test('all expected WASM bindings resolve', async () => {
    const dev = ide.devLogs();
    expect(dev).toMatch(/\[engine\] WASM bindings/);
    expect(dev).not.toMatch(/repl_init: undefined/);
    expect(dev).not.toMatch(/repl_execute: undefined/);
    expect(dev).not.toMatch(/repl_register_fs: undefined/);
  });
});
