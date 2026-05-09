// sab-protocol.js — wire format for the SharedArrayBuffer-based
// async-to-sync bridge between the renderer main thread and the
// tempFS worker. Both sides must agree on offsets + status codes.
//
// Layout (16 MB total):
//   bytes 0..3   Int32  status   (atomic)
//   bytes 4..7   Int32  payloadLen (bytes of UTF-8 payload)
//   bytes 8..15  Int32  reserved
//   bytes 16..   Uint8  payload  (UTF-8 encoded content)
//
// Sync protocol:
//   1. main:   Atomics.store(status, STATUS_WAITING)
//   2. main:   worker.postMessage({op:'sync-read', path})
//   3. main:   Atomics.wait(status, STATUS_WAITING, TIMEOUT)   ← blocks
//   4. worker: process op, write payload + payloadLen
//   5. worker: Atomics.store(status, STATUS_OK | NOT_FOUND | …)
//   6. worker: Atomics.notify(status)
//   7. main:   reads status + payload, returns to caller
//
// 16 MB payload cap mirrors the Electron sync-IPC cap; tempFS files
// realistically never approach this. Files larger than the SAB
// surface STATUS_TOO_LARGE so the caller can fall back to async.

export const SAB_HEADER_SIZE  = 16;                        // bytes
export const SAB_PAYLOAD_SIZE = 16 * 1024 * 1024;          // 16 MB
export const SAB_TOTAL_SIZE   = SAB_HEADER_SIZE + SAB_PAYLOAD_SIZE;

// Int32 indices into a `new Int32Array(sab, 0, 4)` view.
export const SAB_STATUS_INDEX      = 0;
export const SAB_PAYLOAD_LEN_INDEX = 1;

// Status codes. STATUS_WAITING is the "still pending" sentinel that
// Atomics.wait spins on; the worker must store any other value to
// release the wait.
export const STATUS_WAITING   = 0;
export const STATUS_OK        = 1;
export const STATUS_NOT_FOUND = 2;
export const STATUS_TOO_LARGE = 3;
export const STATUS_ERROR     = 4;
