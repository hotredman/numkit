/**
 * fs/vfs-adapter.js — bridges the IDE filesystems (tempFS, Local
 * Folder) to the SYNCHRONOUS callbacks the WASM engine expects for
 * csvread / csvwrite / load.
 *
 * Why this exists
 * ───────────────
 * The WASM side calls `readFile(path)` synchronously from C++ and
 * cannot await. Both filesystems we expose through the IDE are
 * underlyingly async (IndexedDB tempFS, Electron-IPC native local).
 * Each one provides a sync read path:
 *
 *   - Local (Electron):  `ipcRenderer.sendSync` to main → fs.readFileSync
 *   - tempFS:            SharedArrayBuffer + Atomics.wait → worker → IDB
 *
 * Both expose `readFileSync(path)` / `existsSync(path)` to this
 * adapter. With those in hand we DON'T pre-populate any cache —
 * every WASM-side read is on demand, and the renderer never holds
 * file contents proactively. The proactive seeding that this
 * adapter used to do was the proven cause of a 4 GB renderer-OOM
 * on idle when the user mounted a populated local folder.
 *
 * Fallback
 * ────────
 * If a backend doesn't expose readFileSync (e.g. a future web
 * deployment without crossOriginIsolated, where SharedArrayBuffer
 * is unavailable), we fall back to a small bounded seed at mount
 * time. The fallback path is meant for tempFS only — it's the only
 * way to satisfy a sync WASM read against IndexedDB without the
 * SAB bridge.
 */

import tempFS from '../temporary';
import localFS from './local';

// Bounded fallback seed (used only when backend has no readFileSync).
// Limits exist to make sure even a degenerate fallback doesn't recreate
// the OOM bug: text-like extensions, 1 MB / file, 8 MB total. tempFS is
// the realistic only consumer of this path and never approaches the cap.
const FALLBACK_SEED_EXTENSIONS = /\.(m|mlx|txt|csv|tsv|json|yaml|yml|toml|ini|cfg)$/i;
const FALLBACK_FILE_LIMIT_BYTES  = 1 * 1024 * 1024;
const FALLBACK_TOTAL_LIMIT_BYTES = 8 * 1024 * 1024;

// Normalise any cached/backend value to raw bytes (Uint8Array). The WASM
// binary read hook (CallbackFS readFileBytes) requires a Uint8Array.
function toU8(v) {
  if (v instanceof Uint8Array) return v;
  if (v instanceof ArrayBuffer) return new Uint8Array(v);
  if (ArrayBuffer.isView(v)) return new Uint8Array(v.buffer, v.byteOffset, v.byteLength);
  // string → byte array (Latin1: one byte per code unit). Lossless for text
  // that came from our own readFile; for a backend that returned a UTF-8
  // string for a genuinely-binary file the bytes were already lost upstream,
  // which is exactly why binary backends must expose readFileBytesSync.
  const s = String(v);
  const u = new Uint8Array(s.length);
  for (let i = 0; i < s.length; i++) u[i] = s.charCodeAt(i) & 0xff;
  return u;
}

// Normalise any cached/backend value to a text string for the text read hook.
function asText(v) {
  if (typeof v === 'string') return v;
  if (v instanceof Uint8Array) return new TextDecoder().decode(v);
  if (v instanceof ArrayBuffer) return new TextDecoder().decode(new Uint8Array(v));
  if (ArrayBuffer.isView(v)) return new TextDecoder().decode(new Uint8Array(v.buffer, v.byteOffset, v.byteLength));
  return String(v);
}

