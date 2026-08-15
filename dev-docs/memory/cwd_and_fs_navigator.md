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
   - When running the script, `runCode` uses the directory containing `activeTabObj.vfsPath` directly, eliminating path compounding/duplication (`/examples/.../examples/...`).



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
   - `ide/src/fs/pathUtils.js`: Centralized pure path operations (`sanitizeVfsPath`, `getParentDir`, `isLocalDiskPath`, `getFileName`, `getFileBaseName`).
   - `ide/src/fs/examples.js`: Encapsulated example extraction, binary media detection, and target VFS/Local cloning.
   - 100% test coverage with dedicated Vitest suites in `pathUtils.test.js` and `examples.test.js` (526 tests passing).





