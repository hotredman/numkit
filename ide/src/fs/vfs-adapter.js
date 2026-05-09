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
  // True when the backend exposes a synchronous read path (Electron
  // sync IPC). When true we DON'T pre-load files into the cache —
  // each WASM-side csvread/load round-trips to disk on demand. The
  // tradeoff is ~1-5 ms per file vs. multi-GB pre-load that was
  // killing the renderer.
  const sync = typeof backend.readFileSync === 'function';
  // Set when a write/remove happens between flushes; flush() returns this
  // so the caller can decide whether to invalidate UI state. Cleared on
  // each flush() call. Without this signal IDE.jsx was bumping
  // vfsRefreshKey on every script run regardless of whether anything
  // changed — and that triggers a full Sidebar tree-rebuild + recursive
  // listTree IPC walk, which on a populated local-folder mount was the
  // proven path to V8 OOM.
  let dirtySinceFlush = false;

  // Seed budget: only ingest files the WASM engine plausibly needs
  // synchronously (csvread / load / readtable text). Pre-loading every
  // file in a mounted local folder put 4 GB into the renderer working
  // set on the user's machine and was the proven cause of the
  // Chromium-OOM crash on idle (one mount → seed runs once → ws hits
  // 4 GB → renderer killed). Filters:
  //   - extension allowlist: text-like formats only
  //   - per-file size cap: skip > 1 MB (data files that big are
  //     better fetched lazily on the C++ side via async refresh)
  //   - total budget: stop when cache exceeds 64 MB combined
  // Counters are reported via a stats object on the adapter for the
  // status bar / devtools to surface.
  const SEED_EXTENSIONS = /\.(m|mlx|txt|csv|tsv|json|yaml|yml|toml|ini|cfg)$/i;
  const SEED_FILE_LIMIT_BYTES = 1 * 1024 * 1024;       // 1 MB / file
  const SEED_TOTAL_LIMIT_BYTES = 64 * 1024 * 1024;     // 64 MB total
  const seedStats = { seeded: 0, skippedExt: 0, skippedSize: 0, skippedBudget: 0, totalBytes: 0 };

  async function seedFrom(tree) {
    for (const node of tree) {
      if (node.type === 'file') {
        if (seedStats.totalBytes >= SEED_TOTAL_LIMIT_BYTES) {
          seedStats.skippedBudget++;
          continue;
        }
        if (!SEED_EXTENSIONS.test(node.name || node.path)) {
          seedStats.skippedExt++;
          continue;
        }
        let content;
        try { content = await backend.readFile(node.path); }
        catch { continue; }
        if (typeof content !== 'string') continue;
        const bytes = content.length * 2;  // pessimistic UTF-16 estimate
        if (bytes > SEED_FILE_LIMIT_BYTES) {
          seedStats.skippedSize++;
          continue;
        }
        cache.set(node.path, content);
        seedStats.seeded++;
        seedStats.totalBytes += bytes;
      } else if (node.type === 'folder' && node.children) {
        await seedFrom(node.children);
      }
    }
  }

  // Serialise write-backs so we don't race IndexedDB transactions.
  function schedulePersist(path) {
    dirty.add(path);
    dirtySinceFlush = true;
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

  function logSeedStats(action) {
    const s = seedStats;
    const mb = (s.totalBytes / 1048576).toFixed(1);
    // eslint-disable-next-line no-console
    console.log(`[vfs-adapter:${name}] ${action}: seeded=${s.seeded} (${mb} MB), `
      + `skipped(ext=${s.skippedExt}, size=${s.skippedSize}, budget=${s.skippedBudget})`);
  }

  return {
    // Call this before registering with the engine — populates the mirror.
    async seed() {
      if (seeded) return;
      if (backend.init) await backend.init();
      // Backends with sync read (native Electron) skip seeding entirely:
      // the engine reads files lazily via backend.readFileSync. Backends
      // without sync read (FSA on web, IndexedDB tempFS) pre-load with
      // the budgeted seedFrom — they have no other way to satisfy a
      // synchronous WASM-side read.
      if (sync) {
        // eslint-disable-next-line no-console
        console.log(`[vfs-adapter:${name}] sync-IPC backend; skipping seed (lazy reads on demand).`);
      } else {
        const tree = await backend.listTree();
        await seedFrom(tree);
        logSeedStats('seed');
      }
      seeded = true;
    },

    // Manual refresh (e.g. after the user edits a file via an external tool).
    async refresh() {
      cache.clear();
      Object.assign(seedStats, { seeded: 0, skippedExt: 0, skippedSize: 0, skippedBudget: 0, totalBytes: 0 });
      if (!sync) {
        const tree = await backend.listTree();
        await seedFrom(tree);
        logSeedStats('refresh');
      }
    },

    // Flush any pending writes and wait for them — use before shutdown.
    // Resolves to `true` if anything was written/removed since the last
    // flush, `false` otherwise. Callers use the boolean to skip a noisy
    // tree-rebuild when nothing changed.
    async flush() {
      const wasDirty = dirtySinceFlush;
      dirtySinceFlush = false;
      await persistPromise;
      return wasDirty;
    },

    // ── Sync hooks wired into the engine via CallbackFS ──
    readFile(path) {
      // Cache wins (covers writes-before-flush + tempFS seeded entries).
      if (cache.has(path)) return cache.get(path);
      if (sync) {
        // Lazy disk read via sync IPC. We don't populate the cache
        // here on purpose: the engine may read large CSVs once for
        // a single computation and not need them again — caching
        // would re-introduce the leak we just fixed. Hot-reads pay
        // the IPC tax; that's fine for the typical script.
        const v = backend.readFileSync(path);
        if (v == null) throw new Error(`${name}: no such file '${path}'`);
        return v;
      }
      throw new Error(`${name}: no such file '${path}'`);
    },
    writeFile(path, content) {
      cache.set(path, content);
      schedulePersist(path);
    },
    exists(path) {
      if (cache.has(path)) return true;
      if (sync && typeof backend.existsSync === 'function') {
        return backend.existsSync(path);
      }
      return false;
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
