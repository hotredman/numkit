/**
 * fs/local.js — real-filesystem backend for the MLab IDE.
 *
 * Uses the File System Access API (`window.showDirectoryPicker`).
 * Works in Chromium-family browsers and in the Electron desktop
 * shell (which bundles Chromium). Firefox and Safari do not have
 * the API — `isAvailable()` returns false there and the IDE hides
 * the "Local Folder" source.
 *
 * A single root directory is mounted at a time. The FileSystemDirectoryHandle
 * is persisted in IndexedDB so reopening the IDE restores the tree;
 * `reconnect()` re-requests permission on reload (the browser drops
 * active permissions between sessions).
 *
 * Public shape matches `temporary.js` so the UI can treat both as
 * one Filesystem interface:
 *   listTree() → TreeNode[]
 *   readFile(path), writeFile(path, content), exists(path)
 *   mkdir(path), remove(path), rename(oldPath, newPath)
 *
 * Extra, specific to this backend:
 *   isAvailable(), pickDirectory(), reconnect(), disconnect(),
 *   mountName() — UI helpers for the open/reconnect flow.
 */

const DB_NAME = 'mlab-local-fs';
const DB_VERSION = 1;
const STORE = 'handles';
const HANDLE_KEY = 'root';

let db = null;
let rootHandle = null;

// ── IndexedDB helpers ────────────────────────────────────────────

function openDB() {
  return new Promise((resolve, reject) => {
    const req = indexedDB.open(DB_NAME, DB_VERSION);
    req.onupgradeneeded = e => {
      const d = e.target.result;
      if (!d.objectStoreNames.contains(STORE)) d.createObjectStore(STORE);
    };
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });
}

async function ensureDB() {
  if (!db) db = await openDB();
  return db;
}

function idbGet(key) {
  return new Promise((resolve, reject) => {
    const tx = db.transaction(STORE, 'readonly');
    const req = tx.objectStore(STORE).get(key);
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });
}

function idbPut(key, val) {
  return new Promise((resolve, reject) => {
    const tx = db.transaction(STORE, 'readwrite');
    tx.objectStore(STORE).put(val, key);
    tx.oncomplete = () => resolve();
    tx.onerror = () => reject(tx.error);
  });
}

function idbDelete(key) {
  return new Promise((resolve, reject) => {
    const tx = db.transaction(STORE, 'readwrite');
    tx.objectStore(STORE).delete(key);
    tx.oncomplete = () => resolve();
    tx.onerror = () => reject(tx.error);
  });
}

// ── Path helpers ─────────────────────────────────────────────────

/** Normalise a VFS-style path: "/foo/bar" → ["foo","bar"]. */
function splitPath(path) {
  return path.replace(/^\/+/, '').split('/').filter(Boolean);
}

function joinPath(parent, name) {
  if (!parent || parent === '/') return '/' + name;
  return parent + '/' + name;
}

function parentPath(p) {
  const idx = p.lastIndexOf('/');
  if (idx <= 0) return '/';
  return p.substring(0, idx);
}

// ── Handle navigation ────────────────────────────────────────────

/**
 * Walk the root handle to the directory at `path`. If `create` is
 * true, missing intermediate directories are created.
 * Returns the DirectoryHandle.
 */
async function resolveDir(path, { create = false } = {}) {
  if (!rootHandle) throw new Error('No folder mounted');
  let cur = rootHandle;
  for (const part of splitPath(path)) {
    cur = await cur.getDirectoryHandle(part, { create });
  }
  return cur;
}

/**
 * Resolve the parent directory of `path` and return the last path
 * segment so the caller can getFileHandle/removeEntry with it.
 */
async function resolveParent(path, { create = false } = {}) {
  const parts = splitPath(path);
  if (parts.length === 0) throw new Error('Cannot resolve root as file');
  const name = parts.pop();
  let cur = rootHandle;
  for (const part of parts) {
    cur = await cur.getDirectoryHandle(part, { create });
  }
  return { parent: cur, name };
}

// ── Permissions ──────────────────────────────────────────────────

async function verifyPermission(handle, mode = 'readwrite') {
  if (!handle.queryPermission) return true; // very old browsers
  const opts = { mode };
  if ((await handle.queryPermission(opts)) === 'granted') return true;
  if ((await handle.requestPermission(opts)) === 'granted') return true;
  return false;
}

// ── Public API ───────────────────────────────────────────────────

