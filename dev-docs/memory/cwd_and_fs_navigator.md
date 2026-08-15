# Current Folder Bar, FS Mode Combo & File Navigator Architecture

## Context & Problem
When running scripts in the IDE (especially examples or user projects doing file I/O such as `audiowrite`, `imwrite`, `save`, `csvwrite`), files were previously saved to relative paths that could desynchronize between the native REPL process (`numkit_repl.exe`) and the browser/Electron VFS. Furthermore, users lacked MATLAB-style visibility and control over the active working directory and filesystem mode (Local FS vs Virtual FS).

## Decisions & Implementation

1. **Current Folder Bar (`CurrentFolderBar.jsx`)**:
   - Placed directly below the main toolbar, mirroring MATLAB's Current Folder strip.
   - Includes a Filesystem Selector Combo (`<select>`): `⚡ Virtual File System (Temporary)` vs `📁 Local File System`.
   - Displays and allows manual input of Current Working Directory (`cwd`).
   - Features an Up (`⬆`) one directory button and a Browse (`📁`) button that triggers `FileNavigatorModal`.

2. **File Navigator Modal (`FileNavigatorModal.jsx`)**:
   - Rendered with identical geometry to `FigureWindow` (85vw × 80vh overlay).
   - Breadcrumb navigation with clickable directory segments.
   - Comprehensive file table sorting by Name, Type, Size, and Date Modified.
   - File Info sidebar with metadata, "Open in Editor", and "Reveal in Explorer" actions.
   - "Select as Current Folder" action to bind the directory as the active CWD.

3. **Examples Auto-Cloning per Filesystem Mode**:
   - When double-clicking an example script in the Examples browser:
     - **In Virtual FS Mode (`fsMode === 'virtual'`)**:
       - Creates the folder in Virtual FS: `/numkit_ide/examples/<script_base_name>`.
       - Copies only the opened script into `/numkit_ide/examples/<script_base_name>/`.
       - Sets `virtualCwd` to `/numkit_ide/examples/<script_base_name>` (reflected in `CurrentFolderBar` and REPL virtual sync).
     - **In Local FS Mode (`fsMode === 'local'`)**:
       - Creates the folder in OS temporary directory: `<os_temp_dir>/numkit/examples/<script_base_name>` (e.g. `C:\Users\User\AppData\Local\Temp\numkit\examples\arithmetic`).
       - Copies only the opened script into `<os_temp_dir>/numkit/examples/<script_base_name>`.
       - Sets `localCwd` to `<os_temp_dir>/numkit/examples/<script_base_name>`.
     - Opens the script in the editor tab.
   - When running the script, `runCode` automatically aligns the active filesystem mode (`fsMode`) with the tab's source (`localFolder` vs `temporary`), switching mode and setting `targetCwd` in the matching filesystem (e.g. `/numkit_ide/examples/audio_io_roundtrip` for Virtual FS without corrupting it into `C:\numkit_ide\...`).



4. **Synchronized Electron Temporary Backend**:
   - In Electron desktop mode, `tempFS` routes directly to `app.getPath('userData')/temporary`.
   - Native `numkit_repl.exe` and IDE VFS read and write to the exact same physical folder.

5. **REPL CLI and IPC CWD Control**:
   - `numkit_repl.exe` supports `--fs=<mode>`, `--cwd=<path>`, and pipe protocol commands `__SET_CWD__:<path>`, `__GET_CWD__`, `__SET_FS__:<fs>`.
   - In Electron desktop mode, `replSession.run` and `replSession.setCwd` automatically resolve pure virtual paths (e.g. `/numkit_ide/examples/...`) to physical directories under `TEMP_ROOT` (`resolveReplCwd`), ensuring `numkit_repl.exe` executes `cd(...)` without errors while preserving pure virtual paths in the IDE UI.
   - IPC `window.nativeFS.runRepl(code, { cwd })` and `window.nativeFS.setCwd(path)` pass CWD parameters dynamically.


