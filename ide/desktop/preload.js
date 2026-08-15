// preload.js — Electron context-bridge for the Numkit IDE desktop app.
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
    getTempRoot:   () => ipcRenderer.invoke('fs:getTempRoot'),

    // Tree / file operations, all rooted at `root` (the absolute
    // folder path returned by pickDirectory). Paths passed in are
    // "/"-rooted, relative to that root.
    listTree:  (root)                  => ipcRenderer.invoke('fs:listTree', root),
    readFile:  (root, path)            => ipcRenderer.invoke('fs:readFile', root, path),
    // Synchronous read used by the WASM engine's sync csvread / load
    // callbacks. ipcRenderer.sendSync blocks the renderer until main
    // responds; with a local disk read it's typically 1-5 ms per file.
    // The renderer used to pre-load every mounted file into a JS Map
    // to avoid this round-trip (proven OOM on large folders), but
    // sync IPC is the right answer — no proactive memory cost.
    readFileSync: (root, path)         => ipcRenderer.sendSync('fs:readFileSync', root, path),
    // Synchronous BINARY read — raw bytes (Uint8Array) for imread/audioread.
    readFileBytesSync: (root, path)    => ipcRenderer.sendSync('fs:readFileBinarySync', root, path),
    existsSync:   (root, path)         => ipcRenderer.sendSync('fs:existsSync', root, path),
    writeFile: (root, path, content)   => ipcRenderer.invoke('fs:writeFile', root, path, content),
    // Binary write — raw bytes (imwrite/audiowrite).
    writeFileBytes: (root, path, bytes)=> ipcRenderer.invoke('fs:writeFileBinary', root, path, bytes),
    mkdir:     (root, path)            => ipcRenderer.invoke('fs:mkdir', root, path),
    remove:    (root, path)            => ipcRenderer.invoke('fs:remove', root, path),
    rename:    (root, oldPath, newPath)=> ipcRenderer.invoke('fs:rename', root, oldPath, newPath),
    exists:    (root, path)            => ipcRenderer.invoke('fs:exists', root, path),

    // Shell integrations. `path` is optional — when absent, reveals
    // the mounted root itself.
    revealInExplorer: (root, path)     => ipcRenderer.invoke('shell:reveal', root, path || ''),
    showItemInFolder: (root, path)     => ipcRenderer.invoke('shell:showItem', root, path),

    // ── External tools ──────────────────────────────────────────────
    // Open a native file-picker dialog and return the selected path,
    // or null if cancelled. Used by PreferencesModal to browse for
    // the numkit / numkit_codegen / C++ compiler executables.
    pickFile: (opts) => ipcRenderer.invoke('fs:pickFile', opts),

    // Push the current tool paths from the renderer-side settings
    // store into main.js memory so subsequent spawns use them.
    updateSettings: (settings) => ipcRenderer.invoke('settings:update', settings),

    // Ask main.js to resolve the exe paths it would actually use right
    // now (applying the priority chain: explicit setting → next to IDE
    // exe → PATH). Used by PreferencesModal to show the effective path
    // even when the stored setting is empty.
    resolveSettings: () => ipcRenderer.invoke('settings:resolve'),

    // ── Persistent REPL session ──────────────────────────────────
    // Execute code in the long-lived numkit_repl --ide-session process.
    // Returns { stdout, stderr, vars, exitCode, notFound?, sessionRestarted? }
    runRepl: (code, opts) => ipcRenderer.invoke('repl:run', code, opts),

    // Set the current working directory in the REPL session.
    setCwd: (newCwd) => ipcRenderer.invoke('repl:setCwd', newCwd),

    // Send __RESET__ to the REPL session (clear all, workspace wiped).
    resetRepl: () => ipcRenderer.invoke('repl:reset'),

    // Kill the REPL process entirely (it will restart on next runRepl).
    killRepl: () => ipcRenderer.invoke('repl:kill'),

    // Transpile + AOT-compile + run the given source code string via
    // numkit_codegen --run.  Returns a promise that resolves to
    //   { stdout: string, stderr: string, exitCode: number, notFound?: true }
    // The main process writes the code to a temp file, spawns the
    // configured codegen binary, collects stdout/stderr, cleans up,
    // and resolves when the child exits.
    runCodegen: (code, opts) => ipcRenderer.invoke('codegen:run', code, opts),

    // ── Var introspection ──────────────────────────────────────────────────
    // All of these route through the persistent native REPL process via the
    // pipe protocol (__INSPECT__, __GET_SHAPE__, etc.).  They return
    // Promises that resolve with the parsed JSON object from the response.
    // If the process is not running it is spawned on demand.
    getVarShape:  (name)                      => ipcRenderer.invoke('repl:getVarShape', name),
    getVarData:   (name)                      => ipcRenderer.invoke('repl:getVarData',  name),
    getVarPage:   (name, page)                => ipcRenderer.invoke('repl:getVarPage',  name, page),
    getVarTile:   (name, r0, c0, rows, cols, page) =>
                  ipcRenderer.invoke('repl:getVarTile', name, r0, c0, rows, cols, page),
    getVarStats:  (name, page)                => ipcRenderer.invoke('repl:getVarStats', name, page),
    getVarFigure: (name, opts)                => ipcRenderer.invoke('repl:getVarFigure', name, opts),
    inspectPath:  (name, path)                => ipcRenderer.invoke('repl:inspectPath', name, path),

    // ── AST & Script Graph analysis ──────────────────────────────────────
    buildAST:         (source) => ipcRenderer.invoke('repl:buildAST', source),
    buildScriptGraph: (source) => ipcRenderer.invoke('repl:buildScriptGraph', source),

    // ── Debugger ──────────────────────────────────────────────────────────
    // Set breakpoint lines (stored for the next debugStart call).
    debugSetBreakpoints: (lines)   => ipcRenderer.invoke('repl:debugSetBreakpoints', lines),
    // Start a debug session with stored breakpoints. Resolves with
    //   { status:'paused', pauseState, output, figures, ... }  on first pause,
    //   { status:'completed'|'error', output, vars, ... }      on completion.
    debugStart:          (code)    => ipcRenderer.invoke('repl:debugStart', code),
    // Step the paused debugger: action = 'continue'|'step_over'|'step_into'|'step_out'
    // Returns the same shape as debugStart.
    debugStep:           (action)  => ipcRenderer.invoke('repl:debugStep', action),
    // Stop the debug session (sends __DEBUG_CMD__:stop).
    debugStop:           ()        => ipcRenderer.invoke('repl:debugStop'),
});