function makeSyncAdapter({ backend, name }) {
  const cache = new Map();      // path → content (writes-before-flush)
  const dirty = new Set();      // paths pending async persist
  let persistPromise = Promise.resolve();
  let dirtySinceFlush = false;
  let seedPromise = null;       // race lock — concurrent seed() callers share

  const sync = typeof backend.readFileSync === 'function';

  // Serialise write-backs so we don't race async-store transactions.
  function schedulePersist(path) {
    dirty.add(path);
    dirtySinceFlush = true;
    persistPromise = persistPromise.then(async () => {
      if (!dirty.has(path)) return;
      dirty.delete(path);
      const content = cache.get(path);
      try {
        if (content === undefined) {
          await backend.remove(path);
        } else if (content instanceof Uint8Array
                   && typeof backend.writeFileBytes === 'function') {
          // Binary content + a binary-capable backend → persist raw bytes.
          // (tempFS direct-IDB has no writeFileBytes but stores the
          //  Uint8Array losslessly through writeFile, so it falls through.)
          await backend.writeFileBytes(path, content);
        } else {
          await backend.writeFile(path, content);
        }
      } catch (e) {
        console.warn(`[vfs-adapter:${name}] persist failed for ${path}:`, e);
      }
    });
  }

  // Fallback seed — used only when the backend has no sync read.
  async function fallbackSeed() {
    let total = 0;
    let count = 0;
    async function visit(tree) {
      for (const node of tree) {
        if (total >= FALLBACK_TOTAL_LIMIT_BYTES) return;
        if (node.type === 'file') {
          if (!FALLBACK_SEED_EXTENSIONS.test(node.name || node.path)) continue;
          let content;
          try { content = await backend.readFile(node.path); }
          catch { continue; }
          if (typeof content !== 'string') continue;
          const bytes = content.length * 2;
          if (bytes > FALLBACK_FILE_LIMIT_BYTES) continue;
          if (total + bytes > FALLBACK_TOTAL_LIMIT_BYTES) continue;
          cache.set(node.path, content);
          total += bytes;
          count++;
        } else if (node.type === 'folder' && node.children) {
          await visit(node.children);
        }
      }
    }
    const tree = await backend.listTree();
    await visit(tree);
    // eslint-disable-next-line no-console
    console.log(`[vfs-adapter:${name}] fallback seed: ${count} files, ${(total/1024).toFixed(1)} KB`);
  }

  return {
    /**
     * Initialise the backend and (only when no sync read is
     * available) populate the bounded fallback cache. Idempotent;
     * concurrent callers share one in-flight Promise.
     */
    async seed() {
      if (seedPromise) return seedPromise;
      seedPromise = (async () => {
        if (backend.init) await backend.init();
        if (sync) {
          // eslint-disable-next-line no-console
          console.log(`[vfs-adapter:${name}] sync backend; no seed (lazy on demand).`);
        } else {
          await fallbackSeed();
        }
      })();
      return seedPromise;
    },

    /**
     * Wipe in-memory state and re-seed (only when no sync read).
     * Used when the user manually edits files outside the IDE.
     */
    async refresh() {
      cache.clear();
      seedPromise = null;
      if (!sync) await this.seed();
    },

    /**
     * Wait for any pending writes to flush. Resolves to `true` if
     * anything was written/removed since the last flush — callers
     * use this to skip a no-op Sidebar tree rebuild on read-only
     * script runs.
     */
    async flush() {
      const wasDirty = dirtySinceFlush;
      dirtySinceFlush = false;
      await persistPromise;
      return wasDirty;
    },

    // ── Sync hooks wired into the WASM engine via CallbackFS ──
    readFile(path) {
      // Cache hit covers writes-before-flush AND fallback-seeded entries.
      if (cache.has(path)) return asText(cache.get(path));
      if (sync) {
        const v = backend.readFileSync(path);
        if (v == null) throw new Error(`${name}: no such file '${path}'`);
        return asText(v);
      }
      throw new Error(`${name}: no such file '${path}'`);
    },
    // Binary read — returns a Uint8Array (imread/audioread/etc.). Prefers a
    // dedicated binary sync path on the backend; otherwise normalises
    // whatever readFileSync returns to bytes.
    readFileBytes(path) {
      let v;
      if (cache.has(path)) {
        v = cache.get(path);
      } else if (sync) {
        v = typeof backend.readFileBytesSync === 'function'
          ? backend.readFileBytesSync(path)
          : backend.readFileSync(path);
      }
      if (v == null) throw new Error(`${name}: no such file '${path}'`);
      return toU8(v);
    },
    writeFile(path, content) {
      cache.set(path, content);
      schedulePersist(path);
    },
    // Binary write — stores raw bytes (Uint8Array) so the round-trip is
    // lossless; persistence keeps the Uint8Array as-is (IndexedDB stores it
    // natively; the Local backend writes it as a binary file).
    writeFileBytes(path, bytes) {
      cache.set(path, toU8(bytes));
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
// Public entry point: register adapters with the engine.
// ─────────────────────────────────────────────────────────────

export async function installVfsAdapters(engine) {
  const temp = makeSyncAdapter({ backend: tempFS, name: 'temporary' });
  try { await temp.seed(); }
  catch (e) { console.warn('[vfs-adapter] temporary.seed failed:', e); }
  engine.registerFs('temporary', temp);

  const local = await installLocalAdapter(engine);
  return { temp, local };
}

// Register (or re-register) the Local Folder adapter. The FileBrowser
// reconnect()/pickDirectory() flow fires AFTER installVfsAdapters has
// already run, so the first installVfsAdapters call won't see a
// mount; the FileBrowser calls this when one becomes available.
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
