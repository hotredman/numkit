# Input Focus and Cursor Lifecycle Gotchas

## Context
Users occasionally encountered a scenario where the text cursor disappeared and input fields across the IDE (editor, console, path bar, modal inputs) could not be focused or clicked.

## Root Causes Identified & Fixed

1. **Stale `userSelect: 'none'` on `document.body` during split / pane resizing (`ResizeHandle.jsx`, `MatrixPanel.jsx`)**:
   - **Mechanism**: When resizing panels or plot columns, `document.body.style.userSelect = 'none'` and `document.body.style.cursor = 'col-resize'` were applied. If the pointer event was cancelled, pointer capture was lost (e.g. mouse moved outside Electron window, Alt-Tab, hotkey, modal popup), or if the component unmounted during the drag, `document.body.style.userSelect` remained set to `'none'` indefinitely.
   - **Impact**: In Chromium/Blink, `user-select: none` on `<body>` suppresses text selection and stops clicks from placing the text caret into `<input>` and `<textarea>` elements.
   - **Fix**: Added `pointerup`, `pointercancel`, `mouseup`, and `blur` global listeners on `window` plus unconditional cleanup inside `useEffect` on unmount.

2. **Caret Color Transparency in `<textarea>` (`SyntaxEditor.jsx`, `numkit-ide.css`)**:
   - **Mechanism**: The custom syntax editor renders a transparent `<textarea>` over a `<pre>` highlight layer. The textarea relied on `caretColor: C.accent`. If `C.accent` was not resolved or defaulted to `currentColor` (which is `transparent`), the text caret became completely invisible.
   - **Fix**: Enforced `caret-color: var(--accent, #7c6ff0)` directly in CSS (`.nk-editor-textarea`) with `cursor: text !important`, and added a fallback `C?.accent || 'var(--accent, #7c6ff0)'` in JSX inline styles. Added surface click delegation on `editorAreaRef` so clicking anywhere on the editor canvas automatically focuses the textarea.

3. **Global Keyboard Interception (`FigureWindow.jsx`, `MatrixPanel.jsx`)**:
   - **Mechanism**:
     - `FigureWindow.jsx` attached a global `keydown` listener listening for `'0'` (reset viewport) and `'Escape'`. This fired even when `FigureWindow` was embedded inside `FiguresPane` or `InlinePlot`, intercepting `'0'` keystrokes while the user typed in other inputs.
     - `MatrixPanel.jsx` attached a global `keydown` listener that called `e.preventDefault()` on arrow keys, `Home`, `End`, `PageUp`, and `PageDown` without checking whether an input/textarea had active focus.
   - **Fix**: Restricted `FigureWindow` keydown handling to non-embedded standalone mode and added an explicit check `e.target?.tagName` / `isContentEditable` to ignore keystrokes directed at active input fields. Added the same input element guard to `MatrixPanel.jsx`.
