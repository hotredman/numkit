// helpers/launch.js — uniform Electron launcher for the e2e tests.
//
// Each test that calls launchIde() gets a fresh Electron instance with
// an isolated user-data directory under the OS temp dir. We DO NOT
// touch the user's real %APPDATA%/numkit-ide-desktop store — every
// test starts from a blank IndexedDB tempFS, blank UI persistence
// (numkit.ide.* localStorage keys are scoped to that user-data dir).
//
// On test teardown, closeIde() shuts the app down and rm -rfs the
// temp dir. Failures during teardown are logged but don't fail the
// test (cleanup happens after the assertions ran).

import { _electron } from '@playwright/test';
import path from 'node:path';
import os from 'node:os';
import fs from 'node:fs';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const DESKTOP_ROOT = path.resolve(__dirname, '..', '..');

// dist/index.html must exist for IS_PROD=true in main.js. If it doesn't,
// fail loudly with a readable message — easier than chasing weird
// "white window" symptoms downstream.
function assertDistBuilt() {
  const indexHtml = path.join(DESKTOP_ROOT, 'dist', 'index.html');
  if (!fs.existsSync(indexHtml)) {
    throw new Error(
      `Numkit IDE dist not built: ${indexHtml} missing.\n`
      + `Run \`desktop-build.bat\` from the project root before \`npm test\`.`
    );
  }
}

/**
 * Launch the Electron app with an isolated user-data directory.
 * Returns the ElectronApplication. Pair with closeIde(app).
 */
export async function launchIde(opts = {}) {
  assertDistBuilt();

  const userDataDir = fs.mkdtempSync(path.join(os.tmpdir(), 'numkit-ide-test-'));

  const app = await _electron.launch({
    args: [
      '.',
      // Chromium honours --user-data-dir; Electron's app.getPath('userData')
      // and Chromium's IndexedDB / localStorage all live under it.
      `--user-data-dir=${userDataDir}`,
    ],
    cwd: DESKTOP_ROOT,
    // Forward env so child Electron sees it (used by the file logger to
    // pick a fresh log path inside userDataDir).
    env: { ...process.env, ...(opts.env || {}) },
    timeout: 30_000,
  });

  // Stash for cleanup.
  app.__userDataDir = userDataDir;
  return app;
}

/**
 * Close the app + remove its user-data dir. Idempotent and best-effort:
 * if either step fails (process already gone, dir locked by AV), log
 * and move on so test results don't get a misleading teardown error.
 */
export async function closeIde(app) {
  if (!app) return;
  try { await app.close(); }
  catch (e) { console.warn('[launch] app.close failed:', e.message); }
  if (app.__userDataDir) {
    try { fs.rmSync(app.__userDataDir, { recursive: true, force: true }); }
    catch (e) { console.warn('[launch] userDataDir cleanup failed:', e.message); }
  }
}
