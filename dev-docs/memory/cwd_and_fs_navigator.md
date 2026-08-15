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

3. **Examples Auto-Cloning to Active FS**:
   - When running an example script (e.g., `audio_io_roundtrip.m`):
     - The IDE automatically creates a dedicated subdirectory `examples/<script_base_name>/` (e.g. `examples/audio_io_roundtrip/`) in the active filesystem (Local or Temporary).
     - Copies the script and any companion assets into this folder.
     - Sets this subdirectory as the active `cwd` for the execution run.
     - Generated output files (WAV, MAT, PNG, etc.) land directly inside `examples/<script_base_name>/`.

4. **Synchronized Electron Temporary Backend**:
   - In Electron desktop mode, `tempFS` routes directly to `app.getPath('userData')/temporary`.
   - Native `numkit_repl.exe` and IDE VFS read and write to the exact same physical folder.

5. **REPL CLI and IPC CWD Control**:
   - `numkit_repl.exe` supports `--fs=<mode>`, `--cwd=<path>`, and pipe protocol commands `__SET_CWD__:<path>`, `__GET_CWD__`, `__SET_FS__:<fs>`.
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
   - Removed obsolete unmount button and unmount strip.
   - Click behaviors in sidebar file tree:
     - **Folders**: 1 click (on arrow or text) expands/collapses the subfolder; 2 clicks (double-click) navigates into the folder setting it as active CWD.
     - **`..` (Parent Directory)**: 1 click selects the row; 2 clicks (double-click) navigates up one level.
     - **Files**: 1 click selects; 2 clicks opens the file in editor.
   - Fixed `safePath` in `main.js` to ensure Windows drive roots (e.g. `C:\`, `D:\`) with trailing path separators properly validate without false `Path escapes mounted root` errors.




