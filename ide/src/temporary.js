/**
 * temporary.js — IndexedDB-backed scratch filesystem for Numkit IDE.
 *
 * "Temporary" because it lives only in the browser's IndexedDB:
 * clearing site data wipes it. For real-disk access see Local Folder
 * (Electron native FS / FSA on the web).
 *
 * Two execution paths share the same outward API:
 *
 *   1. Sync-capable runtime (Electron with crossOriginIsolated, web
 *      with COOP/COEP headers) — all I/O is delegated to a Web
 *      Worker that owns a single IDB connection. Sync reads from the
 *      WASM engine block on a SharedArrayBuffer + Atomics.wait. The
 *      main thread never holds file content in memory; the cache
 *      that pre-loaded every file at mount has been removed.
 *
 *   2. Fallback runtime (no SharedArrayBuffer / no
 *      crossOriginIsolated, e.g. plain GitHub Pages) — direct IDB
 *      calls from the main thread. Sync hooks are NOT exposed; the
 *      vfs-adapter falls back to its bounded seed-on-init path.
 *
 * Each entry: { path, type: 'file'|'folder', content?, modified }
 *
 * Usage (unchanged from before the refactor):
 *   import tempFS from './temporary';
 *   await tempFS.init();
 *   await tempFS.writeFile('/My Scripts/hello.m', 'disp("hello")');
 *   const code = await tempFS.readFile('/My Scripts/hello.m');
 *   const tree = await tempFS.listTree();
 */

import { makeBridge, isSupported as bridgeSupported } from './sab-bridge/sync-bridge';

const DB_NAME = 'numkit-ide-vfs';
const DB_VERSION = 1;
const STORE_NAME = 'files';

// ── Direct IDB fallback (used when bridge is unavailable) ──

let directDb = null;

function openDirectDB() {
  return new Promise((resolve, reject) => {
    const req = indexedDB.open(DB_NAME, DB_VERSION);
    req.onupgradeneeded = (e) => {
      const d = e.target.result;
      if (!d.objectStoreNames.contains(STORE_NAME)) {
        const store = d.createObjectStore(STORE_NAME, { keyPath: 'path' });
        store.createIndex('type',   'type',   { unique: false });
        store.createIndex('parent', 'parent', { unique: false });
      }
    };
    req.onsuccess = () => resolve(req.result);
    req.onerror   = () => reject(req.error);
  });
}

function dtx(mode) {
  return directDb.transaction(STORE_NAME, mode).objectStore(STORE_NAME);
}

function reqP(req) {
  return new Promise((resolve, reject) => {
    req.onsuccess = () => resolve(req.result);
    req.onerror   = () => reject(req.error);
  });
}

function parentPath(p) {
  const idx = p.lastIndexOf('/');
  if (idx <= 0) return '/';
  return p.substring(0, idx);
}

function normPath(p) {
  p = (p || '').replace(/\/+/g, '/');
  if (!p.startsWith('/')) p = '/' + p;
  if (p.length > 1 && p.endsWith('/')) p = p.slice(0, -1);
  return p;
}

