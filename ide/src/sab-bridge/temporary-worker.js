// temporary-worker.js — runs in a Web Worker, owns the tempFS
// IndexedDB connection, services both sync (SAB-mediated) and async
// (postMessage-mediated) requests from the main thread.
//
// Why a worker exists at all: WASM-side csvread / load callbacks are
// synchronous and cannot await. IndexedDB has no sync API. The trick
// (Emscripten Asyncify uses the same shape) is for main to allocate a
// SharedArrayBuffer + Atomics.wait on it; the worker runs IDB async,
// writes the result into the SAB, and Atomics.notify wakes main. From
// main's perspective the call looks synchronous.
//
// Async ops (writeFile / mkdir / list / etc.) ride a normal
// postMessage protocol with request IDs — the worker is the single
// owner of the IDB connection so we don't have to coordinate
// transactions across two sides.

import {
  SAB_HEADER_SIZE,
  SAB_STATUS_INDEX, SAB_PAYLOAD_LEN_INDEX,
  STATUS_OK, STATUS_NOT_FOUND, STATUS_TOO_LARGE, STATUS_ERROR,
} from './sab-protocol';

const DB_NAME = 'numkit-ide-vfs';
const DB_VERSION = 1;
const STORE_NAME = 'files';

let db = null;
let metaView = null;   // Int32Array(sab, 0, 4)
let dataView = null;   // Uint8Array(sab, SAB_HEADER_SIZE)

const enc = new TextEncoder();

// ── IDB plumbing ─────────────────────────────────────────────────

