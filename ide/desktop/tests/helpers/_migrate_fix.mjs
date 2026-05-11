// _migrate_fix.mjs — second pass to fix specs the first migration missed
// because regex-quoted test names (escaped quotes, template literals)
// didn't match the simple `test('name', async () =>` pattern.
//
// Strategy: for any spec that imports from helpers/shared.js, find all
// `, async () =>` arrow signatures inside test()/test.skip()/test.only()
// calls (NOT inside beforeAll/afterAll/etc which we removed already)
// and rewrite to `, async ({ ide, page }) =>`. The match is anchored
// to a comma so before/afterAll's `async () =>` (which takes no args
// at the start of the call, no preceding comma) is left alone.

import { readFileSync, writeFileSync, readdirSync } from 'node:fs';
import path from 'node:path';

const dir = path.resolve(import.meta.dirname, '..', 'e2e');
const files = readdirSync(dir).filter((f) => f.endsWith('.spec.js'));

const fixed = [];
for (const file of files) {
  const full = path.join(dir, file);
  const orig = readFileSync(full, 'utf8');
  if (!orig.includes("from '../helpers/shared.js'")) continue;

  // Match arrow inside a test()/test.xxx() callback position. The comma
  // before `async () =>` is the discriminator — beforeAll/afterAll have
  // no comma (it's the first/only arg).
  const next = orig.replace(/,\s*async\s*\(\s*\)\s*=>/g, ', async ({ ide, page }) =>');
  if (next !== orig) {
    writeFileSync(full, next);
    fixed.push(file);
  }
}

console.log(`fixed ${fixed.length}`);
for (const f of fixed) console.log('  ✓', f);
