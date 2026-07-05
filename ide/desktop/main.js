// main.js — Numkit IDE desktop shell
// Supports two modes:
//   Dev mode:  spawns Vite dev server, loads from http://
//   Prod mode: loads pre-built static files from dist/
const { app, BrowserWindow, dialog, shell, ipcMain, session } = require('electron');
const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');
const fsp = require('fs/promises');
const http = require('http');

// Bump V8's renderer-process heap limit. The default on 64-bit
// Chromium is roughly 4 GB but actual cap depends on the Electron
// build and OS — empirically the Numkit renderer was hitting OOM
// well below 2 GB. Bumping to 4 GB gives breathing room for big
// scripts (large matrices, long REPL sessions); WASM still grows
// inside this limit. `--js-flags` MUST be set before app.ready, so
// we set it at module top.
app.commandLine.appendSwitch('js-flags', '--max-old-space-size=4096 --expose-gc');

// Never serve a stale renderer. Chromium can disk-cache the file:// bundle
// across launches, so after a rebuild `run-desktop` could reload the OLD code
// even though desktop/dist is fresh ("run launches an old build"). Disabling
// the HTTP cache — and clearing it on startup below — guarantees every launch
// loads the current desktop/dist. Negligible cost for a local IDE.
app.commandLine.appendSwitch('disable-http-cache');

// ── File-based main-process logger ──────────────────────────────
// electron-builder packs `--win portable` exes as the GUI subsystem,
// so stdout/stderr are detached from any launching terminal. The
// renderer's [heap-trace] is visible in DevTools, but [mem-trace]
// from the main process otherwise has nowhere to go.
//
// Write to a STABLE per-user location, never next to the exe:
// portable mode extracts the exe into %TEMP%/<random>/ on launch
// and deletes that whole directory at exit, so a log file there
// vanishes the moment the process dies. app.getPath('userData')
// resolves to %APPDATA%/<productName>/ on Windows and survives
// across launches and crashes.
const LOG_DIR = app.isPackaged ? app.getPath('userData') : __dirname;
fs.mkdirSync(LOG_DIR, { recursive: true });
const LOG_PATH = path.join(LOG_DIR, 'numkit-ide.log');
function logToFile(level, ...args) {
  try {
    const line = `[${new Date().toISOString()}] [${level}] ${args.map(a =>
      typeof a === 'string' ? a : JSON.stringify(a)
    ).join(' ')}\n`;
    fs.appendFileSync(LOG_PATH, line);
  } catch { /* disk full, AV blocked write, etc. — silent */ }
}
// Rotate-on-launch: truncate if > 5 MB so the log doesn't grow forever.
try {
  const st = fs.statSync(LOG_PATH);
  if (st.size > 5 * 1024 * 1024) fs.truncateSync(LOG_PATH, 0);
} catch { /* file doesn't exist — fine, will be created on first append */ }
logToFile('boot', `Numkit IDE main starting; LOG_PATH=${LOG_PATH}`);

// Mirror console.{log,warn,error} to the file so [mem-trace] /
// renderer-gone diagnostics are captured even when the terminal
// can't see stdout.
const origLog = console.log.bind(console);
const origWarn = console.warn.bind(console);
const origErr = console.error.bind(console);
console.log = (...a) => { logToFile('log', ...a); origLog(...a); };
console.warn = (...a) => { logToFile('warn', ...a); origWarn(...a); };
console.error = (...a) => { logToFile('error', ...a); origErr(...a); };

const IDE_DIR = path.resolve(__dirname, '..');
const DIST_DIR = path.join(__dirname, 'dist');
const IS_PROD = fs.existsSync(path.join(DIST_DIR, 'index.html'));
const PRELOAD = path.join(__dirname, 'preload.js');

let mainWindow = null;
let viteProcess = null;