6. **Local FS Default Home & Dynamic CWD Switching**:
   - When Local FS is not explicitly selected by the user, `localFS` defaults to the OS user home directory (`app.getPath('home')` / `os.homedir()`).
   - `localFS.setRootPath(...)` allows changing active disk directories dynamically from the path input, sidebar double-click, or the File Navigator modal.
   - Breadcrumbs calculate path segments cleanly for Windows drive paths (`C:\Users\...`) and POSIX paths (`/home/...`) to prevent path duplication or illegal relative concats.

7. **Shallow Directory Listing & On-Demand (Lazy) Folder Loading**:
   - Replaced monolithic recursive `listTree()` with shallow `listDir(root, relPath)` across Electron IPC, preload, `localFS`, and `tempFS`.
   - `Sidebar` and `FileNavigatorModal` query only the immediate children of the active folder (`< 1 ms`, minimal memory).
   - In `Sidebar.jsx`, expanding a folder in `TreeRow` dynamically loads that folder's children on demand.
8. **Sidebar Navigation, Click Behaviors & Windows Root Fixes**:
   - Removed obsolete unmount button, unmount strip, and redundant "Open folder…" toolbar button (directory selection is handled via `CurrentFolderBar` and `FileNavigatorModal`).
   - Click behaviors in sidebar file tree:
     - **Folders**: 1 click (on arrow or text) expands/collapses the subfolder; 2 clicks (double-click) navigates into the folder setting it as active CWD.
     - **`..` (Parent Directory)**: 1 click selects the row; 2 clicks (double-click) navigates up one level.
     - **Files**: 1 click selects; 2 clicks opens the file in editor.
   - Fixed `safePath` in `main.js` to ensure Windows drive roots (e.g. `C:\`, `D:\`) with trailing path separators properly validate without false `Path escapes mounted root` errors.

9. **Modular Architecture & Test Coverage (`pathUtils.js` & `examples.js`)**:
   - `ide/src/fs/pathUtils.js`: Centralized pure path operations (`sanitizeVfsPath`, `sanitizeLocalPath`, `getParentDir`, `isLocalDiskPath`, `getFileName`, `getFileBaseName`, `getTabPaths`). Handles bare drive letter inputs (e.g. `C:`, `c:`, `C:\`, `C:/`) and root slash inputs (`/`, `\`) cleanly in Local FS mode, navigating to the active drive root (e.g. `C:\` or `D:\`).
   - `ide/src/fs/examples.js`: Encapsulated example extraction, binary media detection, and target VFS/Local cloning.
   - 100% test coverage with dedicated Vitest suites in `pathUtils.test.js`, `examples.test.js`, `CurrentFolderBar.render.test.jsx`, `FileNavigatorModal.render.test.jsx`, and `Sidebar.render.test.jsx` (543 tests passing across 40 test files).

10. **Editor Tab Context Menu Actions**:
    - Right-clicking any tab in `TabStrip` provides:
      - `New tab`
      - `Rename`
      - `Copy file name`: Copies file basename (e.g. `group_aggregation.m`) to system clipboard.
      - `Copy file path`: Copies full path (`C:\Users\...\group_aggregation.m` or `/numkit_ide/examples/...`) to clipboard.
      - `Copy folder path`: Copies parent directory (`C:\Users\...\group_aggregation` or `/numkit_ide/examples`) to clipboard.
      - `Show in explorer`: Automatically resolves directory, aligns active `fsMode` and `cwd`, and opens `FileNavigatorModal` positioned directly at that folder.
      - `Close`, `Close all`, `Close others`.
    - Context menu uses monochrome vector SVG icons matching the IDE theme.
    - Verified and covered by Vitest suites.

11. **Unified Modal Window System (`ModalWindow.jsx`)**:
    - Created `ide/src/components/ui/ModalWindow.jsx` as a single shared foundation for all dialogs and modals (`FileNavigatorModal`, `PreferencesModal`, `FigureWindow`, and future widgets).
    - Unified design system:
      - Window container with consistent `1px solid var(--line)`, rounded corners (`var(--r-lg)`), and elevation glow shadow (`box-shadow: 0 24px 80px oklch(...)`).
      - Fullscreen maximize / restore support with toggle SVG icon (`.modal-window.is-max`).
      - Clean, minimal, and informative headers without visual clutter:
        - `Explorer`: Clean title for file navigation modal.
        - `Inspector`: Clean title with variable name for variable inspector/editor.
        - `Figure`: Clean title for figure graphics window.
      - Neutral monochrome vector SVG icons for toolbar and file types.
      - Escape key handling and backdrop click dismissal.
    - Test coverage: 548 tests passing across 41 test files (including dedicated `ModalWindow.render.test.jsx`).

12. **Explorer Stats & Column Sorting (`FileNavigatorModal.jsx`, `main.js`, `temporary.js`, `local.js`)**:
    - Removed redundant toolbar `Up` button in favor of the in-table `..` row.
    - Simplified parent row label to clean `..`.
    - Fixed `Size` and `Modified` metadata fetching:
      - Electron backend (`desktop/main.js` `fs:listDir`): added `fsp.stat()` resolution for real file sizes and mtime.
      - Web Chromium backend (`fs/local.js` `listDir`): added `handle.getFile()` query for size and lastModified.
      - Virtual filesystem (`temporary.js` `listDir`): computed sizes from memory buffer lengths/string sizes and populated modified timestamps.
    - Implemented full column click sorting for `Name`, `Type`, `Size`, and `Modified` with interactive asc/desc toggles and `▲`/`▼` indicators.

13. **Streamlined Bottom Dock (`IDE.jsx`)**:
    - Removed the `Reference` tab and panel from `BottomDock`, keeping only `Console` and `Workspace` in the dock tabs.
    - Cleaned up unused imports and bundle references, reducing bundle weight.

14. **Subdirectory Path Preservation Across Restarts (`examples.js`, `Sidebar.jsx`, `FileNavigatorModal.jsx`)**:
    - **Problem**: When an example or local file located inside a subdirectory (e.g. `.../examples/step_response/step_response.m`) was extracted or opened, `openExample` assigned `vfsPath: '/${fname}'` (`/step_response.m`), losing the intermediate folder `step_response`. Upon restart, when `localCwd` was restored to root (`.../examples`), `getTabPaths` resolved `.../examples/step_response.m` instead of `.../examples/step_response/step_response.m`.
    - **Fix**:
      - `fs/examples.js`: now constructs and assigns the full target file path (`targetDir + sep + fname`) to `vfsPath`.
      - `Sidebar.jsx` and `FileNavigatorModal.jsx`: resolve full absolute disk paths for all opened local files before passing to `onOpenFile`.
      - `getTabPaths` recognizes absolute disk paths via `isLocalDiskPath` and preserves the exact subfolder and file path regardless of CWD changes.

15. **Full Modal Window Unification (`ModalWindow.jsx`, `Workspace.jsx`, `FigureWindow.jsx`, `FileNavigatorModal.jsx`, `PreferencesModal.jsx`)**:
    - **Problem**: `FigureWindow` and `Workspace` (`VariableEditor`) implemented ad-hoc titlebar, window wrapper, and maximize/close button HTML/CSS, resulting in disparate styling (missing button pill background in Inspector) and duplicated logic.
    - **Fix**:
      - Unified all modal dialogs (`Figure`, `Inspector`, `Explorer`, `Preferences`) to use `<ModalWindow>`.
      - Styled `.modal-title-right` as a consistent pinned control pill with background `var(--bg-3)`, border `1px solid var(--line)`, and uniform `.ve-close` maximize and close buttons.
      - Removed duplicate CSS rules for `.ve-titlebar`, `.ve-title-right`, `.fw-titlebar`, `.fw-title-right`.
16. **Editor Tab Close Button Behavior (`IDE.jsx`)**:
    - **Problem**: When only one tab was open, the close button `×` was hidden due to a legacy `tabs.length > 1` condition, preventing closing the tab.
    - **Fix**: Removed the conditional so every tab displays the `×` close button; closing the single/last tab cleanly resets the editor with a fresh `untitled.m` tab.
    - Test coverage: 549 tests passing across 41 test files.





