// D1 mechanical guard (promised in dev-docs/handbook/
// svelte_architecture.md the moment a second page appeared): page
// files are composition ONLY — no raw <div>, no style= attributes, no
// <style> blocks. lib/ is exempt (that is where markup lives).
import { describe, it, expect } from 'vitest';
import { readdirSync, readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const src = path.resolve(path.dirname(fileURLToPath(import.meta.url)));
const pages = [
  'App.svelte',
  ...readdirSync(path.join(src, 'pages'))
    .filter((f) => f.endsWith('.svelte'))
    .map((f) => path.join('pages', f)),
];

describe('page purity (rules R1/R2)', () => {
  it('scans at least the three route pages + outlet', () => {
    expect(pages.length).toBeGreaterThanOrEqual(4);
  });

  for (const p of pages) {
    it(`${p}: no raw div / style`, () => {
      const text = readFileSync(path.join(src, p), 'utf-8');
      expect(text, `${p} contains a raw <div>`).not.toMatch(/<div[\s>]/);
      expect(text, `${p} contains a style= attribute`).not.toMatch(/\sstyle\s*=/);
      expect(text, `${p} contains a <style> block`).not.toMatch(/<style/);
    });
  }
});

// Router sanity — pure logic, no DOM.
import { parseHash, hrefFor } from './lib/router.js';

describe('router', () => {
  it('parses the three routes', () => {
    expect(parseHash('')).toEqual({ page: 'catalog' });
    expect(parseHash('#/')).toEqual({ page: 'catalog' });
    expect(parseHash('#/c/comms')).toEqual({ page: 'category', id: 'comms' });
    expect(parseHash('#/s/arrays/qr-decomposition.m'))
      .toEqual({ page: 'script', id: 'arrays/qr-decomposition.m' });
  });

  it('hrefFor round-trips', () => {
    expect(hrefFor(parseHash('#/s/dsp/goertzel.m'))).toBe('#/s/dsp/goertzel.m');
  });
});

// Gallery model sanity.
import {
  categoriesOf, scriptsOf, searchScripts, categoryOfEntry,
} from './lib/galleryModel.js';

const fakeGallery = [
  { id: 'arrays/a.m', title: 'Alpha', description: 'qr decomposition', source: '' },
  { id: 'arrays/b.m', title: 'Beta', description: '', source: '' },
  { id: 'comms/c.m', title: 'Gamma', description: 'msk modulation', source: '' },
];

describe('galleryModel', () => {
  it('groups by first id segment', () => {
    const cats = categoriesOf(fakeGallery);
    expect(cats).toEqual([
      { id: 'arrays', name: 'Массивы', count: 2 },
      { id: 'comms', name: 'Связь', count: 1 },
    ]);
  });
  it('lists scripts of a category', () => {
    expect(scriptsOf(fakeGallery, 'arrays').map((s) => s.id))
      .toEqual(['arrays/a.m', 'arrays/b.m']);
  });
  it('searches title, description and id', () => {
    expect(searchScripts(fakeGallery, 'msk').map((s) => s.id))
      .toEqual(['comms/c.m']);
    expect(searchScripts(fakeGallery, 'ALPHA').map((s) => s.id))
      .toEqual(['arrays/a.m']);
    expect(searchScripts(fakeGallery, '  ')).toEqual([]);
  });
  it('derives the category of an entry', () => {
    expect(categoryOfEntry('comms/c.m')).toBe('comms');
  });
});