// Single-instance guard. A second `run-desktop` should focus + reload the
// EXISTING window (picking up a rebuilt desktop/dist) rather than spawn a
// second process that contends with the first for the userData lock and dies
// ("Unable to move the cache: Access is denied") — which would leave the user
// staring at the ORIGINAL, now-stale window. Without this, relaunching never
// actually replaces the old build.
if (!app.requestSingleInstanceLock()) {
  app.quit();
} else {
  app.on('second-instance', () => {
    if (mainWindow) {
      if (mainWindow.isMinimized()) mainWindow.restore();
      mainWindow.focus();
      mainWindow.webContents.reloadIgnoringCache();
    }
  });
}

function createWindow(url) {
  mainWindow = new BrowserWindow({
    width: 1400,
    height: 900,
    title: 'Numkit IDE',
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      preload: PRELOAD,
    },
  });

  console.log('[Numkit IDE] Loading:', url);
  if (url.startsWith('http')) {
    mainWindow.loadURL(url);
  } else {
    mainWindow.loadFile(url);
  }

  // Renderer crash / hang surfaces — Electron used to silently swap to a
  // blank white page. Catch both classes of failure here so the user
  // gets a real dialog instead of guessing whether the app froze.
  // Periodic native-side memory probe. The renderer's V8 heap is small
  // (the in-renderer [heap-trace] shows js=69 wasm=0) yet Chromium
  // reports OOM and kills the process — that means the leak is in
  // memory regions performance.memory cannot see (typed arrays, GPU
  // textures, IPC buffers, etc.). app.getAppMetrics() returns per-
  // process working-set + private bytes from Windows itself, which
  // gives us the ground-truth view.
  const metricsTimer = setInterval(() => {
    if (!mainWindow || mainWindow.isDestroyed()) return;
    try {
      const all = app.getAppMetrics();
      const renderer = all.find((m) => m.type === 'Tab' || m.type === 'renderer'
                                    || (m.serviceName && m.serviceName.includes('Renderer')));
      const fmt = (b) => Math.round(b / 1048576) + ' MB';
      const lines = all.map((m) => `${m.type || 'unknown'}#${m.pid} ws=${fmt(m.memory?.workingSetSize * 1024 || 0)} priv=${fmt(m.memory?.privateBytes * 1024 || 0)}`);
      console.log('[mem-trace]', lines.join(' | '));
    } catch (e) {
      console.warn('[mem-trace] failed:', e.message);
    }
  }, 30000);
  mainWindow.on('closed', () => { clearInterval(metricsTimer); });

  mainWindow.webContents.on('render-process-gone', (_e, details) => {
    // Capture one final memory snapshot before the process is gone
    try {
      const all = app.getAppMetrics();
      const fmt = (b) => Math.round(b / 1048576) + ' MB';
      console.error('[mem-trace at-crash]',
        all.map((m) => `${m.type || 'unknown'}#${m.pid} ws=${fmt(m.memory?.workingSetSize * 1024 || 0)}`).join(' | '));
    } catch { /* ignore */ }
    console.error('[Numkit IDE] Renderer gone:', details);
    if (mainWindow && !mainWindow.isDestroyed()) {
      dialog.showMessageBox(mainWindow, {
        type: 'error',
        title: 'Numkit IDE — renderer crashed',
        message: `Renderer process exited unexpectedly (reason: ${details.reason}, exitCode: ${details.exitCode}).`,
        detail: 'Likely an out-of-memory event. The window can be reloaded; if it keeps happening, try clearing the console or running fewer figures at once.',
        buttons: ['Reload', 'Quit'],
        defaultId: 0,
        cancelId: 1,
      }).then((res) => {
        if (res.response === 0 && mainWindow && !mainWindow.isDestroyed()) {
          mainWindow.reload();
        } else {
          app.quit();
        }
      });
    }
  });
  mainWindow.webContents.on('unresponsive', () => {
    console.warn('[Numkit IDE] Renderer unresponsive — likely a long synchronous WASM call or runaway loop');
  });
  mainWindow.webContents.on('responsive', () => {
    console.log('[Numkit IDE] Renderer responsive again');
  });

  mainWindow.on('closed', () => { mainWindow = null; });
}

// ── Dev mode: start Vite and detect URL ──

