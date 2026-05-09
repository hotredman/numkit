# Numkit IDE — Electron e2e tests

Playwright tests that drive the packaged Numkit IDE renderer
through real Electron. Each test owns its own Electron process
with an isolated user-data dir under the OS temp dir, so runs
don't touch your real `%APPDATA%/numkit-ide-desktop` IDE state.

## Layout

```
tests/
├── e2e/
│   ├── smoke.spec.js       — boot path: window, ready state, banner, no errors
│   ├── workspace.spec.js   — single-click opens VE; Enter doesn't leak
│   ├── figures.spec.js     — × closes; whole card opens; expand button gone
│   └── vfs.spec.js         — Examples manifest, file open, tempFS path
├── helpers/
│   ├── launch.js           — launchIde() / closeIde() with isolated userData
│   └── ide.js              — Page Object: locators + actions
├── package.json            — { "type": "module" } scope for ESM-in-tests
└── README.md               (this)
```

## Running

```cmd
cd ide\desktop

REM Pre-req: dist/ must be built (renderer + WASM). Use the
REM project-root build-desktop.bat to be safe; that's what
REM build-desktop.bat does and it's the same dist/ Electron loads.
..\..\build-desktop.bat

REM Then:
npm test                  REM all suites, single-window each, ~25 s
npm run test:headed       REM windows visible (debugging UI selectors)
npm run test:debug        REM Playwright Inspector — step through

REM Run a single suite or test file:
npx playwright test smoke
npx playwright test workspace.spec.js
npx playwright test --grep "single click"
```

## Diagnostics for failures

On any failure, Playwright drops into `test-results/<name>/`:
- `test-failed-1.png`  screenshot at moment of failure
- `trace.zip`          full DOM + network trace
- `error-context.md`   summary

View the trace interactively:
```cmd
npx playwright show-trace test-results\<dir>\trace.zip
```

## Adding new tests

Use the `IdePage` page object from `helpers/ide.js`. Common pattern:

```js
import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('my feature', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('does the thing', async () => {
    await ide.runScript('x = 42');
    // …assertions
  });
});
```

When a CSS class shifts, fix the locator in one place
(`helpers/ide.js`) — every spec picks it up.

## What we DON'T test (yet)

- Memory regressions — needs a longer-running fixture and reading
  `app.getAppMetrics()` from the main process. Worth adding once
  we have a heap-bounded baseline.
- Local Folder mount — needs File System Access permission flow
  or a real folder fixture; current scope is tempFS + Examples.
- Multi-window flows — close window, re-open, restore state.
- Cross-platform — tests run on whatever host runs them; no CI
  matrix yet.
