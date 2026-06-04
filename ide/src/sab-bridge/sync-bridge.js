// sync-bridge.js — main-thread half of the async-to-sync bridge that
// fronts the tempFS IndexedDB worker. Exposes:
//   - readSync(path)   sync, blocks via Atomics.wait until worker notifies
//   - existsSync(path) ditto
//   - {async}: read / write / mkdir / exists / remove / rename / listTree
//              / clear / count — round-trip postMessage promises
//
// The bridge is a no-op (returns null from makeBridge()) when
// SharedArrayBuffer / crossOriginIsolated isn't available — the
// vfs-adapter falls back to seed-on-init in that case (web build).

import {
  SAB_TOTAL_SIZE, SAB_HEADER_SIZE,
  SAB_STATUS_INDEX, SAB_PAYLOAD_LEN_INDEX,
  STATUS_WAITING, STATUS_OK, STATUS_NOT_FOUND, STATUS_TOO_LARGE, STATUS_ERROR,
} from './sab-protocol';

// Anything tighter than 5 s would fire on a slow IDB write (a thousand-
// entry transaction can take 1-2 s on cold disk). Anything looser than
// 30 s makes the WASM REPL appear hung when the worker actually wedged.
const SYNC_TIMEOUT_MS = 5000;
// Drop-dead for the worker's initial IDB open. Slow disks / migration
// can take a couple seconds; 10 s leaves plenty of headroom.
const INIT_TIMEOUT_MS = 10000;

const dec = new TextDecoder();

/**
 * Returns true when the runtime can support a synchronous bridge.
 * Requires SharedArrayBuffer + Atomics.wait + crossOriginIsolated
 * (or a runtime that pre-dates that requirement entirely).
 */
export function isSupported() {
  if (typeof SharedArrayBuffer === 'undefined') return false;
  if (typeof Atomics === 'undefined') return false;
  if (typeof Atomics.wait !== 'function') return false;
  // crossOriginIsolated is only meaningful inside a window/worker
  // global; treat undefined as "old runtime, allow it".
  if (typeof self !== 'undefined' && self.crossOriginIsolated === false) return false;
  return true;
}

/**
 * Spawn the tempFS worker and return a bridge handle, or null when
 * the runtime can't support sync calls (web without
 * crossOriginIsolated). The caller must `await ready()` before
 * issuing requests.
 */
export function makeBridge() {
  if (!isSupported()) return null;

  const sab = new SharedArrayBuffer(SAB_TOTAL_SIZE);
  const metaView = new Int32Array(sab, 0, 4);
  const dataView = new Uint8Array(sab, SAB_HEADER_SIZE);

  const worker = new Worker(new URL('./temporary-worker.js', import.meta.url),
                            { type: 'module' });

  let asyncId = 0;
  const pending = new Map();   // id → { resolve, reject }

  // Single onmessage handler — routes ready/init-failed to the init
  // promise, reply messages to their pending request ID.
  let resolveReady;
  let rejectReady;
  const readyPromise = new Promise((resolve, reject) => {
    resolveReady = resolve;
    rejectReady  = reject;
  });
  const initTimer = setTimeout(() => {
    rejectReady(new Error('vfs worker init timeout'));
  }, INIT_TIMEOUT_MS);

  worker.onmessage = (e) => {
    const msg = e.data;
    if (msg.op === 'ready') {
      clearTimeout(initTimer);
      resolveReady();
      return;
    }
    if (msg.op === 'init-failed') {
      clearTimeout(initTimer);
      rejectReady(new Error(msg.error || 'vfs worker init failed'));
      return;
    }
    if (msg.id != null && pending.has(msg.id)) {
      const { resolve, reject } = pending.get(msg.id);
      pending.delete(msg.id);
      if (msg.error) reject(new Error(msg.error));
      else           resolve(msg.result);
    }
  };
  worker.onerror = (e) => {
    // A worker-level error (parse error, uncaught throw) should reject
    // every pending async call so the UI doesn't hang on a dead worker.
    const err = new Error(`vfs worker error: ${e.message || e.filename}`);
    for (const { reject } of pending.values()) reject(err);
    pending.clear();
    rejectReady(err);
  };

  worker.postMessage({ op: 'init', sab });

  function syncCall(op, payload) {
    Atomics.store(metaView, SAB_STATUS_INDEX, STATUS_WAITING);
    worker.postMessage({ op, ...payload });
    const r = Atomics.wait(metaView, SAB_STATUS_INDEX, STATUS_WAITING, SYNC_TIMEOUT_MS);
    if (r === 'timed-out') {
      throw new Error(`vfs ${op} timed out after ${SYNC_TIMEOUT_MS} ms`);
    }
    return Atomics.load(metaView, SAB_STATUS_INDEX);
  }

  function asyncCall(op, payload = {}) {
    const id = ++asyncId;
    return new Promise((resolve, reject) => {
      pending.set(id, { resolve, reject });
      worker.postMessage({ op, id, ...payload });
    });
  }

  return {
    ready() { return readyPromise; },

    // Sync hooks for the WASM engine ───────────────────────────────
    readSync(path) {
      const status = syncCall('sync-read', { path });
      if (status === STATUS_NOT_FOUND) return null;
      if (status === STATUS_TOO_LARGE) {
        throw new Error(`vfs file too large for sync read: ${path}`);
      }
      if (status === STATUS_ERROR) {
        throw new Error(`vfs read error: ${path}`);
      }
      if (status !== STATUS_OK) {
        throw new Error(`vfs unexpected status ${status} for ${path}`);
      }
      const len = Atomics.load(metaView, SAB_PAYLOAD_LEN_INDEX);
      return dec.decode(dataView.subarray(0, len));
    },
    existsSync(path) {
      const status = syncCall('sync-exists', { path });
      if (status !== STATUS_OK) return false;
      return Atomics.load(metaView, SAB_PAYLOAD_LEN_INDEX) === 1;
    },

    // Async ops (UI side) ─────────────────────────────────────────
    read     (path)              { return asyncCall('read',      { path }); },
    write    (path, content)     { return asyncCall('write',     { path, content }); },
    mkdir    (path)              { return asyncCall('mkdir',     { path }); },
    exists   (path)              { return asyncCall('exists',    { path }); },
    remove   (path)              { return asyncCall('remove',    { path }); },
    rename   (oldPath, newPath)  { return asyncCall('rename',    { oldPath, newPath }); },
    listTree ()                  { return asyncCall('list-tree'); },
    clear    ()                  { return asyncCall('clear'); },
    count    ()                  { return asyncCall('count'); },
  };
}
