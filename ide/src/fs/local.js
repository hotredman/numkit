/**
 * fs/local.js — real-filesystem backend for the numkit mIDE.
 *
 * Two interchangeable backends with the same public shape:
 *
 *  NATIVE  — Electron desktop. Uses window.nativeFS exposed by the
 *            shell's preload.js (see ide/desktop/preload.js). Paths
 *            are OS absolute paths; every operation round-trips
 *            through ipcMain, which enforces a sandbox inside the
 *            chosen mount root.
 *
 *  FSA     — Web (Chromium-family browsers). Uses window.showDirectoryPicker
 *            and FileSystemDirectoryHandle. Sandboxed by the browser;
 *            no OS paths, no "reveal in explorer" possible.
 *
 * Selection happens once at module load: if window.nativeFS is present
 * we're in the Electron shell and use it; else if showDirectoryPicker
 * exists we fall back to FSA; else the backend is unavailable and the
 * Local Folder source is hidden from the UI.
 *
 * Public shape (mirrors temporary.js where it overlaps):
 *   isAvailable, supportsReveal,
 *   isMounted, mountName,
 *   pickDirectory, reconnect, disconnect,
 *   listTree, readFile, writeFile, mkdir, exists, remove, rename, clear,
 *   revealInExplorer, showItemInFolder.
 */

// ─────────────────────────────────────────────────────────────────
// Native backend — Electron desktop
// ─────────────────────────────────────────────────────────────────
//
// Persists the root folder path in localStorage ("numkit.mide.native.root")
// so reloading the IDE reopens the same folder without asking again.
// No permission handshake is needed — the main process runs with
// the user's OS permissions.

const NATIVE_ROOT_KEY = 'numkit.mide.native.root';

function makeNativeBackend() {
    const api = window.nativeFS;
    let rootPath = null;
    let rootDisplayName = null;

    function stored() {
        try { return localStorage.getItem(NATIVE_ROOT_KEY); }
        catch (_) { return null; }
    }
    function remember(p) {
        try { if (p) localStorage.setItem(NATIVE_ROOT_KEY, p); else localStorage.removeItem(NATIVE_ROOT_KEY); }
        catch (_) {}
    }
    function setRoot(p) {
        rootPath = p;
        if (p) {
            // Last segment of the path — works for Win and POSIX.
            const parts = p.split(/[\\/]/).filter(Boolean);
            rootDisplayName = parts.length ? parts[parts.length - 1] : p;
        } else {
            rootDisplayName = null;
        }
    }

    return {
        kind: 'native',
        isAvailable: () => true,
        supportsReveal: () => true,

        isMounted: () => rootPath !== null,
        mountName: () => rootDisplayName,

        async pickDirectory() {
            const picked = await api.pickDirectory();
            if (!picked) return null;
            setRoot(picked);
            remember(picked);
            return rootDisplayName;
        },

        async reconnect() {
            const saved = stored();
            if (!saved) return null;
            // Verify the path is still reachable — avoids surfacing a
            // stale mount after the folder was renamed/removed from disk.
            try {
                await api.listTree(saved);
                setRoot(saved);
                return rootDisplayName;
            } catch (_) {
                remember(null);
                return null;
            }
        },

        async disconnect() {
            setRoot(null);
            remember(null);
        },

        async listTree() {
            if (!rootPath) return [];
            return api.listTree(rootPath);
        },
        async readFile(p)                { return rootPath ? api.readFile(rootPath, p) : null; },
        async writeFile(p, content)      { return api.writeFile(rootPath, p, content); },
        async mkdir(p)                   { return api.mkdir(rootPath, p); },
        async exists(p)                  { return rootPath ? api.exists(rootPath, p) : false; },
        async remove(p)                  { return api.remove(rootPath, p); },
        async rename(oldP, newP)         { return api.rename(rootPath, oldP, newP); },

        async revealInExplorer(p)        {
            if (!rootPath) return;
            return api.revealInExplorer(rootPath, p || '');
        },
        async showItemInFolder(p)        {
            if (!rootPath) return;
            return api.showItemInFolder(rootPath, p);
        },

        async clear() { /* never wipe a real disk */ },
    };
}

