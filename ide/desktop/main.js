// main.js — Numkit IDE desktop shell
// Supports two modes:
//   Dev mode:  spawns Vite dev server, loads from http://
//   Prod mode: loads pre-built static files from dist/
const { app, BrowserWindow, dialog, shell, ipcMain, session } = require('electron');
const { spawn } = require('child_process');
const path = require('path');
const os = require('os');
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
// across launches, so after a rebuild `desktop-run` could reload the OLD code
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

// Single-instance guard. A second `desktop-run` should focus + reload the
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
      // console.log('[mem-trace]', lines.join(' | '));
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

  mainWindow.on('closed', () => { replSession.kill(); mainWindow = null; });
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
      windowsHide: true,
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
  const resolvedRoot = path.resolve(root || '.');
  const rootWithSep = resolvedRoot.endsWith(path.sep) ? resolvedRoot : resolvedRoot + path.sep;
  const full = path.resolve(resolvedRoot, cleaned);
  if (full !== resolvedRoot && !full.startsWith(rootWithSep)) {
    throw new Error('Path escapes mounted root: ' + relPath);
  }
  return full;
}

const TEMP_ROOT = path.join(app.getPath('userData'), 'temporary');
try { fs.mkdirSync(TEMP_ROOT, { recursive: true }); } catch { /* ignore */ }

ipcMain.handle('fs:getTempRoot', async () => {
  return TEMP_ROOT;
});

ipcMain.handle('fs:getUserHome', async () => {
  return app.getPath('home') || os.homedir() || '';
});

ipcMain.handle('fs:pickDirectory', async () => {
  const result = await dialog.showOpenDialog(mainWindow, {
    title: 'Select Folder',
    properties: ['openDirectory', 'createDirectory'],
  });
  if (result.canceled || !result.filePaths || !result.filePaths[0]) return null;
  return result.filePaths[0];
});

ipcMain.handle('fs:listDir', async (_e, root, relPath = '/') => {
  const full = safePath(root, relPath);
  let list;
  try { list = await fsp.readdir(full, { withFileTypes: true }); }
  catch { return []; }
  const entries = [];
  for (const d of list) {
    if (d.name.startsWith('.DS_Store')) continue;
    const cleanRel = (relPath || '/').replace(/\\/g, '/').replace(/\/+$/, '');
    const itemPath = cleanRel === '' || cleanRel === '/' ? `/${d.name}` : `${cleanRel}/${d.name}`;
    entries.push({
      name: d.name,
      path: itemPath,
      type: d.isDirectory() ? 'folder' : 'file',
    });
  }
  entries.sort((a, b) => {
    if (a.type !== b.type) return a.type === 'folder' ? -1 : 1;
    return a.name.localeCompare(b.name);
  });
  return entries;
});

