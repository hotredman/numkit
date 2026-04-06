// main.js — MLab IDE desktop shell
// Supports two modes:
//   Dev mode:  spawns Vite dev server, loads from http://
//   Prod mode: loads pre-built static files from dist/
const { app, BrowserWindow } = require('electron');
const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');
const http = require('http');

const IDE_DIR = path.resolve(__dirname, '..');
const DIST_DIR = path.join(__dirname, 'dist');
const IS_PROD = fs.existsSync(path.join(DIST_DIR, 'index.html'));

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
