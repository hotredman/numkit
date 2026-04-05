/**
 * vfs.js — Virtual File System backed by IndexedDB
 *
 * Stores files and folders for the MLab IDE.
 * Each entry: { path, type: 'file'|'folder', content?, modified }
 *
 * Usage:
 *   import vfs from './vfs';
 *   await vfs.init();
 *   await vfs.writeFile('/My Scripts/hello.m', 'disp("hello")');
 *   const code = await vfs.readFile('/My Scripts/hello.m');
 *   const tree = await vfs.listTree();
 */

const DB_NAME = 'mlab-vfs';
const DB_VERSION = 1;
const STORE_NAME = 'files';

let db = null;

function openDB() {
  return new Promise((resolve, reject) => {
    const req = indexedDB.open(DB_NAME, DB_VERSION);
    req.onupgradeneeded = (e) => {
      const db = e.target.result;
      if (!db.objectStoreNames.contains(STORE_NAME)) {
        const store = db.createObjectStore(STORE_NAME, { keyPath: 'path' });
        store.createIndex('type', 'type', { unique: false });
        store.createIndex('parent', 'parent', { unique: false });
      }
    };
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });
}

function tx(mode = 'readonly') {
  const t = db.transaction(STORE_NAME, mode);
  return t.objectStore(STORE_NAME);
}

function reqP(req) {
  return new Promise((resolve, reject) => {
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });
}

/** Get the parent path of a given path */
function parentPath(p) {
  const idx = p.lastIndexOf('/');
  if (idx <= 0) return '/';
  return p.substring(0, idx);
}

/** Normalize a path: remove trailing slash, ensure leading slash */
function normPath(p) {
  p = p.replace(/\/+/g, '/');
  if (!p.startsWith('/')) p = '/' + p;
  if (p.length > 1 && p.endsWith('/')) p = p.slice(0, -1);
  return p;
}

const vfs = {
  /** Initialize the database */
  async init() {
    db = await openDB();
  },

  /** Check if database is initialized and has any content */
  async isEmpty() {
    const store = tx('readonly');
    const count = await reqP(store.count());
    return count === 0;
  },

  /** Create a folder (and parent folders) */
  async mkdir(path) {
    path = normPath(path);
    if (path === '/') return;

    // Ensure parent exists
    const parent = parentPath(path);
    if (parent !== '/') {
      const parentEntry = await reqP(tx('readonly').get(parent));
      if (!parentEntry) await this.mkdir(parent);
    }

    const store = tx('readwrite');
    const existing = await reqP(store.get(path));
    if (!existing) {
      await reqP(tx('readwrite').put({
        path,
        type: 'folder',
        parent: parentPath(path),
        modified: Date.now(),
      }));
    }
  },

  /** Write a file (creates parent folders automatically) */
  async writeFile(path, content) {
    path = normPath(path);
    const parent = parentPath(path);
    await this.mkdir(parent);

    const store = tx('readwrite');
    await reqP(store.put({
      path,
      type: 'file',
      parent,
      content: content || '',
      modified: Date.now(),
    }));
  },

  /** Read file content. Returns null if not found */
  async readFile(path) {
    path = normPath(path);
    const entry = await reqP(tx('readonly').get(path));
    if (!entry || entry.type !== 'file') return null;
    return entry.content;
  },

  /** Check if a path exists */
  async exists(path) {
    path = normPath(path);
    const entry = await reqP(tx('readonly').get(path));
    return !!entry;
  },

  /** Delete a file or folder (and all children) */
  async remove(path) {
    path = normPath(path);
    const store = tx('readonly');
    const all = await reqP(store.getAll());

    const toDelete = all
      .filter(e => e.path === path || e.path.startsWith(path + '/'))
      .map(e => e.path);

    const wstore = tx('readwrite');
    for (const p of toDelete) {
      wstore.delete(p);
    }

    return new Promise((resolve, reject) => {
      wstore.transaction.oncomplete = () => resolve();
      wstore.transaction.onerror = () => reject(wstore.transaction.error);
    });
  },

  /** Rename/move a file or folder */
  async rename(oldPath, newPath) {
    oldPath = normPath(oldPath);
    newPath = normPath(newPath);

    const store = tx('readonly');
    const all = await reqP(store.getAll());

    const toMove = all.filter(e => e.path === oldPath || e.path.startsWith(oldPath + '/'));

    // Ensure new parent exists
    await this.mkdir(parentPath(newPath));

    const wstore = tx('readwrite');
    for (const entry of toMove) {
      // Delete old
      wstore.delete(entry.path);
      // Write new with updated path
      const suffix = entry.path.substring(oldPath.length);
      const updatedPath = newPath + suffix;
      wstore.put({
        ...entry,
        path: updatedPath,
        parent: parentPath(updatedPath),
        modified: Date.now(),
      });
    }

    return new Promise((resolve, reject) => {
      wstore.transaction.oncomplete = () => resolve();
      wstore.transaction.onerror = () => reject(wstore.transaction.error);
    });
  },

  /** List children of a folder (one level) */
  async listDir(path) {
    path = normPath(path);
    const store = tx('readonly');
    const index = store.index('parent');
    const entries = await reqP(index.getAll(path));
    return entries.map(e => ({
      name: e.path.split('/').pop(),
      path: e.path,
      type: e.type,
      modified: e.modified,
    })).sort((a, b) => {
      if (a.type !== b.type) return a.type === 'folder' ? -1 : 1;
      return a.name.localeCompare(b.name);
    });
  },

  /**
   * Build a full nested tree structure for UI rendering.
   * Returns: { name, path, type, children? }[]
   */
  async listTree() {
    const store = tx('readonly');
    const all = await reqP(store.getAll());

    // Build tree from flat list
    const root = [];
    const folderMap = { '/': root };

    // Sort so folders come first, then by path depth
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
      if (parentChildren) {
        parentChildren.push(node);
      } else {
        root.push(node);
      }
    }

    // Sort each level: folders first, then alphabetical
    function sortChildren(nodes) {
      nodes.sort((a, b) => {
        if (a.type !== b.type) return a.type === 'folder' ? -1 : 1;
        return a.name.localeCompare(b.name);
      });
      for (const n of nodes) {
        if (n.children) sortChildren(n.children);
      }
    }
    sortChildren(root);

    return root;
  },

  /** Clear all data */
  async clear() {
    const store = tx('readwrite');
    await reqP(store.clear());
  },

  /** Get total number of entries */
  async count() {
    const store = tx('readonly');
    return await reqP(store.count());
  },
};

export default vfs;
