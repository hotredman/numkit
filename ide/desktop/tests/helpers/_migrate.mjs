// _migrate.mjs — one-shot script to convert specs from per-test launch
// to the shared-Electron fixture in helpers/shared.js.
//
// Pattern in target specs (~89 files):
//
//   import { test, expect } from '@playwright/test';
//   import { launchIde, closeIde } from '../helpers/launch.js';
//   import { IdePage } from '../helpers/ide.js';
//   ...
//   test.describe('...', () => {
//     let app, page, ide;
//     test.beforeEach(async () => {
//       app = await launchIde();
//       page = await app.firstWindow();
//       ide = new IdePage(page);
//       await ide.waitForReady();
//     });
//     test.afterEach(async () => { await closeIde(app); });
//     test('...', async () => { ... });
//   });
//
// After:
//
//   import { test, expect } from '../helpers/shared.js';
//   ...
//   test.describe('...', () => {
//     test('...', async ({ ide, page }) => { ... });
//   });
//
// Files that don't fit the pattern (custom env vars, different boilerplate)
// are skipped and logged.

import { readFileSync, writeFileSync, readdirSync } from 'node:fs';
import path from 'node:path';

const dir = path.resolve(import.meta.dirname, '..', 'e2e');
const all = readdirSync(dir).filter((f) => f.endsWith('.spec.js'));

const skipped = [];
const migrated = [];

for (const file of all) {
  const full = path.join(dir, file);
  let src = readFileSync(full, 'utf8');

  // Skip if already migrated.
  if (src.includes('helpers/shared.js')) { skipped.push(`${file} (already migrated)`); continue; }

  // Must have all three legacy imports to be a candidate.
  if (!src.includes("from '@playwright/test'")
      || !src.includes("'../helpers/launch.js'")
      || !src.includes("'../helpers/ide.js'")) {
    skipped.push(`${file} (non-standard imports)`);
    continue;
  }

  // 1. Collapse the three import lines into the shared one.
  src = src.replace(
    /import\s*{\s*test\s*,\s*expect\s*}\s*from\s*'@playwright\/test'\s*;\s*\nimport\s*{\s*launchIde\s*,\s*closeIde\s*}\s*from\s*'\.\.\/helpers\/launch\.js'\s*;\s*\nimport\s*{\s*IdePage\s*}\s*from\s*'\.\.\/helpers\/ide\.js'\s*;/,
    "import { test, expect } from '../helpers/shared.js';"
  );

  // 2. Remove `let app, page, ide;` declaration (any subset of those names,
  //    most files use exactly that triple).
  src = src.replace(/^\s*let\s+app\s*,\s*page\s*,\s*ide\s*;\s*\n/m, '');

  // 3. Remove the beforeEach block. Match the canonical 6-line form.
  src = src.replace(
    /^\s*test\.beforeEach\(async\s*\(\s*\)\s*=>\s*{\s*\n\s*app\s*=\s*await\s+launchIde\(\s*(?:\{[^}]*\}\s*)?\)\s*;\s*\n\s*page\s*=\s*await\s+app\.firstWindow\(\s*\)\s*;\s*\n\s*ide\s*=\s*new\s+IdePage\(page\)\s*;\s*\n\s*await\s+ide\.waitForReady\(\s*\)\s*;\s*\n\s*}\s*\)\s*;\s*\n/m,
    ''
  );

  // 4. Remove afterEach block (single-line form most common).
  src = src.replace(
    /^\s*test\.afterEach\(async\s*\(\s*\)\s*=>\s*{\s*await\s+closeIde\(app\)\s*;?\s*}\s*\)\s*;\s*\n/m,
    ''
  );

  // 5. Inject `{ ide, page }` into test() callbacks that took no args.
  src = src.replace(/\btest\((\s*['"`][^'"`]*['"`]\s*,)\s*async\s*\(\s*\)\s*=>/g,
    'test($1 async ({ ide, page }) =>');

  // 6. Drop now-leftover blank lines around the removed blocks.
  src = src.replace(/\n{3,}/g, '\n\n');

  // Sanity: file should no longer reference the removed names.
  const stillRefsApp = /\bapp\b/.test(src) && !/\.app\b/.test(src);
  if (stillRefsApp) {
    skipped.push(`${file} (residual 'app' reference after rewrite — manual)`);
    continue;
  }
  if (src.includes('launchIde') || src.includes('closeIde')) {
    skipped.push(`${file} (still references launchIde/closeIde — manual)`);
    continue;
  }

  writeFileSync(full, src);
  migrated.push(file);
}

console.log(`migrated ${migrated.length}/${all.length}`);
for (const f of migrated) console.log('  ✓', f);
console.log(`skipped ${skipped.length}`);
for (const s of skipped) console.log('  ·', s);