function startVite() {
  const NODE_EXE = path.join(process.env.PROGRAMFILES || 'C:\\Program Files', 'nodejs', 'node.exe');
  const VITE_BIN = path.join(IDE_DIR, 'node_modules', 'vite', 'bin', 'vite.js');

  return new Promise((resolve, reject) => {
    console.log('[Numkit IDE] Dev mode — starting Vite');

    viteProcess = spawn(NODE_EXE, [VITE_BIN, '--host', '127.0.0.1'], {
      cwd: IDE_DIR,
      stdio: ['ignore', 'pipe', 'pipe'],
    });

    viteProcess.on('error', (err) => {
      console.error('[Numkit IDE] Failed to start Vite:', err.message);
      reject(err);
    });

    let resolved = false;
    let outputBuffer = '';
    viteProcess.stdout.on('data', (data) => {
      const text = data.toString();
      process.stdout.write(text);
      outputBuffer += text;

      if (!resolved) {
        const clean = outputBuffer.replace(/\x1b\[[0-9;]*m/g, '');
        const match = clean.match(/https?:\/\/[\w.]+:\d+\/\S*/);
        if (match) {
          resolved = true;
          resolve(match[0]);
        }
      }
    });

    viteProcess.stderr.on('data', (d) => process.stderr.write(d));

    setTimeout(() => {
      if (!resolved) {
        console.error('[Numkit IDE] Vite did not start within 30s');
        resolve(null);
      }
    }, 30000);
  });
}

function waitForServer(url, timeoutMs = 15000) {
  return new Promise((resolve) => {
    const deadline = Date.now() + timeoutMs;
    const check = () => {
      if (Date.now() > deadline) { resolve(); return; }
      http.get(url, (res) => {
        res.resume();
        resolve();
      }).on('error', () => {
        setTimeout(check, 200);
      });
    };
    check();
  });
}

// ── App lifecycle ──

app.whenReady().then(async () => {
  // Inject Cross-Origin-Opener-Policy + Cross-Origin-Embedder-Policy
  // headers on every response in this Electron session. Without them
  // Chromium refuses to enable SharedArrayBuffer / Atomics.wait, and
  // the tempFS sync bridge silently falls back to direct IDB (which
  // doesn't expose the sync hooks the WASM engine needs). Setting
  // them session-wide is fine for a desktop IDE that loads only its
  // own bundled content.
  session.defaultSession.webRequest.onHeadersReceived((details, callback) => {
    callback({
      responseHeaders: {
        ...details.responseHeaders,
        'Cross-Origin-Opener-Policy':   ['same-origin'],
        'Cross-Origin-Embedder-Policy': ['require-corp'],
      },
    });
  });

  // Clear any renderer cache persisted from a previous launch so a rebuilt
  // desktop/dist is always picked up (paired with --disable-http-cache above).
  try { await session.defaultSession.clearCache(); } catch { /* best effort */ }

  let loadTarget;

  if (IS_PROD) {
    console.log('[Numkit IDE] Production mode — loading from dist/');
    loadTarget = path.join(DIST_DIR, 'index.html');
  } else {
    const viteUrl = await startVite();
    if (viteUrl) {
      await waitForServer(viteUrl);
      loadTarget = viteUrl;
    } else {
      console.error('[Numkit IDE] No Vite URL detected, exiting');
      app.quit();
      return;
    }
  }

  createWindow(loadTarget);

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow(loadTarget);
  });
});

app.on('window-all-closed', () => {
  if (viteProcess) {
    viteProcess.kill();
    viteProcess = null;
  }
  app.quit();
});

// ── Native filesystem bridge (counterpart of preload.js) ──────────
//
// All "real FS" operations the renderer makes via window.nativeFS
// land here. The renderer always passes (root, relPath); safePath()
// enforces that relPath resolves inside root so a compromised
// renderer can't escape to arbitrary disk locations.

function safePath(root, relPath) {
  const cleaned = String(relPath || '').replace(/^\/+/, '');
  const resolvedRoot = path.resolve(root);
  const full = path.resolve(resolvedRoot, cleaned);
  if (full !== resolvedRoot && !full.startsWith(resolvedRoot + path.sep)) {
    throw new Error('Path escapes mounted root: ' + relPath);
  }
  return full;
}