// ─────────────────────────────────────────────────────────────────
// FSA backend — Chromium web
// ─────────────────────────────────────────────────────────────────
//
// Stores the FileSystemDirectoryHandle in IndexedDB so reloading
// preserves the mount across page reloads (still needs a permission
// re-grant from the user).

const DB_NAME = 'numkit-mide-local-fs';
const LEGACY_DB_NAME = 'mlab-local-fs';
const DB_VERSION = 1;
const STORE = 'handles';
const HANDLE_KEY = 'root';

// One-shot migration of the persisted directory handle from the legacy
// DB. Runs only when the new DB has no handle yet and the old one does.
async function migrateLegacyLocalFs(newDb) {
    const dbs = await (indexedDB.databases ? indexedDB.databases() : Promise.resolve([]));
    if (!dbs.some(d => d.name === LEGACY_DB_NAME)) return;

    const hasNew = await new Promise(r => {
        const req = newDb.transaction(STORE, 'readonly').objectStore(STORE).get(HANDLE_KEY);
        req.onsuccess = () => r(!!req.result);
        req.onerror = () => r(false);
    });
    if (hasNew) return;

    const oldDb = await new Promise(r => {
        const req = indexedDB.open(LEGACY_DB_NAME, DB_VERSION);
        req.onsuccess = () => r(req.result);
        req.onerror = () => r(null);
    });
    if (!oldDb) return;

    const handle = await new Promise(r => {
        try {
            const req = oldDb.transaction(STORE, 'readonly').objectStore(STORE).get(HANDLE_KEY);
            req.onsuccess = () => r(req.result);
            req.onerror = () => r(null);
        } catch { r(null); }
    });
    oldDb.close();

    if (handle) {
        const tx = newDb.transaction(STORE, 'readwrite');
        tx.objectStore(STORE).put(handle, HANDLE_KEY);
        await new Promise(r => { tx.oncomplete = r; tx.onerror = r; });
    }
    indexedDB.deleteDatabase(LEGACY_DB_NAME);
}

