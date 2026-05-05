// ide/src/fs/vfs-adapter.test.js
//
// Tests the sync/async bridge used to route engine-side csvread / csvwrite
// through the IDE's asynchronous IndexedDB-backed filesystems.
//
// Rather than booting indexeddb-in-node, we hand the adapter a fake async
// backend that returns tracked Promises. That lets us assert:
//   • the cache mirror serves sync reads after seed()
//   • writes are applied to the cache immediately (sync contract holds)
//   • persists happen in order, and flush() waits for all of them
//   • the latest write for a given path always wins (coalescing)

import { describe, it, expect, beforeEach, vi } from 'vitest';

// vitest runs with Node + jsdom-less by default; we need to stub out
// the imports that reach into IndexedDB since the adapter imports the
// real backends at module load. We mock them per-test so each run is
// independent.

vi.mock('../temporary', () => ({ default: createFakeBackend() }));
vi.mock('./local', () => {
  const backend = createFakeBackend();
  let mounted = false;
  return {
    default: {
      ...backend,
      isAvailable: () => true,
      isMounted: () => mounted,
      // Test helper — lets specific tests simulate a post-init mount.
      _setMounted(v) { mounted = v; },
    },
  };
});

function createFakeBackend() {
  const files = new Map();
  const log = [];
  return {
    _files: files,
    _log: log,
    async init() {},
    async listTree() {
      const out = [];
      for (const [path] of files)
        out.push({ name: path.split('/').pop(), path, type: 'file' });
      return out;
    },
    async readFile(path) {
      log.push(['read', path]);
      return files.has(path) ? files.get(path) : null;
    },
    async writeFile(path, content) {
      log.push(['write', path, content]);
      await Promise.resolve(); // force microtask
      files.set(path, content);
    },
    async remove(path) {
      log.push(['remove', path]);
      files.delete(path);
    },
  };
}

// Import adapter AFTER the mocks register.
const { installVfsAdapters, installLocalAdapter } = await import('./vfs-adapter');
const tempFS = (await import('../temporary')).default;
const localFS = (await import('./local')).default;

function fakeEngine() {
  const fs = new Map();
  return {
    registerFs(name, handler) { fs.set(name, handler); },
    getFs(name) { return fs.get(name); },
  };
}

describe('vfs-adapter (temporary)', () => {
  beforeEach(() => {
    tempFS._files.clear();
    tempFS._log.length = 0;
    localFS._files.clear();
    localFS._log.length = 0;
    localFS._setMounted(false);
  });

  it('seeds the cache from backend before engine registration', async () => {
    tempFS._files.set('/a.csv', 'hello');
    tempFS._files.set('/nested/b.csv', 'world');

    // listTree here only returns files at root level (the fake backend
    // doesn't synthesise folder nodes) — ensure the adapter at least
    // picks up root-level files through its seed walk.
    const engine = fakeEngine();
    await installVfsAdapters(engine);

    const handler = engine.getFs('temporary');
    expect(handler.exists('/a.csv')).toBe(true);
    expect(handler.readFile('/a.csv')).toBe('hello');
  });

  it('readFile returns content synchronously after writeFile', async () => {
    const engine = fakeEngine();
    await installVfsAdapters(engine);
    const handler = engine.getFs('temporary');

    handler.writeFile('/out.csv', '1,2\n3,4\n');
    // Sync contract: read immediately, before any await.
    expect(handler.readFile('/out.csv')).toBe('1,2\n3,4\n');
    expect(handler.exists('/out.csv')).toBe(true);
  });

  it('throws on readFile for a missing path', async () => {
    const engine = fakeEngine();
    await installVfsAdapters(engine);
    const handler = engine.getFs('temporary');
    expect(() => handler.readFile('/does-not-exist')).toThrow(/no such file/);
  });

  it('writes are persisted to the backend asynchronously and flush() awaits them', async () => {
    const engine = fakeEngine();
    const { temp } = await installVfsAdapters(engine);
    const handler = engine.getFs('temporary');

    handler.writeFile('/a.csv', 'A');
    handler.writeFile('/b.csv', 'B');

    // Before flush: writes may still be in flight.
    await temp.flush();

    expect(tempFS._files.get('/a.csv')).toBe('A');
    expect(tempFS._files.get('/b.csv')).toBe('B');
  });

  it('repeated writes to the same path coalesce — only the latest content lands', async () => {
    const engine = fakeEngine();
    const { temp } = await installVfsAdapters(engine);
    const handler = engine.getFs('temporary');

    handler.writeFile('/x.csv', 'v1');
    handler.writeFile('/x.csv', 'v2');
    handler.writeFile('/x.csv', 'v3');

    await temp.flush();

    expect(tempFS._files.get('/x.csv')).toBe('v3');
    // At least one write was issued; the intermediate values may be
    // dropped. Strict count isn't what we care about — only that the
    // final disk state matches the last cache set.
    const writeOps = tempFS._log.filter(e => e[0] === 'write' && e[1] === '/x.csv');
    expect(writeOps.length).toBeGreaterThanOrEqual(1);
    expect(writeOps[writeOps.length - 1][2]).toBe('v3');
  });

  it('does not register the local adapter when Local Folder is not mounted', async () => {
    const engine = fakeEngine();
    const { local } = await installVfsAdapters(engine);
    expect(local).toBe(null);
    expect(engine.getFs('local')).toBeUndefined();
  });

  it('installLocalAdapter registers the local FS after a post-init mount', async () => {
    // Mirrors the real IDE flow: engine boots while Local Folder is still
    // disconnected; user then mounts a folder via FileBrowser, which calls
    // onLocalMount → installLocalAdapter. Before this helper existed, the
    // local adapter was permanently null for the rest of the session, and
    // scripts from Local Folder tabs wrote to Temporary instead.
    const engine = fakeEngine();
    await installVfsAdapters(engine);
    expect(engine.getFs('local')).toBeUndefined();

    localFS._setMounted(true);
    localFS._files.set('/data.csv', '1,2\n');

    const local = await installLocalAdapter(engine);
    expect(local).not.toBe(null);

    const handler = engine.getFs('local');
    expect(handler).toBeDefined();
    expect(handler.readFile('/data.csv')).toBe('1,2\n');
  });

  it('installLocalAdapter is a no-op when Local Folder remains unmounted', async () => {
    const engine = fakeEngine();
    const local = await installLocalAdapter(engine);
    expect(local).toBe(null);
    expect(engine.getFs('local')).toBeUndefined();
  });

  it('still registers the temporary adapter when seed() fails', async () => {
    // Simulate a transient backend failure during seed — e.g. Firefox
    // private mode where IndexedDB is partially restricted. Before the
    // fix, installVfsAdapters rethrew and App.jsx silently left
    // vfsAdapters null, which later surfaced as
    // "filesystem 'temporary' is not available" on csvwrite.
    const origListTree = tempFS.listTree;
    tempFS.listTree = async () => { throw new Error('simulated IDB failure'); };

    try {
      const engine = fakeEngine();
      await installVfsAdapters(engine);

      const handler = engine.getFs('temporary');
      expect(handler).toBeDefined();

      handler.writeFile('/foo.csv', 'x');
      expect(handler.readFile('/foo.csv')).toBe('x');
    } finally {
      tempFS.listTree = origListTree;
    }
  });
});
