# TVIDE — Project Memory

Text-mode IDE for PHP / web dev. Built on [magiblot/tvision](https://github.com/magiblot/tvision) (Turbo Vision port). RHIDE-style menu UI.

## Build

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/tvide [files... | project-dir]
```

Tvision is vendored at `deps/tvision`. CMake adds it as subdirectory, links `tvide` against `tvision` lib. C++17. Warnings: `-Wall -Wextra -Wno-deprecated -Wno-unknown-pragmas -Wno-missing-field-initializers`.

When adding new `.cpp`, register it in root `CMakeLists.txt` `add_executable(tvide ...)`.

## Source layout

```
src/
  main.cpp              # entry
  app.h / app.cpp       # TVIDEApp, menus, status line, command dispatch, idle loop, relayout()
  editor/
    editor.h            # TSyntaxEditor, TSyntaxEditWindow, TLineGutter, EditorSettings, TSyntaxColorsDialog
    editor.cpp          # syntax-highlighted draw(), options dialog
    editwindow.cpp      # window scaffolding (scrollbars + indicator + gutter + editor)
    linenums.cpp        # gutter view
  syntax/
    lexer.h / lexer.cpp # SyntaxLexer base, TokenType, factory by ext, color map, ThemeManager
    <lang>.cpp          # one file per language
  dialogs/
    dialogs.h           # createGotoLineDialog(), createFindInFilesDialog(), createAboutDialog()
    *.cpp               # one per dialog
  panels/
    panels.h            # TFileTreePanel, TMessagePanel, TStructurePanel, TUnsortedStringCollection, TWindowListPanel, TTerminalWindow
    filetree.cpp        # left-docked sidebar
    structure.cpp       # right-docked sidebar
    messages.cpp        # bottom panel (inserted into desktop)
    terminal.cpp        # PTY-backed terminal window (editor-style, lives on desktop)
    winlist.cpp         # tool window listing open desktop windows
  project/
    project.h / project.cpp # JetBrains .idea parser
```

## Architecture cheat-sheet

### Application
- `TVIDEApp : TApplication`. Owns docked panels (`fileTreePanel`, `structurePanel`) inserted **into application** (not desktop). `messagePanel` goes into desktop.
- `relayout()` repositions docked panels and resizes desktop between them. Triggered by `cmRelayout` broadcast.
- `idle()` tracks last focused editor, validates it's still alive, drives structure-panel refresh and external-file-change detection. **Also pumps PTYs** for terminal windows.

### Command IDs
All in `app.h`, range `2001+` to avoid tvision built-ins. Add new ones there.

### Menu wiring
`initMenuBar()` in `app.cpp`. Built with `+ *new TMenuItem(...)` chain. `handleEvent()` switches on `event.message.command` and dispatches.

### Editor windows
- `TSyntaxEditWindow : TWindow` — wraps `TSyntaxEditor : TFileEditor`, plus scrollbars, indicator, optional `TLineGutter`.
- Multiple instances on `deskTop`. `options |= ofTileable`.
- Open via `openEditor(filename, visible)`.

### Lexer factory
`SyntaxLexer::create(filename)` in `syntax/lexer.cpp` dispatches by extension. Add a new lang by:
1. New file `syntax/<lang>.cpp`.
2. Subclass `SyntaxLexer`, implement `nextToken()`.
3. Register in factory.
4. Add to `CMakeLists.txt`.

### Panels
- Docked panels (`TFileTreePanel`, `TStructurePanel`) override `changeBounds()` to broadcast `cmRelayout`, override `sizeLimits()`, set `flags = 0`, `growMode = 0`, custom palette via `getPalette()`.
- Bottom panel (`TMessagePanel`) lives in desktop, tileable.
- New: `TWindowListPanel` is a desktop tool window listing currently open windows; `TTerminalWindow` is a desktop window that hosts a PTY shell.

### File-tree → editor message pattern
`cmFileTreeOpen` event with `infoPtr=filename` (static buffer, not heap). App side opens the file. Same pattern for window-list panel: `cmWinListSelect` with `infoPtr=TWindow*`.

### TUnsortedStringCollection
Custom `TCollection` that preserves insertion order and `delete[]`s C-string items. Used for list boxes everywhere.

### Settings & themes
`EditorSettings::instance()` singleton holds toggles + colors. `ThemeManager::instance()` persists colour themes.

## Conventions

- **No comments unless WHY non-obvious.** Existing code has historical inline numeric labels (`L1`, `M2`, `C1`) — those came from issue tracking; don't add new ones.
- Length-prefixed `TInputLine` data: `buf[0]` = length, `buf[1..]` = chars. See `findInFiles()` for example.
- `execDialog(d, data)` helper handles `validView` + setData/getData lifecycle.
- `dynamic_cast<TSyntaxEditWindow *>(view)` is the standard "is this an editor window?" check.
- Iterate desktop ring: `TView *p = deskTop->first(); TView *t = p; do { ... t = t->next; } while (t != p);`. Always **collect into a vector first** before mutating (close, etc.) — see `winCloseAll()`.

## State / WIP

- `WORK.md` at repo root holds short-term plan and decisions for the active session. Rewrite as task changes.
- `ISSUES.md` is a historical issue log; `bc1d75c` claimed to fix 29/32 of them.

## What NOT to do

- Don't insert docked sidebar panels into `deskTop` — they go into `TProgram::application`. Otherwise they fight the desktop's tiling and resize logic.
- Don't free `infoPtr` strings used in event messages — pattern uses static buffers.
- Don't call `TWindow::changeBounds` on docked panels via the public path — use `TWindow::changeBounds` directly inside `relayout()` to avoid re-broadcasting `cmRelayout` (loop).