const localFS = {
  /** Whether the File System Access API is present at all. */
  isAvailable() {
    return typeof window !== 'undefined'
      && typeof window.showDirectoryPicker === 'function';
  },

  /** Name of the currently-mounted folder, or null if none. */
  mountName() {
    return rootHandle ? rootHandle.name : null;
  },

  isMounted() {
    return rootHandle !== null;
  },

  /**
   * Prompt the user to pick a folder. Replaces any currently-mounted
   * folder. Returns the folder name on success, null if the user
   * cancelled.
   */
  async pickDirectory() {
    if (!this.isAvailable()) throw new Error('File System Access API not available');
    let handle;
    try {
      handle = await window.showDirectoryPicker({ mode: 'readwrite' });
    } catch (err) {
      if (err && err.name === 'AbortError') return null;
      throw err;
    }
    if (!await verifyPermission(handle, 'readwrite')) {
      throw new Error('Read/write permission denied for folder');
    }
    rootHandle = handle;
    await ensureDB();
    await idbPut(HANDLE_KEY, handle);
    return handle.name;
  },

  /**
   * Try to restore the previously-mounted folder from IndexedDB and
   * re-request permission. Returns the folder name on success, or
   * null if no stored handle or permission was denied.
   */
  async reconnect() {
    if (!this.isAvailable()) return null;
    await ensureDB();
    const stored = await idbGet(HANDLE_KEY);
    if (!stored) return null;
    if (!await verifyPermission(stored, 'readwrite')) return null;
    rootHandle = stored;
    return stored.name;
  },

  /** Forget the current folder and its stored handle. */
  async disconnect() {
    rootHandle = null;
    await ensureDB();
    await idbDelete(HANDLE_KEY);
  },

  // ── Filesystem ops mirroring temporary.js ──

  async listTree() {
    if (!rootHandle) return [];
    const root = [];
    async function walk(dirHandle, path, bucket) {
      const entries = [];
      for await (const [name, handle] of dirHandle.entries()) {
        const itemPath = joinPath(path, name);
        const node = { name, path: itemPath, type: handle.kind === 'directory' ? 'folder' : 'file' };
        if (handle.kind === 'directory') {
          node.children = [];
          await walk(handle, itemPath, node.children);
        }
        entries.push(node);
      }
      entries.sort((a, b) => {
        if (a.type !== b.type) return a.type === 'folder' ? -1 : 1;
        return a.name.localeCompare(b.name);
      });
      bucket.push(...entries);
    }
    await walk(rootHandle, '', root);
    return root;
  },

  async readFile(path) {
    if (!rootHandle) return null;
    try {
      const { parent, name } = await resolveParent(path);
      const fh = await parent.getFileHandle(name);
      const file = await fh.getFile();
      return await file.text();
    } catch (err) {
      if (err && (err.name === 'NotFoundError' || err.name === 'TypeMismatchError')) return null;
      throw err;
    }
  },

  async writeFile(path, content) {
    const { parent, name } = await resolveParent(path, { create: true });
    const fh = await parent.getFileHandle(name, { create: true });
    const writable = await fh.createWritable();
    await writable.write(content ?? '');
    await writable.close();
  },

  async mkdir(path) {
    if (!path || path === '/') return;
    await resolveDir(path, { create: true });
  },

  async exists(path) {
    if (!rootHandle) return false;
    try {
      const { parent, name } = await resolveParent(path);
      try { await parent.getFileHandle(name); return true; } catch (_) {}
      try { await parent.getDirectoryHandle(name); return true; } catch (_) {}
      return false;
    } catch (_) {
      return false;
    }
  },

  async remove(path) {
    const { parent, name } = await resolveParent(path);
    await parent.removeEntry(name, { recursive: true });
  },

  /**
   * Rename within the same parent folder. Cross-folder moves would
   * require copying; we keep it simple and refuse them for now.
   */
  async rename(oldPath, newPath) {
    const oldParts = splitPath(oldPath);
    const newParts = splitPath(newPath);
    if (oldParts.length !== newParts.length) {
      throw new Error('Cross-folder rename is not supported yet');
    }
    for (let i = 0; i < oldParts.length - 1; i++) {
      if (oldParts[i] !== newParts[i])
        throw new Error('Cross-folder rename is not supported yet');
    }
    const { parent, name: oldName } = await resolveParent(oldPath);
    const newName = newParts[newParts.length - 1];
    if (oldName === newName) return;

    // No native rename in FSA — copy then delete.
    const srcFile = await parent.getFileHandle(oldName).catch(() => null);
    if (srcFile) {
      const content = await (await srcFile.getFile()).text();
      const dst = await parent.getFileHandle(newName, { create: true });
      const w = await dst.createWritable();
      await w.write(content);
      await w.close();
      await parent.removeEntry(oldName);
      return;
    }
    const srcDir = await parent.getDirectoryHandle(oldName).catch(() => null);
    if (srcDir) {
      // Recursive copy directory — rare path; keep it simple.
      async function copyDir(from, toParent, toName) {
        const to = await toParent.getDirectoryHandle(toName, { create: true });
        for await (const [nm, h] of from.entries()) {
          if (h.kind === 'file') {
            const content = await (await h.getFile()).text();
            const fh = await to.getFileHandle(nm, { create: true });
            const w = await fh.createWritable();
            await w.write(content);
            await w.close();
          } else {
            await copyDir(h, to, nm);
          }
        }
      }
      await copyDir(srcDir, parent, newName);
      await parent.removeEntry(oldName, { recursive: true });
      return;
    }
    throw new Error('Rename source not found');
  },

  /** Clear is a no-op here — we never want to wipe the user's real disk. */
  async clear() {
    /* intentionally unsupported */
  },
};

export default localFS;
