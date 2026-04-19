// ide/src/fs/run-origin.test.js
//
// Covers every branch of pickRunOrigin — the policy that decides which
// VirtualFS a Run button press routes through. Written as plain unit
// tests (no React) so the decision matrix stays legible and we catch
// regressions without mounting components.

import { describe, it, expect } from 'vitest';
import { originForSource, pickRunOrigin } from './run-origin';

const tempAdapter = { _name: 'temp' };
const localAdapter = { _name: 'local' };

describe('originForSource', () => {
  it('maps localFolder to "local"', () => {
    expect(originForSource('localFolder')).toBe('local');
  });

  it.each([
    ['temporary'],
    ['examples'],
    ['github'],
    [null],
    [undefined],
    ['some-unknown-source'],
  ])('maps %s to "temporary"', (source) => {
    expect(originForSource(source)).toBe('temporary');
  });
});

describe('pickRunOrigin', () => {
  it('returns the local adapter for a Local Folder tab when local is registered', () => {
    const r = pickRunOrigin('localFolder', { temp: tempAdapter, local: localAdapter });
    expect(r).toEqual({ adapter: localAdapter, origin: 'local', fallbackUsed: false });
  });

  it('falls back to Temporary (with flag) when local tab runs but local is null', () => {
    const r = pickRunOrigin('localFolder', { temp: tempAdapter, local: null });
    expect(r).toEqual({ adapter: tempAdapter, origin: 'temporary', fallbackUsed: true });
  });

  it('returns the temp adapter for a Temporary tab', () => {
    const r = pickRunOrigin('temporary', { temp: tempAdapter, local: localAdapter });
    expect(r).toEqual({ adapter: tempAdapter, origin: 'temporary', fallbackUsed: false });
  });

  it('falls back to temp for an Examples tab, no fallback flag (not a real downgrade)', () => {
    const r = pickRunOrigin('examples', { temp: tempAdapter, local: null });
    expect(r).toEqual({ adapter: tempAdapter, origin: 'temporary', fallbackUsed: false });
  });

  it('returns nulls when nothing is registered', () => {
    const r = pickRunOrigin('localFolder', { temp: null, local: null });
    expect(r).toEqual({ adapter: null, origin: null, fallbackUsed: false });
  });

  it('returns nulls when adapters is null (pre-install failure)', () => {
    const r = pickRunOrigin('temporary', null);
    expect(r).toEqual({ adapter: null, origin: null, fallbackUsed: false });
  });

  it('returns nulls when adapters is undefined (pre-install)', () => {
    const r = pickRunOrigin('localFolder', undefined);
    expect(r).toEqual({ adapter: null, origin: null, fallbackUsed: false });
  });

  it('does NOT set fallbackUsed for a temp tab that just lacks a local adapter', () => {
    // Only flag as fallback when the user *wanted* local — that way we
    // don't warn on every REPL command / examples run.
    const r = pickRunOrigin('temporary', { temp: tempAdapter, local: null });
    expect(r.fallbackUsed).toBe(false);
  });
});