// Flat list for any legacy listTree callers
ipcMain.handle('fs:listTree', async (_e, root) => {
  const full = safePath(root, '/');
  let list;
  try { list = await fsp.readdir(full, { withFileTypes: true }); }
  catch { return []; }
  const entries = [];
  for (const d of list) {
    if (d.name.startsWith('.DS_Store')) continue;
    entries.push({
      name: d.name,
      path: `/${d.name}`,
      type: d.isDirectory() ? 'folder' : 'file',
    });
  }
  entries.sort((a, b) => {
    if (a.type !== b.type) return a.type === 'folder' ? -1 : 1;
    return a.name.localeCompare(b.name);
  });
  return entries;
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

// ── External tools ──────────────────────────────────────────────────
//
// runtimeSettings holds the tool paths pushed from the renderer via
// settings:update. Main process reads them at spawn time so the user
// can change paths without restarting the IDE.
let runtimeSettings = { interpreterPath: '', codegenPath: '', cxxPath: '' };

// Resolve the absolute path to an external tool executable.
//
// Priority order:
//   1. Explicit path from runtimeSettings (user configured in Preferences).
//   2. The same directory as the running IDE executable — this is where
//      desktop-build.bat places numkit_repl.exe / numkit_codegen.exe in the
//      deploy/ bundle, so an out-of-the-box install Just Works without
//      any Preferences configuration.
//   3. Bare name (OS PATH lookup at spawn time).
//
// Returns the resolved path string. Existence is NOT guaranteed for case 3
// (PATH lookup happens lazily at spawn); for cases 1 and 2 the file is
// checked with fs.existsSync before returning.
function resolveExe(settingPath, exeName) {
  const exeOnWin = process.platform === 'win32' ? `${exeName}.exe` : exeName;
  const explicit = (settingPath || '').trim();

  // 1. If explicit setting is an absolute path or existing file on disk, use it.
  if (explicit && (path.isAbsolute(explicit) || fs.existsSync(explicit))) {
    // console.log(`[resolveExe] ${exeName}: using existing explicit path → ${explicit}`);
    return explicit;
  }

  // 2. Portable executable directory (electron-builder sets PORTABLE_EXECUTABLE_DIR to original exe dir).
  if (process.env.PORTABLE_EXECUTABLE_DIR) {
    const candidate = path.join(process.env.PORTABLE_EXECUTABLE_DIR, exeOnWin);
    // console.log(`[resolveExe] ${exeName}: checking PORTABLE_EXECUTABLE_DIR → ${candidate}`);
    if (fs.existsSync(candidate)) {
      // console.log(`[resolveExe] ${exeName}: found in PORTABLE_EXECUTABLE_DIR → ${candidate}`);
      return candidate;
    }
  }

  // 3. Same directory as the packaged IDE executable (non-portable / unpacked mode).
  try {
    const ideDir = path.dirname(app.getPath('exe'));
    const candidate = path.join(ideDir, exeOnWin);
    // console.log(`[resolveExe] ${exeName}: checking next to IDE exe → ${candidate}`);
    if (fs.existsSync(candidate)) {
      // console.log(`[resolveExe] ${exeName}: found next to IDE exe → ${candidate}`);
      return candidate;
    }
  } catch (e) { /* console.log(`[resolveExe] app.getPath failed: ${e.message}`); */ }

  // 4. Search build, deploy, repo directories (picking the newest binary by mtime)
  try {
    const repoRoot = path.resolve(__dirname, '..', '..');
    const devCandidates = [
      path.join(repoRoot, 'build', 'desktop-fast', 'apps', 'numkit',  'Release', exeOnWin),
      path.join(repoRoot, 'build', 'desktop-fast', 'apps', 'numkit',  'Debug', exeOnWin),
      path.join(repoRoot, 'build', 'desktop-fast', 'apps', exeName,   'Release', exeOnWin),
      path.join(repoRoot, 'build', 'desktop-fast', 'apps', exeName,   'Debug', exeOnWin),
      path.join(repoRoot, 'build', 'desktop-fast', 'Release', exeOnWin),
      path.join(repoRoot, 'build', 'desktop-fast', 'Debug', exeOnWin),
      path.join(repoRoot, 'deploy', 'desktop', exeOnWin),
      path.join(process.cwd(), 'deploy', 'desktop', exeOnWin),
      path.join(process.cwd(), exeOnWin),
    ];
    let newest = null;
    let newestMtime = 0;
    for (const c of devCandidates) {
      if (fs.existsSync(c)) {
        try {
          const mtime = fs.statSync(c).mtimeMs;
          if (mtime > newestMtime) {
            newestMtime = mtime;
            newest = c;
          }
        } catch (e) {}
      }
    }
    if (newest) return newest;
  } catch (e) { /* console.log(`[resolveExe] dev search failed: ${e.message}`); */ }

  // 5. Fallback to explicit if provided, otherwise bare name
  return explicit || exeOnWin;
}

function autoDetectToolPaths(settings) {
  const current = { ...(settings || {}) };
  let modified = false;

  try {
    const replResolved = resolveExe(current.interpreterPath, 'numkit_repl');
    if (replResolved && (replResolved.includes('/') || replResolved.includes('\\')) && current.interpreterPath !== replResolved) {
      current.interpreterPath = replResolved;
      modified = true;
    }

    const codegenResolved = resolveExe(current.codegenPath, 'numkit_codegen');
    if (codegenResolved && (codegenResolved.includes('/') || codegenResolved.includes('\\')) && current.codegenPath !== codegenResolved) {
      current.codegenPath = codegenResolved;
      modified = true;
    }
  } catch (e) {
    console.warn('[main.js] autoDetectToolPaths failed:', e.message);
  }

  if (modified) {
    runtimeSettings = { ...runtimeSettings, ...current };
    console.log('[main.js] Auto-detected tool paths:', runtimeSettings);
  }

  return current;
}

// Open a native file-picker dialog.
// Returns the selected absolute path or null on cancel.
ipcMain.handle('fs:pickFile', async (_e, opts) => {
  const result = await dialog.showOpenDialog(mainWindow, {
    title: opts?.title || 'Select executable',
    properties: ['openFile'],
    filters: process.platform === 'win32'
      ? [
          { name: 'Executable', extensions: ['exe', 'cmd', 'bat'] },
          { name: 'All Files',  extensions: ['*'] },
        ]
      : [{ name: 'All Files', extensions: ['*'] }],
  });
  return result.canceled ? null : result.filePaths[0];
});

// Receive updated tool paths from the renderer and store them in
// memory so subsequent codegen:run / interpreter spawns pick them up.
ipcMain.handle('settings:update', async (_e, settings) => {
  if (settings && typeof settings === 'object') {
    const oldCompat = runtimeSettings.matlabCompatibility;
    const oldPath = runtimeSettings.interpreterPath;
    runtimeSettings = { ...runtimeSettings, ...settings };
    autoDetectToolPaths(runtimeSettings);
    console.log('[Numkit IDE] settings updated:', runtimeSettings);
    if (typeof replSession !== 'undefined' && (oldCompat !== runtimeSettings.matlabCompatibility || oldPath !== runtimeSettings.interpreterPath)) {
      replSession._kill();
    }
  }
});

// Return the resolved exe paths so the renderer can display them in the
// Preferences modal even when the setting fields are left empty.
ipcMain.handle('settings:resolve', async () => {
  autoDetectToolPaths(runtimeSettings);
  return {
    interpreterPath: resolveExe(runtimeSettings.interpreterPath, 'numkit_repl'),
    codegenPath:     resolveExe(runtimeSettings.codegenPath,     'numkit_codegen'),
    cxxPath: (runtimeSettings.cxxPath || '').trim(),
  };
});

// ── Persistent REPL session ────────────────────────────────────────────────
//
// Manages a long-lived `numkit_repl --ide-session` child process.
// The IDE routes ALL code execution (ConsolePane REPL lines + Run button)
// through this session so the workspace is shared and persistent between runs.
//
// Protocol:
//   main.js → stdin:   <code>\n__END_OF_INPUT__\n   (or __RESET__ / __QUIT__)
//   stdin   → stdout:  <output>\n__VARS__:{json}\n__END_OF_RUN__\n
//
// On process crash: sets crashed=true; next run() call transparently
// restarts the process and returns { sessionRestarted: true } in the result.
// ── Figure-marker extraction helper ──────────────────────────────────────────────
// Strip __FIGURE_DATA__:, __FIGURE_CLOSE__:, __FIGURE_CLOSE_ALL__ markers from
// a raw output string.  Returns { cleanOutput, figures, closedFigureIds,
// closeAllFigures, errorLine }.  Kept as a plain CJS function (no ESM import)
// so main.js has no build-time dependency on repl-protocol.js.
function _extractFigureMarkers(output) {
  const FIGURE_MARKER     = '__FIGURE_DATA__:';
  const CLOSE_MARKER      = '__FIGURE_CLOSE__:';
  const CLOSE_ALL_MARKER  = '__FIGURE_CLOSE_ALL__';
  const ERROR_LINE_MARKER = '__ERROR_LINE__:';
  const rawLines        = (output || '').split('\n');
  const cleanLines      = [];
  const figures         = [];
  const closedFigureIds = [];
  let closeAllFigures   = false;
  let errorLine         = null;

  for (const line of rawLines) {
    const errIdx = line.indexOf(ERROR_LINE_MARKER);
    if (errIdx !== -1) {
      const num = parseInt(line.substring(errIdx + ERROR_LINE_MARKER.length).trim(), 10);
      if (!isNaN(num) && num > 0) errorLine = num;
      continue;
    }
    if (line.trim() === CLOSE_ALL_MARKER) { closeAllFigures = true; continue; }
    const closeIdx = line.indexOf(CLOSE_MARKER);
    if (closeIdx !== -1) {
      const id = parseInt(line.substring(closeIdx + CLOSE_MARKER.length).trim(), 10);
      if (!isNaN(id)) closedFigureIds.push(id);
      continue;
    }
    const figIdx = line.indexOf(FIGURE_MARKER);
    if (figIdx !== -1) {
      const before = line.substring(0, figIdx).trimEnd();
      if (before) cleanLines.push(before);
      const payload = line.substring(figIdx + FIGURE_MARKER.length).trim();
      if (payload.startsWith('{')) {
        let depth = 0, end = 0;
        for (let i = 0; i < payload.length; i++) {
          if (payload[i] === '{') depth++;
          else if (payload[i] === '}') { depth--; if (depth === 0) { end = i + 1; break; } }
        }
        if (end > 0) {
          try { figures.push(JSON.parse(payload.substring(0, end))); }
          catch (e) { console.warn('[ReplSession] bad figure JSON:', e.message); }
        }
      }
      continue;
    }
    cleanLines.push(line);
  }
  while (cleanLines.length && cleanLines[cleanLines.length - 1] === '') cleanLines.pop();
  return { cleanOutput: cleanLines.join('\n'), figures, closedFigureIds, closeAllFigures, errorLine };
}

class ReplSession {
  constructor() {
    this.proc      = null;    // child_process.ChildProcess
    this.exePath   = null;    // path used to spawn (for restart comparison)
    this.crashed   = false;   // set true when proc dies unexpectedly
    this.pending   = null;    // active request item: { exePath, payload, resolve, wasRestarted, stdout, stderr }
    this._queue    = [];      // queued request items waiting to run sequentially
    this._buf         = '';      // stdout accumulation buffer
    this._breakpoints = [];      // breakpoint lines stored for next debugStart()
  }

  // Ensure the process is running with the given exe path.
  // Spawns lazily; kills+respawns if the exe path changed.
  _spawn(exePath) {
    if (this.proc && this.exePath === exePath) return; // already up
    this._kill();
    this.exePath = exePath;
    this.crashed = false;
    this._buf    = '';

    let child;
    try {
      const args = ['--ide-session'];
      if (runtimeSettings.matlabCompatibility) args.push('--compat');
      child = spawn(exePath, args, {
        stdio: ['pipe', 'pipe', 'pipe'],
        windowsHide: true,
      });
    } catch (err) {
      throw Object.assign(new Error(err.message), { notFound: err.code === 'ENOENT' });
    }

    child.stdout.on('data', (data) => {
      this._buf += data.toString();
      this._flush();
    });

    child.stderr.on('data', (data) => {
      if (this.pending) this.pending.stderr += data.toString();
    });

    child.on('error', (err) => {
      this._drainQueue(err);
      this.proc = null;
      this.crashed = err.code !== 'ENOENT'; // ENOENT = config error, not a crash
    });

    child.on('close', (code, signal) => {
      this._drainQueue(null, code);
      if (this.proc === child) {
        this.proc    = null;
        this.crashed = signal !== null || (code !== 0 && code !== null);
      }
    });

    this.proc = child;
  }

  _drainQueue(err, code) {
    if (this.pending) {
      const { resolve, stdout, stderr } = this.pending;
      this.pending = null;
      resolve({ stdout: stdout || '', stderr: (stderr || '') + (err ? err.message : ''), exitCode: code ?? -1, notFound: err?.code === 'ENOENT' });
    }
    while (this._queue.length > 0) {
      const item = this._queue.shift();
      item.resolve({ stdout: '', stderr: 'REPL session closed', exitCode: code ?? -1, notFound: err?.code === 'ENOENT' });
    }
  }

  _enqueue(exePath, payload, resolve, wasRestarted = false, kind = 'run') {
    this._queue.push({ exePath, payload, resolve, wasRestarted, kind, stdout: '', stderr: '' });
    this._processQueue();
  }

  _processQueue() {
    if (this.pending || this._queue.length === 0) return;

    const next = this._queue[0];
    try {
      this._spawn(next.exePath);
    } catch (err) {
      const item = this._queue.shift();
      item.resolve({ stdout: '', stderr: err.message, vars: null, exitCode: -1, notFound: err.notFound, error: err.message });
      this._processQueue();
      return;
    }

    this.pending = this._queue.shift();
    if (this.pending.wasRestarted) {
      const orig = this.pending.resolve;
      this.pending.resolve = (r) => orig({ ...r, sessionRestarted: true });
    }

    try {
      this.proc.stdin.write(this.pending.payload, 'utf8');
    } catch (err) {
      const p = this.pending;
      this.pending = null;
      p.resolve({ stdout: '', stderr: err.message, vars: null, exitCode: -1, error: err.message });
      this._processQueue();
    }
  }

  // Drain _buf: resolve pending when __END_OF_RUN__ or __END_OF_STEP__ is seen.
  _flush() {
    this._buf = this._buf.replace(/\r\n/g, '\n');

    const END_RUN  = '__END_OF_RUN__\n';
    const END_STEP = '__END_OF_STEP__\n';
    const runIdx  = this._buf.indexOf(END_RUN);
    const stepIdx = this._buf.indexOf(END_STEP);

    let idx, markerLen, isStep;
    if (stepIdx !== -1 && (runIdx === -1 || stepIdx < runIdx)) {
      idx = stepIdx; markerLen = END_STEP.length; isStep = true;
    } else if (runIdx !== -1) {
      idx = runIdx;  markerLen = END_RUN.length;  isStep = false;
    } else {
      return;
    }

    const chunk  = this._buf.slice(0, idx);
    this._buf    = this._buf.slice(idx + markerLen);
    // console.log('[ReplSession] chunk (%s):', isStep ? 'STEP' : 'RUN',
    //             JSON.stringify(chunk.slice(0, 400)));

    if (!this.pending) {
      this._processQueue();
      return;
    }
    const { resolve, stderr, kind } = this.pending;
    this.pending = null;
    const lines = chunk.split('\n');

    // Helper to complete the current item and continue to next in queue
    const finish = (result) => {
      resolve(result);
      this._processQueue();
    };

    // ── Debug step: paused at breakpoint ────────────────────────────────
    if (isStep) {
      const bpLine = lines.find(l => l.startsWith('__BREAKPOINT__:'));
      if (bpLine) {
        try {
          const parsed = JSON.parse(bpLine.slice('__BREAKPOINT__:'.length));
          const rawOut = parsed.output || '';
          const { cleanOutput, figures, closedFigureIds, closeAllFigures }
            = _extractFigureMarkers(rawOut);
          finish({
            status:          'paused',
            pauseState:      parsed.pauseState || null,
            output:          cleanOutput,
            figures,
            closedFigureIds,
            closeAllFigures,
          });
        } catch (e) {
          finish({ status: 'error', message: 'Failed to parse BREAKPOINT: ' + e.message });
        }
      } else {
        finish({ status: 'error', message: 'No __BREAKPOINT__: line in debug step chunk' });
      }
      return;
    }

    // ── Debug stopped ────────────────────────────────────────────────
    if (lines.find(l => l === '__DEBUG_STOPPED__')) {
      finish({ status: 'stopped' });
      return;
    }

    // ── Debug completion (__DEBUG_END__) ───────────────────────────────
    if (lines.find(l => l === '__DEBUG_END__')) {
      let result = { status: 'completed' };
      const drLine = lines.find(l => l.startsWith('__DEBUG_RESULT__:'));
      if (drLine) {
        try { result = JSON.parse(drLine.slice('__DEBUG_RESULT__:'.length)); } catch { /* keep default */ }
      }
      let vars = null;
      const vl = lines.find(l => l.startsWith('__VARS__:'));
      if (vl) try { vars = JSON.parse(vl.slice(9)); } catch { /* ignore */ }

      // collect output lines before __DEBUG_END__
      const rawOutputLines = [];
      for (const l of lines) {
        if (l === '__DEBUG_END__') break;
        if (l.startsWith('__VARS__:') || l.startsWith('__DEBUG_RESULT__:')) continue;
        rawOutputLines.push(l);
      }
      while (rawOutputLines.length && rawOutputLines[rawOutputLines.length - 1] === '')
        rawOutputLines.pop();
      const { cleanOutput, figures, closedFigureIds, closeAllFigures }
        = _extractFigureMarkers(rawOutputLines.join('\n'));

      finish({
        status:          result.status || 'completed',
        message:         result.message,
        line:            result.line,
        output:          cleanOutput,
        figures,
        closedFigureIds,
        closeAllFigures,
        vars,
      });
      return;
    }

    // ── Introspection / command responses (only for query or reset requests) ──
    if (kind === 'query' || kind === 'reset') {
      const INTROSPECT_MARKERS = [
        '__VAR_DATA__:', '__SHAPE_DATA__:', '__PAGE_DATA__:',
        '__STATS_DATA__:', '__TILE_DATA__:', '__PATH_DATA__:',
        '__AST_DATA__:', '__GRAPH_DATA__:', '__FIGURE_DATA__:',
      ];
      for (const m of INTROSPECT_MARKERS) {
        const found = lines.find(l => l.startsWith(m));
        if (found) {
          try { finish(JSON.parse(found.slice(m.length))); }
          catch (e) { finish({ error: 'JSON parse error: ' + e.message, raw: found.slice(0, 200) }); }
          return;
        }
      }
      if (lines.find(l => l === '__RESET_OK__')) {
        let vars = null;
        const vl = lines.find(l => l.startsWith('__VARS__:'));
        if (vl) try { vars = JSON.parse(vl.slice(9)); } catch { /* ignore */ }
        finish({ ok: true, vars });
        return;
      }
      const errLine = lines.find(l => l.startsWith('__CMD_ERROR__:'));
      if (errLine) { finish({ error: errLine.slice(14) }); return; }
    }

    // ── Regular run response (with figure extraction) ──────────────────────
    let vars = null;
    const rawOutputLines = [];
    for (const line of lines) {
      const trimmed = line.trimEnd();
      if (trimmed.startsWith('__VARS__:')) {
        try {
          vars = JSON.parse(trimmed.slice(9));
          // console.log('[ReplSession] vars keys:', Object.keys(vars));
        } catch (e) {
          console.warn('[ReplSession] failed to parse vars:', trimmed.slice(0, 200), e.message);
        }
      } else {
        rawOutputLines.push(line);
      }
    }
    while (rawOutputLines.length && rawOutputLines[rawOutputLines.length - 1] === '')
      rawOutputLines.pop();

    const { cleanOutput, figures, closedFigureIds, closeAllFigures, errorLine }
      = _extractFigureMarkers(rawOutputLines.join('\n'));

    finish({
      stdout:          cleanOutput,
      stderr,
      vars,
      figures,
      closedFigureIds,
      closeAll:        closeAllFigures,
      errorLine:       errorLine ?? null,
      exitCode:        0,
    });
  }

  // Send code to the session; return Promise<{stdout,stderr,vars,exitCode,notFound?,sessionRestarted?}>.
  async run(code, opts = null) {
    const exePath = resolveExe(runtimeSettings.interpreterPath, 'numkit_repl');
    const wasRestarted = this.crashed;
    let payload = '';
    if (opts?.cwd) {
      const normalizedCwd = opts.cwd.replace(/\\/g, '/');
      payload += `cd('${normalizedCwd}')\n`;
    }
    payload += (typeof code === 'string' ? code : '') + '\n__END_OF_INPUT__\n';

    return new Promise((resolve) => {
      this._enqueue(exePath, payload, resolve, wasRestarted, 'run');
    });
  }

  // Set the current working directory in the session.
  async setCwd(newCwd) {
    const exePath = resolveExe(runtimeSettings.interpreterPath, 'numkit_repl');
    const normalizedCwd = (newCwd || '').replace(/\\/g, '/');
    return new Promise((resolve) => {
      this._enqueue(exePath, `__SET_CWD__:${normalizedCwd}\n`, resolve, false, 'query');
    });
  }

  // Send __RESET__ to clear the workspace (no code executed).
  async reset() {
    const exePath = resolveExe(runtimeSettings.interpreterPath, 'numkit_repl');
    return new Promise((resolve) => {
      this._enqueue(exePath, '__RESET__\n', resolve, false, 'reset');
    });
  }

  // Send a single-line introspection command; resolve with the parsed JSON response.
  // The process is spawned on demand (same as run()) and stays alive.
  async query(command) {
    const exePath = resolveExe(runtimeSettings.interpreterPath, 'numkit_repl');
    return new Promise((resolve) => {
      this._enqueue(exePath, command + '\n', resolve, false, 'query');
    });
  }

  // ── Introspection convenience methods ───────────────────────────────────
  // All send a single-line command and await the __*_DATA__:json response.
  getVarShape(name)                              { return this.query(`__GET_SHAPE__:${name}`); }
  getVarData(name, page = 0)                     { return this.query(`__INSPECT__:${name}`); }
  getVarPage(name, page)                         { return this.query(`__GET_PAGE__:${name}\t${page}`); }
  getVarTile(name, r0, c0, rows, cols, page = 0) { return this.query(`__GET_TILE__:${name}\t${r0}\t${c0}\t${rows}\t${cols}\t${page}`); }
  getVarStats(name, page = -1)                   { return this.query(`__GET_STATS__:${name}\t${page}`); }
  getVarFigure(name, opts = {})                  { return this.query(`__GET_FIGURE__:${name}\t${JSON.stringify(opts)}`); }
  inspectPath(name, path)                        { return this.query(`__INSPECT_PATH__:${name}\t${path || ''}`); }

  async buildAST(source) {
    const exePath = resolveExe(runtimeSettings.interpreterPath, 'numkit_repl');
    const payload = `__BUILD_AST__\n${source || ''}\n__END_OF_INPUT__\n`;
    return new Promise((resolve) => {
      this._enqueue(exePath, payload, resolve, false, 'query');
    });
  }

  async buildScriptGraph(source) {
    const exePath = resolveExe(runtimeSettings.interpreterPath, 'numkit_repl');
    const payload = `__BUILD_GRAPH__\n${source || ''}\n__END_OF_INPUT__\n`;
    return new Promise((resolve) => {
      this._enqueue(exePath, payload, resolve, false, 'query');
    });
  }

  // ── Debug methods ─────────────────────────────────────────────────────────
  // Breakpoints are stored here; they are sent with the next debugStart() call.
  debugSetBreakpoints(lines) {
    this._breakpoints = Array.isArray(lines) ? lines : [];
  }

  // Start a debug session: send __DEBUG_START__:<bpJson>\n<code>\n__END_OF_INPUT__
  // Returns Promise<{ status:'paused'|'completed'|'error'|'stopped', ... }>
  async debugStart(code) {
    const exePath = resolveExe(runtimeSettings.interpreterPath, 'numkit_repl');
    const bpJson = JSON.stringify(this._breakpoints);
    const payload = `__DEBUG_START__:${bpJson}\n${code}\n__END_OF_INPUT__\n`;
    return new Promise((resolve) => {
      this._enqueue(exePath, payload, resolve, false, 'debug');
    });
  }

  // Send a debug command while paused: action = 'continue'|'step_over'|'step_into'|'step_out'|'stop'
  // Returns Promise<{ status:'paused'|'completed'|'error'|'stopped', ... }>
  async debugStep(action) {
    const exePath = this.exePath || resolveExe(runtimeSettings.interpreterPath, 'numkit_repl');
    return new Promise((resolve) => {
      this._enqueue(exePath, `__DEBUG_CMD__:${action}\n`, resolve, false, 'debug');
    });
  }

  // Kill the process (cleanup on IDE close or explicit reset from UI).
  _kill() {
    if (this.proc) {
      try { this.proc.stdin.write('__QUIT__\n', 'utf8'); } catch { /* ignore */ }
      try { this.proc.kill(); } catch { /* ignore */ }
      this.proc    = null;
      this.crashed = false;
      this._buf    = '';
      this._drainQueue(null);
    }
  }

  kill() { this._kill(); }
}


const replSession = new ReplSession();

// IPC: execute code in the persistent REPL session.
// Returns { stdout, stderr, vars, exitCode, notFound?, sessionRestarted? }
ipcMain.handle('repl:run', async (_e, code, opts) => {
  return replSession.run(typeof code === 'string' ? code : '', opts);
});

ipcMain.handle('repl:setCwd', async (_e, newCwd) => {
  return replSession.setCwd(newCwd);
});

// IPC: reset the REPL workspace (clear all).
ipcMain.handle('repl:reset', async () => {
  return replSession.reset();
});

// IPC: kill the REPL process entirely (user-triggered full reset).
ipcMain.handle('repl:kill', async () => {
  replSession.kill();
});

// IPC: var introspection — all route through the same native REPL process.
ipcMain.handle('repl:getVarShape',   (_e, name)                        => replSession.getVarShape(name));
ipcMain.handle('repl:getVarData',    (_e, name)                        => replSession.getVarData(name));
ipcMain.handle('repl:getVarPage',    (_e, name, page)                  => replSession.getVarPage(name, page));
ipcMain.handle('repl:getVarTile',    (_e, name, r0, c0, rows, cols, page) => replSession.getVarTile(name, r0, c0, rows, cols, page));
ipcMain.handle('repl:getVarStats',   (_e, name, page)                  => replSession.getVarStats(name, page));
ipcMain.handle('repl:getVarFigure',  (_e, name, opts)                  => replSession.getVarFigure(name, opts));
ipcMain.handle('repl:inspectPath',   (_e, name, path)                  => replSession.inspectPath(name, path));
ipcMain.handle('repl:buildAST',         (_e, source)                      => replSession.buildAST(source));
ipcMain.handle('repl:buildScriptGraph', (_e, source)                      => replSession.buildScriptGraph(source));

// Transpile + AOT-compile + run the given numkit source code.
//
// The renderer passes the script source as a string; we write it to a
// temp .m file, spawn numkit_codegen --run against it, collect both
// stdout and stderr, clean up the temp file, then resolve with:
//   { stdout, stderr, exitCode }       on normal exit
//   { stdout, stderr, exitCode, notFound: true }  when the exe is missing
ipcMain.handle('codegen:run', async (_e, code, opts) => {
  // 1. Resolve executable via priority chain (settings → next to IDE → PATH).
  const exe = resolveExe(runtimeSettings.codegenPath, 'numkit_codegen');

  // 2. Write source to a temp file.
  const tmpDir = path.join(os.tmpdir(), 'numkit_codegen_run');
  try { fs.mkdirSync(tmpDir, { recursive: true }); } catch { /* exists */ }
  const tmpFile = path.join(tmpDir, `run_${Date.now()}.m`);
  try { fs.writeFileSync(tmpFile, typeof code === 'string' ? code : '', 'utf8'); }
  catch (err) { return { stdout: '', stderr: `Failed to write temp file: ${err.message}`, exitCode: -1 }; }

  // 3. Build argument list: always --run; optional --entry / --args.
  const args = [tmpFile, '--run'];
  if (opts?.entry) args.push('--entry', opts.entry);
  if (opts?.args)  args.push('--args', opts.args);

  // 4. Env: pass NUMKIT_CXX override when configured.
  const env = { ...process.env };
  if ((runtimeSettings.cxxPath || '').trim()) env.NUMKIT_CXX = runtimeSettings.cxxPath.trim();

  // 5. Spawn, collect, clean up, resolve.
  return new Promise((resolve) => {
    let stdout = '', stderr = '';
    let child;
    try {
      child = spawn(exe, args, { env, windowsHide: true });
    } catch (err) {
      fs.unlink(tmpFile, () => {});
      resolve({ stdout: '', stderr: err.message, exitCode: -1, notFound: true });
      return;
    }
    child.stdout.on('data', (d) => { stdout += d.toString(); });
    child.stderr.on('data', (d) => { stderr += d.toString(); });
    child.on('close', (code) => {
      fs.unlink(tmpFile, () => {});
      resolve({ stdout, stderr, exitCode: code ?? -1 });
    });
    child.on('error', (err) => {
      fs.unlink(tmpFile, () => {});
      const notFound = err.code === 'ENOENT';
      resolve({ stdout, stderr: err.message, exitCode: -1, notFound });
    });
  });
});

ipcMain.handle('shell:showItem', async (_e, root, relPath) => {
  const full = safePath(root, relPath);
  shell.showItemInFolder(full);
});

// IPC: debugger — all route through the same native REPL process.
// debugSetBreakpoints stores lines for the next debugStart; returns immediately.
// debugStart / debugStep return Promise<debugResult> (paused|completed|error|stopped).
// debugStop is a convenience alias for debugStep('stop').
ipcMain.handle('repl:debugSetBreakpoints', (_e, lines)  => { replSession.debugSetBreakpoints(lines); });
ipcMain.handle('repl:debugStart',          (_e, code)   => replSession.debugStart(code));
ipcMain.handle('repl:debugStep',           (_e, action) => replSession.debugStep(action));
ipcMain.handle('repl:debugStop',           ()           => replSession.debugStep('stop'));
