// preload.js — Electron context-bridge for the MLab desktop app.
//
// Exposes window.nativeFS to the renderer: a path-based filesystem
// API plus OS-level "reveal in explorer" actions. The renderer
// (ide/src/fs/local.js) prefers this over the browser's File System
// Access API when it's present, so the desktop build gets a native
// folder picker and can open the mounted folder in the real OS file
// manager.
//
// Security: the main process validates every path against the
// mounted root so a compromised renderer cannot escape to other
// parts of the filesystem. This file itself deliberately does NOT
// import node's `fs` — all real I/O is routed through ipcRenderer.invoke.
const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('nativeFS', {
    // Folder mount & persistence.
    pickDirectory: () => ipcRenderer.invoke('fs:pickDirectory'),

    // Tree / file operations, all rooted at `root` (the absolute
    // folder path returned by pickDirectory). Paths passed in are
    // "/"-rooted, relative to that root.
    listTree:  (root)                  => ipcRenderer.invoke('fs:listTree', root),
    readFile:  (root, path)            => ipcRenderer.invoke('fs:readFile', root, path),
    writeFile: (root, path, content)   => ipcRenderer.invoke('fs:writeFile', root, path, content),
    mkdir:     (root, path)            => ipcRenderer.invoke('fs:mkdir', root, path),
    remove:    (root, path)            => ipcRenderer.invoke('fs:remove', root, path),
    rename:    (root, oldPath, newPath)=> ipcRenderer.invoke('fs:rename', root, oldPath, newPath),
    exists:    (root, path)            => ipcRenderer.invoke('fs:exists', root, path),

    // Shell integrations. `path` is optional — when absent, reveals
    // the mounted root itself.
    revealInExplorer: (root, path)     => ipcRenderer.invoke('shell:reveal', root, path || ''),
    showItemInFolder: (root, path)     => ipcRenderer.invoke('shell:showItem', root, path),
});
