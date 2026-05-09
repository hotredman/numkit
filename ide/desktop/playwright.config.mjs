// playwright.config.js — config for the Numkit IDE Electron e2e tests.
//
// Single worker on purpose: each test launches a real Electron process,
// owns a renderer window, talks to a real WASM engine. Parallel runs
// would just contend for CPU and tank reliability without making
// anything faster on a typical dev box.
//
// Tests live in tests/e2e/. Helpers (Electron launcher, page object)
// in tests/helpers/. Reports + traces in test-results/ which is
// gitignored.

import { defineConfig } from '@playwright/test';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

export default defineConfig({
  testDir: path.join(__dirname, 'tests', 'e2e'),

  // Each test gets up to 60 s. The slowest path is the WASM module
  // download + repl_init + first listTree — ~3 s on cold disk. Memory
  // tests that idle on purpose tolerate longer per-test timeouts via
  // test.setTimeout(...) inside the test body.
  timeout: 60_000,

  // No retries — flake is a real bug we want to catch, not paper over.
  retries: 0,

  // Sequential. Each test owns an Electron process; parallelism here
  // doesn't pay back for a single dev machine.
  workers: 1,
  fullyParallel: false,

  reporter: [['list']],

  use: {
    // Capture trace + screenshot on failure so a CI run can be
    // diagnosed post-hoc without re-running. The trace viewer lives
    // at https://trace.playwright.dev.
    trace: 'retain-on-failure',
    screenshot: 'only-on-failure',
    video: 'off',
  },

  outputDir: path.join(__dirname, 'test-results'),
});
