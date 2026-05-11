#!/usr/bin/env node
/**
 * generate-manifest.js
 *
 * Single source of truth for example .m files: ../../docs/examples/.
 * GitHub Pages browses that tree directly (linked from README); the
 * Vite build needs them inside ide/public/examples/. Rather than
 * keeping two copies in git that silently drift (we got bitten once
 * — see commit a975c106), this script:
 *
 *   1. Walks docs/examples/ to compute the desired tree.
 *   2. Mirrors that tree into ide/public/examples/ in rsync style:
 *      copy missing/changed files, delete leftover files/folders.
 *      No nuke-and-recreate — that triggers EBUSY on Windows when
 *      vite dev server / antivirus / OneDrive keeps the folder open.
 *   3. Writes the same manifest.json (folders + files, alphabetical)
 *      into BOTH trees so they never disagree.
 *
 * Triggered automatically via package.json `predev` / `prebuild`,
 * and explicitly by deploy.sh / deploy.bat. Idempotent (~50 ms).
 *
 * The destination tree (ide/public/examples/) is in .gitignore — it
 * is regenerated on every build.
 */

import {
  readdirSync, statSync, mkdirSync, rmSync,
  copyFileSync, existsSync, writeFileSync, readFileSync,
} from 'fs';
import { join, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const SRC = join(__dirname, '..', '..', 'docs', 'examples');
const DST = join(__dirname, '..', 'public', 'examples');

if (!existsSync(SRC)) {
  console.error(`[examples] source missing: ${SRC}`);
  process.exit(1);
}

mkdirSync(DST, { recursive: true });

// 1. Build expected tree from SRC (folders → list of .m files).
const expected = new Map();
for (const entry of readdirSync(SRC).sort()) {
  if (entry === 'manifest.json') continue;
  const srcDir = join(SRC, entry);
  if (!statSync(srcDir).isDirectory()) continue;

  const mFiles = readdirSync(srcDir).filter(f => f.endsWith('.m')).sort();
  if (mFiles.length === 0) continue;
  expected.set(entry, mFiles);
}

// 2. Mirror into DST.
let copied = 0, skipped = 0, removed = 0;

// 2a. Copy / refresh files for every expected folder.
for (const [folder, files] of expected) {
  const dstDir = join(DST, folder);
  mkdirSync(dstDir, { recursive: true });

  // Copy each .m if missing or content changed (compare bytes).
  for (const f of files) {
    const srcPath = join(SRC, folder, f);
    const dstPath = join(dstDir, f);
    let needsCopy = true;
    if (existsSync(dstPath)) {
      try {
        const a = readFileSync(srcPath);
        const b = readFileSync(dstPath);
        if (a.length === b.length && a.equals(b)) needsCopy = false;
      } catch { /* fall through to copy */ }
    }
    if (needsCopy) { copyFileSync(srcPath, dstPath); copied++; }
    else skipped++;
  }

  // Remove .m files in DST that aren't in SRC anymore.
  const expectedSet = new Set(files);
  for (const f of readdirSync(dstDir)) {
    if (!f.endsWith('.m')) continue;          // leave alone non-.m
    if (expectedSet.has(f)) continue;
    rmSync(join(dstDir, f), { force: true });
    removed++;
  }
}

// 2b. Remove DST folders that aren't in SRC (e.g. renamed/deleted).
for (const entry of readdirSync(DST)) {
  if (entry === 'manifest.json') continue;
  const dstDir = join(DST, entry);
  if (!statSync(dstDir).isDirectory()) continue;
  if (expected.has(entry)) continue;
  rmSync(dstDir, { recursive: true, force: true });
  removed++;
}

// 3. Write identical manifest.json into both trees.
const folders = [...expected].map(([name, files]) => ({ name, files }));
const manifestJson = JSON.stringify({ folders }, null, 2) + '\n';
writeFileSync(join(SRC, 'manifest.json'), manifestJson);
writeFileSync(join(DST, 'manifest.json'), manifestJson);

const totalFiles = folders.reduce((s, f) => s + f.files.length, 0);
console.log(
  `[examples] ${folders.length} folders / ${totalFiles} files. ` +
  `copied=${copied} skipped=${skipped} removed=${removed}`
);