function openDB() {
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

function tx(mode) {
  return db.transaction(STORE_NAME, mode).objectStore(STORE_NAME);
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

// ── Sync ops (write into SAB, notify) ────────────────────────────

async function syncRead(path) {
  try {
    const entry = await reqP(tx('readonly').get(normPath(path)));
    if (!entry || entry.type !== 'file') {
      Atomics.store(metaView, SAB_PAYLOAD_LEN_INDEX, 0);
      Atomics.store(metaView, SAB_STATUS_INDEX, STATUS_NOT_FOUND);
      Atomics.notify(metaView, SAB_STATUS_INDEX);
      return;
    }
    const bytes = enc.encode(entry.content || '');
    if (bytes.byteLength > dataView.byteLength) {
      Atomics.store(metaView, SAB_PAYLOAD_LEN_INDEX, 0);
      Atomics.store(metaView, SAB_STATUS_INDEX, STATUS_TOO_LARGE);
      Atomics.notify(metaView, SAB_STATUS_INDEX);
      return;
    }
    dataView.set(bytes);
    Atomics.store(metaView, SAB_PAYLOAD_LEN_INDEX, bytes.byteLength);
    Atomics.store(metaView, SAB_STATUS_INDEX, STATUS_OK);
    Atomics.notify(metaView, SAB_STATUS_INDEX);
  } catch {
    Atomics.store(metaView, SAB_PAYLOAD_LEN_INDEX, 0);
    Atomics.store(metaView, SAB_STATUS_INDEX, STATUS_ERROR);
    Atomics.notify(metaView, SAB_STATUS_INDEX);
  }
}

async function syncExists(path) {
  try {
    const entry = await reqP(tx('readonly').get(normPath(path)));
    Atomics.store(metaView, SAB_PAYLOAD_LEN_INDEX, entry ? 1 : 0);
    Atomics.store(metaView, SAB_STATUS_INDEX, STATUS_OK);
    Atomics.notify(metaView, SAB_STATUS_INDEX);
  } catch {
    Atomics.store(metaView, SAB_PAYLOAD_LEN_INDEX, 0);
    Atomics.store(metaView, SAB_STATUS_INDEX, STATUS_ERROR);
    Atomics.notify(metaView, SAB_STATUS_INDEX);
  }
}

// ── Async ops (return via postMessage) ───────────────────────────

async function aMkdir(path) {
  path = normPath(path);
  if (path === '/') return;
  const parent = parentPath(path);
  if (parent !== '/') {
    const p = await reqP(tx('readonly').get(parent));
    if (!p) await aMkdir(parent);
  }
  const existing = await reqP(tx('readonly').get(path));
  if (!existing) {
    await reqP(tx('readwrite').put({
      path, type: 'folder', parent: parentPath(path), modified: Date.now(),
    }));
  }
}

async function aWrite(path, content) {
  path = normPath(path);
  const parent = parentPath(path);
  if (parent !== '/') await aMkdir(parent);
  await reqP(tx('readwrite').put({
    path, type: 'file', parent, content: content || '', modified: Date.now(),
  }));
}

async function aRead(path) {
  const entry = await reqP(tx('readonly').get(normPath(path)));
  if (!entry || entry.type !== 'file') return null;
  return entry.content;
}

async function aExists(path) {
  const entry = await reqP(tx('readonly').get(normPath(path)));
  return !!entry;
}

async function aRemove(path) {
  path = normPath(path);
  const all = await reqP(tx('readonly').getAll());
  const toDelete = all.filter((e) => e.path === path || e.path.startsWith(path + '/'));
  const wstore = tx('readwrite');
  for (const e of toDelete) wstore.delete(e.path);
  return new Promise((resolve, reject) => {
    wstore.transaction.oncomplete = () => resolve();
    wstore.transaction.onerror    = () => reject(wstore.transaction.error);
  });
}

async function aRename(oldP, newP) {
  oldP = normPath(oldP);
  newP = normPath(newP);
  const all = await reqP(tx('readonly').getAll());
  const toMove = all.filter((e) => e.path === oldP || e.path.startsWith(oldP + '/'));
  await aMkdir(parentPath(newP));
  const wstore = tx('readwrite');
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
}

async function aListTree() {
  const all = await reqP(tx('readonly').getAll());
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
}

async function aClear() { await reqP(tx('readwrite').clear()); }
async function aCount() { return await reqP(tx('readonly').count()); }

// ── Message dispatch ─────────────────────────────────────────────
// Serialize all requests through one promise chain so a slow async
// op doesn't race with a fresh sync request mid-IDB-transaction.

let queue = Promise.resolve();

self.onmessage = (e) => {
  queue = queue.then(() => handle(e.data)).catch((err) => {
    // Async ops with an id: forward the error to the caller. Sync ops
    // already wrote STATUS_ERROR inside their try/catch.
    if (e.data?.id != null) {
      self.postMessage({ id: e.data.id, error: err.message || String(err) });
    }
  });
};

async function handle(msg) {
  switch (msg.op) {
    case 'init': {
      const sab = msg.sab;
      metaView = new Int32Array(sab, 0, 4);
      dataView = new Uint8Array(sab, SAB_HEADER_SIZE);
      try {
        db = await openDB();
        self.postMessage({ op: 'ready' });
      } catch (err) {
        self.postMessage({ op: 'init-failed', error: err.message || String(err) });
      }
      return;
    }
    case 'sync-read':   await syncRead(msg.path); return;
    case 'sync-exists': await syncExists(msg.path); return;

    case 'write':     return reply(msg.id, () => aWrite(msg.path, msg.content));
    case 'mkdir':     return reply(msg.id, () => aMkdir(msg.path));
    case 'read':      return reply(msg.id, () => aRead(msg.path));
    case 'exists':    return reply(msg.id, () => aExists(msg.path));
    case 'remove':    return reply(msg.id, () => aRemove(msg.path));
    case 'rename':    return reply(msg.id, () => aRename(msg.oldPath, msg.newPath));
    case 'list-tree': return reply(msg.id, () => aListTree());
    case 'clear':     return reply(msg.id, () => aClear());
    case 'count':     return reply(msg.id, () => aCount());
  }
}

async function reply(id, fn) {
  try {
    const result = await fn();
    self.postMessage({ id, result });
  } catch (err) {
    self.postMessage({ id, error: err.message || String(err) });
  }
}