const direct = {
  async init() { directDb = await openDirectDB(); },

  async mkdir(path) {
    path = normPath(path);
    if (path === '/') return;
    const parent = parentPath(path);
    if (parent !== '/') {
      const p = await reqP(dtx('readonly').get(parent));
      if (!p) await direct.mkdir(parent);
    }
    const existing = await reqP(dtx('readonly').get(path));
    if (!existing) {
      await reqP(dtx('readwrite').put({
        path, type: 'folder', parent: parentPath(path), modified: Date.now(),
      }));
    }
  },

  async writeFile(path, content) {
    path = normPath(path);
    const parent = parentPath(path);
    if (parent !== '/') await direct.mkdir(parent);
    await reqP(dtx('readwrite').put({
      path, type: 'file', parent, content: content || '', modified: Date.now(),
    }));
  },

  async readFile(path) {
    const entry = await reqP(dtx('readonly').get(normPath(path)));
    if (!entry || entry.type !== 'file') return null;
    return entry.content;
  },

  async exists(path) {
    const entry = await reqP(dtx('readonly').get(normPath(path)));
    return !!entry;
  },

  async remove(path) {
    path = normPath(path);
    const all = await reqP(dtx('readonly').getAll());
    const toDelete = all.filter((e) => e.path === path || e.path.startsWith(path + '/'));
    const wstore = dtx('readwrite');
    for (const e of toDelete) wstore.delete(e.path);
    return new Promise((resolve, reject) => {
      wstore.transaction.oncomplete = () => resolve();
      wstore.transaction.onerror    = () => reject(wstore.transaction.error);
    });
  },

  async rename(oldP, newP) {
    oldP = normPath(oldP);
    newP = normPath(newP);
    const all = await reqP(dtx('readonly').getAll());
    const toMove = all.filter((e) => e.path === oldP || e.path.startsWith(oldP + '/'));
    await direct.mkdir(parentPath(newP));
    const wstore = dtx('readwrite');
    for (const entry of toMove) {
      wstore.delete(entry.path);
      const suffix = entry.path.substring(oldP.length);
      const updatedPath = newP + suffix;
      wstore.put({
        ...entry,
        path: updatedPath,
        parent: parentPath(updatedPath),
        modified: Date.now(),
      });
    }
    return new Promise((resolve, reject) => {
      wstore.transaction.oncomplete = () => resolve();
      wstore.transaction.onerror    = () => reject(wstore.transaction.error);
    });
  },

  async listTree() {
    const all = await reqP(dtx('readonly').getAll());
    const root = [];
    const folderMap = { '/': root };
    all.sort((a, b) => a.path.localeCompare(b.path));
    for (const entry of all) {
      const node = {
        name: entry.path.split('/').pop(),
        path: entry.path,
        type: entry.type,
        modified: entry.modified,
      };
      if (entry.type === 'folder') {
        node.children = [];
        folderMap[entry.path] = node.children;
      }
      const parent = parentPath(entry.path);
      const parentChildren = folderMap[parent];
      if (parentChildren) parentChildren.push(node);
      else root.push(node);
    }
    function sortChildren(nodes) {
      nodes.sort((a, b) => {
        if (a.type !== b.type) return a.type === 'folder' ? -1 : 1;
        return a.name.localeCompare(b.name);
      });
      for (const n of nodes) if (n.children) sortChildren(n.children);
    }
    sortChildren(root);
    return root;
  },

  async clear() { await reqP(dtx('readwrite').clear()); },
  async count() { return await reqP(dtx('readonly').count()); },
};

// ── Public tempFS — bridge if supported, direct IDB otherwise ──
//
// 2026-05-09: bridge path is gated behind NUMKIT_TEMPFS_BRIDGE=1 because
// packaged Electron (file:// origin) doesn't reliably expose
// crossOriginIsolated even with COOP/COEP headers injected via
// webRequest.onHeadersReceived — the worker silently fails to start and
// every tempFS call hangs. Direct IDB is the proven path for tempFS;
// the leak we fixed was in LOCAL FS, where Electron sync IPC still
// covers sync reads. Re-enable the bridge by setting the env var on a
// build that proves crossOriginIsolated is true (e.g. via a custom
// protocol handler with explicit secure-context registration).
const bridgeEnabled = (typeof import.meta !== 'undefined'
                    && import.meta.env?.VITE_TEMPFS_BRIDGE === '1');
const bridge = bridgeEnabled ? makeBridge() : null;
const useBridge = bridge !== null;

const tempFS = {
  /** Whether sync read/exists hooks are available on this runtime. */
  isSyncCapable() { return useBridge; },

  async init() {
    if (useBridge) await bridge.ready();
    else           await direct.init();
  },

  async listTree()                  { return useBridge ? bridge.listTree()              : direct.listTree(); },
  async readFile(p)                 { return useBridge ? bridge.read(p)                  : direct.readFile(p); },
  async writeFile(p, c)             { return useBridge ? bridge.write(p, c)              : direct.writeFile(p, c); },
  async mkdir(p)                    { return useBridge ? bridge.mkdir(p)                 : direct.mkdir(p); },
  async exists(p)                   { return useBridge ? bridge.exists(p)                : direct.exists(p); },
  async remove(p)                   { return useBridge ? bridge.remove(p)                : direct.remove(p); },
  async rename(o, n)                { return useBridge ? bridge.rename(o, n)             : direct.rename(o, n); },
  async clear()                     { return useBridge ? bridge.clear()                  : direct.clear(); },
  async count()                     { return useBridge ? bridge.count()                  : direct.count(); },
};

// Expose sync hooks ONLY when the bridge is up. The vfs-adapter
// branches on `typeof backend.readFileSync === 'function'`; presence
// of these methods is the signal to skip seeding.
if (useBridge) {
  tempFS.readFileSync = (p) => bridge.readSync(p);
  tempFS.existsSync   = (p) => bridge.existsSync(p);
}

// One-line trace so a user inspecting devtools can tell which path
// is active without reading the source.
// eslint-disable-next-line no-console
console.log(`[tempFS] ${
  useBridge
    ? 'sync bridge active (Worker + SAB)'
    : !bridgeEnabled
      ? 'direct IDB (bridge gated off — set VITE_TEMPFS_BRIDGE=1 to opt in)'
      : bridgeSupported()
        ? 'direct IDB (bridge construction failed — check console for vfs-worker errors)'
        : 'direct IDB (no SharedArrayBuffer / crossOriginIsolated in this runtime)'
}`);

export default tempFS;
