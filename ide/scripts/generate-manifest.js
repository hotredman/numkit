#!/usr/bin/env node
/**
 * generate-manifest.js
 * Scans public/examples/ and generates public/examples/manifest.json
 * Run: node scripts/generate-manifest.js
 */

import { readdirSync, statSync, writeFileSync } from 'fs';
import { join, basename, dirname } from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
const EXAMPLES_DIR = join(__dirname, '..', 'public', 'examples');

function scanDir(dir) {
  const entries = readdirSync(dir).sort();
  const folders = [];

  for (const entry of entries) {
    const fullPath = join(dir, entry);
    const stat = statSync(fullPath);

    if (stat.isDirectory()) {
      const files = readdirSync(fullPath)
        .filter(f => f.endsWith('.m'))
        .sort();

      if (files.length > 0) {
        folders.push({ name: entry, files });
      }
    }
  }

  return { folders };
}

const manifest = scanDir(EXAMPLES_DIR);
const outPath = join(EXAMPLES_DIR, 'manifest.json');
writeFileSync(outPath, JSON.stringify(manifest, null, 2) + '\n');

console.log(`[manifest] Generated ${outPath}`);
console.log(`[manifest] ${manifest.folders.length} folders, ${manifest.folders.reduce((s, f) => s + f.files.length, 0)} files`);