ipcMain.handle('fs:pickDirectory', async () => {
  const result = await dialog.showOpenDialog(mainWindow, {
    title: 'Select Folder',
    properties: ['openDirectory', 'createDirectory'],
  });
  if (result.canceled || !result.filePaths || !result.filePaths[0]) return null;
  return result.filePaths[0];
});

// Folders that explode the tree without ever being useful in the IDE
// explorer. Skipped during the recursive walk regardless of depth.
const TREE_SKIP_DIRS = new Set([
  'node_modules', '.git', '.svn', '.hg', '.idea', '.vscode',
  'build', 'build-browser', 'build-bench-wasm', 'build-portable',
  'build-desktop-fast', 'build-apple-m', 'dist', '.next', '.nuxt',
  '.cache', '.parcel-cache', '.turbo', '.venv', 'venv', '__pycache__',
  'target', '.gradle', 'cmake-build-debug', 'cmake-build-release',
]);

const TREE_MAX_ENTRIES = 8000;  // hard cap on total nodes returned
const TREE_MAX_DEPTH = 12;      // hard cap on recursion depth

ipcMain.handle('fs:listTree', async (_e, root) => {
  let total = 0;
  let truncated = false;

  async function walk(dir, rel, depth) {
    if (depth > TREE_MAX_DEPTH) { truncated = true; return []; }
    if (total >= TREE_MAX_ENTRIES) { truncated = true; return []; }
    let list;
    try { list = await fsp.readdir(dir, { withFileTypes: true }); }
    catch { return []; }
    const entries = [];
    for (const d of list) {
      if (total >= TREE_MAX_ENTRIES) { truncated = true; break; }
      // Skip obvious junk that shouldn't clutter the tree.
      if (d.name.startsWith('.DS_Store')) continue;
      // Skip well-known noise dirs: every numkit worktree has these
      // at the root and walking them was the proven cause of Electron
      // renderer OOM (the serialised tree exceeded V8's heap when a
      // user mounted a folder with a populated node_modules / build-*).
      if (d.isDirectory() && TREE_SKIP_DIRS.has(d.name)) continue;
      const full = path.join(dir, d.name);
      const itemPath = rel === '/' ? `/${d.name}` : `${rel}/${d.name}`;
      const node = {
        name: d.name,
        path: itemPath,
        type: d.isDirectory() ? 'folder' : 'file',
      };
      total++;
      if (d.isDirectory()) {
        try { node.children = await walk(full, itemPath, depth + 1); }
        catch { node.children = []; }
      }
      entries.push(node);
    }
    entries.sort((a, b) => {
      if (a.type !== b.type) return a.type === 'folder' ? -1 : 1;
      return a.name.localeCompare(b.name);
    });
    return entries;
  }

  const tree = await walk(path.resolve(root), '/', 0);
  if (truncated) {
    console.warn(`[Numkit IDE] fs:listTree truncated: ${total} entries reached cap`
               + ` (max ${TREE_MAX_ENTRIES}, depth ${TREE_MAX_DEPTH}). Some nested folders may be hidden.`);
  }
  return tree;
});

ipcMain.handle('fs:readFile', async (_e, root, relPath) => {
  const full = safePath(root, relPath);
  try { return await fsp.readFile(full, 'utf8'); }
  catch (err) {
    if (err.code === 'ENOENT' || err.code === 'EISDIR') return null;
    throw err;
  }
});

// ── Synchronous read for the WASM engine ────────────────────────
// `ipcMain.on(...)` paired with `event.returnValue =` is the only
// way to satisfy `ipcRenderer.sendSync()` from the renderer, which
// is what the vfs-adapter's sync readFile callback needs (csvread /
// load are called from C++ inside a single Emscripten frame and
// can't await). Cap reads at 16 MB so a stray sync-read of a huge
// binary doesn't freeze the UI for tens of seconds. Using fs.readFileSync
// (sync stdlib call) keeps the round-trip cost to one IPC hop +
// one disk read; for a typical .m / .csv file that's 1-5 ms.
const SYNC_READ_LIMIT_BYTES = 16 * 1024 * 1024;
ipcMain.on('fs:readFileSync', (event, root, relPath) => {
  try {
    const full = safePath(root, relPath);
    const st = fs.statSync(full);
    if (st.size > SYNC_READ_LIMIT_BYTES) {
      event.returnValue = { error: `file too large for sync read: ${st.size} bytes` };
      return;
    }
    event.returnValue = { content: fs.readFileSync(full, 'utf8') };
  } catch (err) {
    if (err.code === 'ENOENT' || err.code === 'EISDIR') {
      event.returnValue = { content: null };
      return;
    }
    event.returnValue = { error: err.message || String(err) };
  }
});
ipcMain.on('fs:existsSync', (event, root, relPath) => {
  try {
    const full = safePath(root, relPath);
    fs.accessSync(full);
    event.returnValue = true;
  } catch { event.returnValue = false; }
});