function makeFsaBackend() {
    let db = null;
    let rootHandle = null;

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
        if (!db) {
            db = await openDB();
            try { await migrateLegacyLocalFs(db); } catch {}
        }
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

    function splitPath(p) { return p.replace(/^\/+/, '').split('/').filter(Boolean); }
    function joinPath(parent, name) { return (!parent || parent === '/') ? '/' + name : parent + '/' + name; }

    async function resolveDir(p, { create = false } = {}) {
        if (!rootHandle) throw new Error('No folder mounted');
        let cur = rootHandle;
        for (const part of splitPath(p)) cur = await cur.getDirectoryHandle(part, { create });
        return cur;
    }
    async function resolveParent(p, { create = false } = {}) {
        const parts = splitPath(p);
        if (parts.length === 0) throw new Error('Cannot resolve root as file');
        const name = parts.pop();
        let cur = rootHandle;
        for (const part of parts) cur = await cur.getDirectoryHandle(part, { create });
        return { parent: cur, name };
    }

    async function verifyPermission(handle, mode = 'readwrite') {
        if (!handle.queryPermission) return true;
        const opts = { mode };
        if ((await handle.queryPermission(opts)) === 'granted') return true;
        if ((await handle.requestPermission(opts)) === 'granted') return true;
        return false;
    }

    return {
        kind: 'fsa',
        isAvailable: () => typeof window.showDirectoryPicker === 'function',
        supportsReveal: () => false,

        isMounted: () => rootHandle !== null,
        mountName: () => (rootHandle ? rootHandle.name : null),

        async pickDirectory() {
            let handle;
            try { handle = await window.showDirectoryPicker({ mode: 'readwrite' }); }
            catch (err) { if (err?.name === 'AbortError') return null; throw err; }
            if (!await verifyPermission(handle, 'readwrite'))
                throw new Error('Read/write permission denied for folder');
            rootHandle = handle;
            await ensureDB();
            await idbPut(HANDLE_KEY, handle);
            return handle.name;
        },

        async reconnect() {
            await ensureDB();
            const stored = await idbGet(HANDLE_KEY);
            if (!stored) return null;
            if (!await verifyPermission(stored, 'readwrite')) return null;
            rootHandle = stored;
            return stored.name;
        },

        async disconnect() {
            rootHandle = null;
            await ensureDB();
            await idbDelete(HANDLE_KEY);
        },

        async listTree() {
            if (!rootHandle) return [];
            const root = [];
            async function walk(dirHandle, pathStr, bucket) {
                const entries = [];
                for await (const [name, handle] of dirHandle.entries()) {
                    const itemPath = joinPath(pathStr, name);
                    const node = { name, path: itemPath, type: handle.kind === 'directory' ? 'folder' : 'file' };
                    if (handle.kind === 'directory') {
                        node.children = [];
                        await walk(handle, itemPath, node.children);
                    }
                    entries.push(node);
                }
                entries.sort((a, b) => (a.type !== b.type) ? (a.type === 'folder' ? -1 : 1) : a.name.localeCompare(b.name));
                bucket.push(...entries);
            }
            await walk(rootHandle, '', root);
            return root;
        },

        async readFile(p) {
            if (!rootHandle) return null;
            try {
                const { parent, name } = await resolveParent(p);
                const fh = await parent.getFileHandle(name);
                return await (await fh.getFile()).text();
            } catch (err) {
                if (err && (err.name === 'NotFoundError' || err.name === 'TypeMismatchError')) return null;
                throw err;
            }
        },

        async writeFile(p, content) {
            const { parent, name } = await resolveParent(p, { create: true });
            const fh = await parent.getFileHandle(name, { create: true });
            const w = await fh.createWritable();
            await w.write(content ?? '');
            await w.close();
        },

        async mkdir(p) { if (!p || p === '/') return; await resolveDir(p, { create: true }); },

        async exists(p) {
            if (!rootHandle) return false;
            try {
                const { parent, name } = await resolveParent(p);
                try { await parent.getFileHandle(name); return true; } catch (_) {}
                try { await parent.getDirectoryHandle(name); return true; } catch (_) {}
                return false;
            } catch (_) { return false; }
        },

        async remove(p) {
            const { parent, name } = await resolveParent(p);
            await parent.removeEntry(name, { recursive: true });
        },

        async rename(oldPath, newPath) {
            const oldParts = splitPath(oldPath);
            const newParts = splitPath(newPath);
            if (oldParts.length !== newParts.length) throw new Error('Cross-folder rename is not supported yet');
            for (let i = 0; i < oldParts.length - 1; i++) {
                if (oldParts[i] !== newParts[i]) throw new Error('Cross-folder rename is not supported yet');
            }
            const { parent, name: oldName } = await resolveParent(oldPath);
            const newName = newParts[newParts.length - 1];
            if (oldName === newName) return;
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

        // Reveal is not possible in the FSA world — handles hide OS paths
        // and browsers can't launch the file manager. These are no-ops so
        // callers don't have to branch; the UI also gates on supportsReveal.
        async revealInExplorer(_p) { /* unsupported in FSA */ },
        async showItemInFolder(_p) { /* unsupported in FSA */ },

        async clear() { /* never wipe a real disk */ },
    };
}

// ─────────────────────────────────────────────────────────────────
// Dispatcher
// ─────────────────────────────────────────────────────────────────

function pickBackend() {
    if (typeof window === 'undefined') return null;
    if (window.nativeFS) return makeNativeBackend();
    if (typeof window.showDirectoryPicker === 'function') return makeFsaBackend();
    return null;
}

const backend = pickBackend();

const unavailable = {
    kind: 'none',
    isAvailable: () => false,
    supportsReveal: () => false,
    isMounted: () => false,
    mountName: () => null,
    async pickDirectory() { throw new Error('Local Folder not available in this browser'); },
    async reconnect() { return null; },
    async disconnect() {},
    async listTree() { return []; },
    async readFile() { return null; },
    async writeFile() {},
    async mkdir() {},
    async exists() { return false; },
    async remove() {},
    async rename() {},
    async revealInExplorer() {},
    async showItemInFolder() {},
    async clear() {},
};

const localFS = backend || unavailable;
export default localFS;
