// @vitest-environment jsdom
//
// Render smoke for the file-browser Sidebar. On mount it loads a tree:
// the default 'examples' source fetches examples/manifest.json, and the
// 'temporary' source lists the in-memory VFS. We stub fetch so the
// examples manifest resolves deterministically (one folder, one file),
// exercising the real tree-render path. localStorage (usePersistedState)
// and the temporary FS memory-fallback both work in this env. Catches
// the mount-time ReferenceError class that build + pure-logic tests miss.

import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, cleanup, act } from '@testing-library/react';
import Sidebar from './Sidebar';

beforeEach(() => {
  globalThis.ResizeObserver = globalThis.ResizeObserver
    || class { observe() {} unobserve() {} disconnect() {} };
  // Deterministic examples manifest + file fetches.
  globalThis.fetch = vi.fn(async (url) => {
    const u = String(url);
    if (u.includes('manifest.json')) {
      return { ok: true, json: async () => ({ folders: [{ name: 'demo_set', files: ['hello.m'] }] }) };
    }
    return { ok: true, text: async () => 'disp(1)' };
  });
  try { localStorage.clear(); } catch { /* no localStorage */ }
});
afterEach(cleanup);

const noop = () => {};

// Wrap render in act + flush microtasks so the async loadTree() effect
// (fetch → setTree) settles before assertions and isn't flagged as an
// unwrapped state update.
async function mountSidebar(props = {}) {
  let result;
  await act(async () => {
    result = render(<Sidebar onOpenFile={noop} isTabUnsaved={() => false}
      onLocalMount={noop} vfsAdapters={{}} vfsRefreshKey={0} {...props} />);
    await Promise.resolve();
    await Promise.resolve();
  });
  return result;
}

describe('Sidebar render smoke', () => {
  it('mounts the examples source (default) without throwing', async () => {
    const { container } = await mountSidebar();
    // The two-row head (combo + new-file/new-folder/refresh) is always present.
    expect(container.querySelector('.sidebar-head')).toBeTruthy();
  });

  it('mounts the temporary source without throwing (listTree errors are swallowed)', async () => {
    // No IndexedDB in node env, so tempFS.listTree rejects. Sidebar's
    // loadTree catches it and logs — assert it degrades gracefully and
    // still renders, while keeping the expected error out of the output.
    localStorage.setItem('numkit.ide.sidebar.source', JSON.stringify('temporary'));
    const errSpy = vi.spyOn(console, 'error').mockImplementation(() => {});
    try {
      const { container } = await mountSidebar();
      expect(container.querySelector('.sidebar-head')).toBeTruthy();
    } finally {
      errSpy.mockRestore();
    }
  });
});
