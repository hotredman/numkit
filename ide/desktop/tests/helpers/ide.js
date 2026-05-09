// helpers/ide.js — Page Object for the Numkit IDE renderer window.
//
// Wraps the raw Playwright Page with named locators + small action
// helpers so individual specs read as intent, not selector soup. When
// a CSS class name shifts (and they will), we change one place here
// instead of fixing fifty test files.

import { expect } from '@playwright/test';

export class IdePage {
  /** @param {import('playwright').Page} page */
  constructor(page) {
    this.page = page;
    // Capture every renderer-side console message from the moment we
    // attach. Two channels share the term "console" in this codebase:
    //   - DevTools/browser console: `console.log` from JS, captured here
    //   - In-app REPL pane: the `.console` DOM element (use consoleText())
    // Tests typically want devLogs() for engine boot/heap trace lines
    // and consoleText() for what the user would see.
    this.devMessages = [];
    page.on('console', (msg) => {
      this.devMessages.push({ type: msg.type(), text: msg.text() });
    });
    this.pageErrors = [];
    page.on('pageerror', (err) => { this.pageErrors.push(err); });
  }

  /** All renderer console messages joined as one string (for matching). */
  devLogs() {
    return this.devMessages.map((m) => `[${m.type}] ${m.text}`).join('\n');
  }
  /** Just messages of type 'error'. */
  devErrors() {
    return this.devMessages.filter((m) => m.type === 'error').map((m) => m.text);
  }

  /* ── Locators ── */

  get editor()             { return this.page.locator('.editor textarea'); }
  get runButton()          { return this.page.locator('.tool-action.tool-run'); }

  get console()            { return this.page.locator('.console'); }
  get consoleOutput()      { return this.page.locator('.console .console-out'); }
  get consoleInput()       { return this.page.locator('.console .console-input input'); }

  get figuresPane()        { return this.page.locator('.figures'); }
  get figureCards()        { return this.page.locator('.fp-card'); }
  get figureCloseAll()     { return this.page.locator('.fp-closeall'); }

  get workspacePane()      { return this.page.locator('.workspace'); }
  get workspaceCards()     { return this.page.locator('.var-card'); }

  get sidebar()            { return this.page.locator('.editor-tabs').first(); /* tab strip — proxy for sidebar present */ }
  get figureWindow()       { return this.page.locator('.fw-overlay'); }
  get variableEditor()     { return this.page.locator('.ve-overlay'); }

  /**
   * Bottom dock has Console / Workspace / Reference tabs. By default
   * Console is active; Workspace cards live under the Workspace tab.
   * `dockTab(name)` returns the locator; `openWorkspaceTab()` clicks
   * + waits.
   */
  dockTab(name) {
    return this.page.locator('.dock-tab', { hasText: new RegExp(name, 'i') });
  }
  async openWorkspaceTab() {
    await this.dockTab('Workspace').click();
    await expect(this.workspacePane).toBeVisible({ timeout: 5_000 });
  }

  /* ── Actions ── */

  /**
   * Wait until the IDE has finished booting: WASM loaded, tempFS
   * registered, the editor textarea is in the DOM and responsive.
   * We key on a Console pane log line (`Numkit IDE v3 — build …`)
   * because it's the last init step before user interaction is safe.
   */
  async waitForReady() {
    // The editor textarea is mounted only AFTER the WASM init promise
    // resolves in App.jsx — so its presence is a reliable readiness
    // gate without hard-coding a timeout.
    await this.editor.waitFor({ state: 'visible', timeout: 30_000 });
    // The console banner ("Numkit IDE v3 …") is the last thing the
    // engine writes during init; once it's there, runCode is safe.
    await expect(this.consoleOutput).toContainText(/Numkit IDE v3/i, { timeout: 15_000 });
  }

  /**
   * Replace the active editor's content and click Run. Returns when
   * the next console line lands (proxy for "execution finished").
   */
  async runScript(code) {
    // .fill() replaces existing content atomically — no keystroke-by-
    // keystroke flicker, no risk of lingering content from a prior run.
    await this.editor.fill(code);
    await this.runButton.click();
    // Wait for output to settle. Most scripts emit at least one line;
    // for noisy/long scripts, callers can await specific text instead.
    await this.page.waitForTimeout(150);
  }

  /**
   * Type something into the inline REPL input and submit with Enter.
   * Useful for testing single-statement evaluation without touching the
   * editor pane.
   */
  async repl(line) {
    await this.consoleInput.click();
    await this.consoleInput.fill(line);
    await this.consoleInput.press('Enter');
    await this.page.waitForTimeout(100);
  }

  /** All console lines as plain text, oldest first. */
  async consoleText() {
    return await this.consoleOutput.innerText();
  }
}