// ── Synchronous BINARY read for the WASM engine ─────────────────
// Mirror of fs:readFileSync but returns raw bytes (no 'utf8' encoding),
// for imread / audioread and friends. The renderer hands the bytes to
// the WASM CallbackFS binary read hook untouched. Same 16 MB cap.
ipcMain.on('fs:readFileBinarySync', (event, root, relPath) => {
  try {
    const full = safePath(root, relPath);
    const st = fs.statSync(full);
    if (st.size > SYNC_READ_LIMIT_BYTES) {
      event.returnValue = { error: `file too large for sync read: ${st.size} bytes` };
      return;
    }
    const buf = fs.readFileSync(full); // Buffer (no encoding → raw bytes)
    const bytes = new Uint8Array(buf.length);
    bytes.set(buf);
    event.returnValue = { bytes };
  } catch (err) {
    if (err.code === 'ENOENT' || err.code === 'EISDIR') {
      event.returnValue = { bytes: null };
      return;
    }
    event.returnValue = { error: err.message || String(err) };
  }
});

// Binary write (imwrite / audiowrite). Accepts a Uint8Array; writes raw
// bytes with no String() coercion (which would corrupt non-UTF-8 data).
ipcMain.handle('fs:writeFileBinary', async (_e, root, relPath, bytes) => {
  const full = safePath(root, relPath);
  await fsp.mkdir(path.dirname(full), { recursive: true });
  const buf = bytes == null
    ? Buffer.alloc(0)
    : Buffer.from(bytes.buffer ? bytes.buffer : bytes,
                  bytes.byteOffset || 0,
                  bytes.byteLength != null ? bytes.byteLength : undefined);
  await fsp.writeFile(full, buf);
});

ipcMain.handle('fs:writeFile', async (_e, root, relPath, content) => {
  const full = safePath(root, relPath);
  await fsp.mkdir(path.dirname(full), { recursive: true });
  await fsp.writeFile(full, content == null ? '' : String(content), 'utf8');
});

ipcMain.handle('fs:mkdir', async (_e, root, relPath) => {
  const full = safePath(root, relPath);
  await fsp.mkdir(full, { recursive: true });
});

ipcMain.handle('fs:remove', async (_e, root, relPath) => {
  const full = safePath(root, relPath);
  if (full === path.resolve(root)) throw new Error('Refusing to remove mount root');
  await fsp.rm(full, { recursive: true, force: true });
});

ipcMain.handle('fs:rename', async (_e, root, oldRel, newRel) => {
  const srcFull = safePath(root, oldRel);
  const dstFull = safePath(root, newRel);
  await fsp.mkdir(path.dirname(dstFull), { recursive: true });
  await fsp.rename(srcFull, dstFull);
});

ipcMain.handle('fs:exists', async (_e, root, relPath) => {
  try { await fsp.access(safePath(root, relPath)); return true; }
  catch (_) { return false; }
});

ipcMain.handle('shell:reveal', async (_e, root, relPath) => {
  // Empty relPath → reveal the mount root itself.
  const full = relPath ? safePath(root, relPath) : path.resolve(root);
  const errMsg = await shell.openPath(full);
  if (errMsg) throw new Error(errMsg);
});

ipcMain.handle('shell:showItem', async (_e, root, relPath) => {
  const full = safePath(root, relPath);
  shell.showItemInFolder(full);
});
