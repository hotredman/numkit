// main.js — MLab IDE desktop shell
// Supports two modes:
//   Dev mode:  spawns Vite dev server, loads from http://
//   Prod mode: loads pre-built static files from dist/
const { app, BrowserWindow, dialog, shell, ipcMain } = require('electron');
const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');
const fsp = require('fs/promises');
const http = require('http');

const IDE_DIR = path.resolve(__dirname, '..');
const DIST_DIR = path.join(__dirname, 'dist');
const IS_PROD = fs.existsSync(path.join(DIST_DIR, 'index.html'));
const PRELOAD = path.join(__dirname, 'preload.js');

let mainWindow = null;
let viteProcess = null;

function createWindow(url) {
  mainWindow = new BrowserWindow({
    width: 1400,
    height: 900,
    title: 'MLab IDE',
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      preload: PRELOAD,
    },
  });

  console.log('[MLab] Loading:', url);
  if (url.startsWith('http')) {
    mainWindow.loadURL(url);
  } else {
    mainWindow.loadFile(url);
  }
  mainWindow.on('closed', () => { mainWindow = null; });
}

// ── Dev mode: start Vite and detect URL ──

function startVite() {
  const NODE_EXE = path.join(process.env.PROGRAMFILES || 'C:\\Program Files', 'nodejs', 'node.exe');
  const VITE_BIN = path.join(IDE_DIR, 'node_modules', 'vite', 'bin', 'vite.js');

  return new Promise((resolve, reject) => {
    console.log('[MLab] Dev mode — starting Vite');

    viteProcess = spawn(NODE_EXE, [VITE_BIN, '--host', '127.0.0.1'], {
      cwd: IDE_DIR,
      stdio: ['ignore', 'pipe', 'pipe'],
    });

    viteProcess.on('error', (err) => {
      console.error('[MLab] Failed to start Vite:', err.message);
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
        console.error('[MLab] Vite did not start within 30s');
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
  let loadTarget;

  if (IS_PROD) {
    console.log('[MLab] Production mode — loading from dist/');
    loadTarget = path.join(DIST_DIR, 'index.html');
  } else {
    const viteUrl = await startVite();
    if (viteUrl) {
      await waitForServer(viteUrl);
      loadTarget = viteUrl;
    } else {
      console.error('[MLab] No Vite URL detected, exiting');
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

ipcMain.handle('fs:listTree', async (_e, root) => {
  async function walk(dir, rel) {
    const list = await fsp.readdir(dir, { withFileTypes: true });
    const entries = [];
    for (const d of list) {
      // Skip obvious junk that shouldn't clutter the tree.
      if (d.name.startsWith('.DS_Store')) continue;
      const full = path.join(dir, d.name);
      const itemPath = rel === '/' ? `/${d.name}` : `${rel}/${d.name}`;
      const node = {
        name: d.name,
        path: itemPath,
        type: d.isDirectory() ? 'folder' : 'file',
      };
      if (d.isDirectory()) {
        try { node.children = await walk(full, itemPath); }
        catch (err) { node.children = []; }
      }
      entries.push(node);
    }
    entries.sort((a, b) => {
      if (a.type !== b.type) return a.type === 'folder' ? -1 : 1;
      return a.name.localeCompare(b.name);
    });
    return entries;
  }
  return walk(path.resolve(root), '/');
});

ipcMain.handle('fs:readFile', async (_e, root, relPath) => {
  const full = safePath(root, relPath);
  try { return await fsp.readFile(full, 'utf8'); }
  catch (err) {
    if (err.code === 'ENOENT' || err.code === 'EISDIR') return null;
    throw err;
  }
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
