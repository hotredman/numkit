/**
 * fs/vfs-adapter.js — bridges the async IDE filesystems (tempFS, Local Folder)
 * to the sync callbacks the WASM engine expects for csvread / csvwrite.
 *
 * Why this exists
 * ───────────────
 * The WASM side calls `readFile(path)` SYNCHRONOUSLY from C++. The IDE's
 * real filesystems (IndexedDB-backed tempFS, FSA-backed Local Folder,
 * Electron-IPC local) are all async. We bridge the mismatch by keeping
 * a sync-accessible in-memory mirror:
 *
 *   • tempFS:  on init we load every entry into a Map<path,string>.
 *     Writes are applied to the Map immediately (sync) and persisted to
 *     IndexedDB asynchronously in the background (fire-and-forget).
 *
 *   • Local Folder (FSA / Electron): on mount we walk the tree and
 *     cache each file's contents into the Map. Same write-through.
 *     Works well for typical project folders; very large mounts should
 *     move to Asyncify in the WASM build.
 *
 * A later refactor could swap the sync mirror for Asyncify — the engine
 * API stays the same (readFile/writeFile/exists are still what the C++
 * side calls) so this adapter is the only place that has to change.
 */

import tempFS from '../temporary';
import localFS from './local';

// ─────────────────────────────────────────────────────────────
// Shared adapter — takes an async backend and a seed routine,
// exposes sync read/write/exists by keeping a Map mirror.
// ─────────────────────────────────────────────────────────────

function makeSyncAdapter({ backend, name }) {
  const cache = new Map();      // path → string content
  const dirty = new Set();      // paths pending async persist
  let persistPromise = Promise.resolve();
  let seeded = false;

  async function seedFrom(tree) {
    for (const node of tree) {
      if (node.type === 'file') {
        const content = await backend.readFile(node.path);
        if (typeof content === 'string') cache.set(node.path, content);
      } else if (node.type === 'folder' && node.children) {
        await seedFrom(node.children);
      }
    }
  }

  // Serialise write-backs so we don't race IndexedDB transactions.
  function schedulePersist(path) {
    dirty.add(path);
    persistPromise = persistPromise.then(async () => {
      if (!dirty.has(path)) return;
      dirty.delete(path);
      const content = cache.get(path);
      try {
        if (content === undefined) await backend.remove(path);
        else await backend.writeFile(path, content);
      } catch (e) {
        console.warn(`[vfs-adapter:${name}] persist failed for ${path}:`, e);
      }
    });
  }

  return {
    // Call this before registering with the engine — populates the mirror.
    async seed() {
      if (seeded) return;
      if (backend.init) await backend.init();
      const tree = await backend.listTree();
      await seedFrom(tree);
      seeded = true;
    },

    // Manual refresh (e.g. after the user edits a file via an external tool).
    async refresh() {
      cache.clear();
      const tree = await backend.listTree();
      await seedFrom(tree);
    },

    // Flush any pending writes and wait for them — use before shutdown.
    async flush() {
      return persistPromise;
    },

    // ── Sync hooks wired into the engine via CallbackFS ──
    readFile(path) {
      const v = cache.get(path);
      if (v === undefined)
        throw new Error(`${name}: no such file '${path}'`);
      return v;
    },
    writeFile(path, content) {
      cache.set(path, content);
      schedulePersist(path);
    },
    exists(path) {
      return cache.has(path);
    },
  };
}

// ─────────────────────────────────────────────────────────────
// Public entry point: seed both adapters and register them with
// the engine. Call once at application startup, after the WASM
// module has been initialised.
// ─────────────────────────────────────────────────────────────

export async function installVfsAdapters(engine) {
  // Register the adapter even if seed() fails — a partially- or zero-
  // populated cache is still functional for csvwrite followed by csvread
  // in the same session, and avoids "filesystem 'X' is not available"
  // errors at execution time when seed hits a transient backend hiccup
  // (IndexedDB permission, browser privacy mode, empty FS, etc.).
  const temp = makeSyncAdapter({ backend: tempFS, name: 'temporary' });
  try { await temp.seed(); }
  catch (e) { console.warn('[vfs-adapter] temporary.seed failed:', e); }
  engine.registerFs('temporary', temp);

  const local = await installLocalAdapter(engine);
  return { temp, local };
}

// Register (or re-register) the Local Folder adapter. Call this whenever
// the user mounts a folder post-page-load — the FileBrowser's
// reconnect()/pickDirectory() flow is async and fires AFTER App.jsx has
// already run installVfsAdapters, so the first call wouldn't see the mount.
// Returns the adapter, or null if Local Folder isn't available/mounted.
export async function installLocalAdapter(engine) {
  if (!localFS.isAvailable || !localFS.isAvailable()
      || !localFS.isMounted  || !localFS.isMounted())
    return null;

  const local = makeSyncAdapter({ backend: localFS, name: 'local' });
  try { await local.seed(); }
  catch (e) { console.warn('[vfs-adapter] local.seed failed:', e); }
  engine.registerFs('local', local);
  return local;
}
