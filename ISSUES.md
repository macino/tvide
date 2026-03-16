# TVIDE — Known Issues & Bug Report

This document lists all known bugs, UX issues, and missing features in TVIDE.
Each issue describes the **expected behavior** vs **actual behavior** so that an
AI agent or developer can understand and fix the problem without ambiguity.

The project uses magiblot/tvision (C++ port of Borland Turbo Vision). The source
is at `/home/tomas/vcs/tvide`. Build with `cmake -B build && cmake --build build`.

---

## Severity Legend

- **CRITICAL** — Crash, memory corruption, or data loss
- **HIGH** — Incorrect behavior that breaks a major feature
- **MEDIUM** — Wrong behavior, stubs, or significant UX gaps
- **LOW** — Polish, minor UX improvements, missing convenience features

---

## Fix Summary

**Date:** 2026-03-16

| Status | Count |
|--------|-------|
| ✅ Fixed | 29 |
| ⏸️ Deferred/Won't Fix | 2 |
| 🔴 Open | 1 |

---

## CRITICAL Issues

### C1. focusItem(range-1) crashes on empty list — ✅ Fixed (2026-03-16)

**Files:** `src/panels/filetree.cpp` (kbEnd handler), `src/panels/structure.cpp` (kbEnd handler)

**Actual behavior:** When the file tree or structure panel list is empty (`listBox->range == 0`),
pressing the End key calls `listBox->focusItem(listBox->range - 1)` which evaluates to
`focusItem(-1)`. The `short` type wraps to -1. TListViewer::focusItem does not guard against
negative values, causing undefined behavior or a crash.

**Expected behavior:** Pressing End (or PgUp/PgDn) on an empty list should be a no-op. All
keyboard navigation handlers in both `TFileTreePanel::handleEvent` and
`TStructurePanel::handleEvent` must guard with `if (listBox->range > 0)` before calling
`focusItem`.

**How to reproduce:** Open TVIDE without any project or files. Open the structure panel
(Tools → Structure). Press the End key.

**Fix:** Add `if (listBox->range <= 0) break;` at the top of every kbEnd, kbHome, kbPgUp,
kbPgDn, kbUp, kbDown case in both panel handleEvent methods. Alternatively, add a single
early return: `if (listBox->range <= 0) { clearEvent(event); return; }` before the switch.

---

### C2. Dangling pointer: trackedEditor in structure panel — ✅ Fixed (2026-03-16)

**File:** `src/panels/structure.cpp` — `TStructurePanel::trackedEditor` member,
`navigateToSymbol()` method (around line 565)

**Actual behavior:** `trackedEditor` is a raw `TSyntaxEditor*` stored when `refresh()` is
called. If the user closes the editor window that trackedEditor points to, the pointer becomes
dangling. When the user then presses Enter in the structure panel, `navigateToSymbol()` dereferences
`trackedEditor->bufLen`, `trackedEditor->buffer`, `trackedEditor->curPtr`, and
`trackedEditor->owner` — all on freed memory. This causes a crash or memory corruption.

Note: `app.cpp` idle() validates `lastEditorWindow` against the desktop's child list, but
`trackedEditor` inside the structure panel is never validated.

**Expected behavior:** `navigateToSymbol()` must verify that `trackedEditor` still points to a
valid, live editor before using it. One approach: before dereferencing, walk the desktop's view
list and check that `trackedEditor->owner` is still inserted. Another approach: clear
`trackedEditor` when the editor is destroyed — override `TStructurePanel::handleEvent` to listen
for a broadcast sent when editor windows close, or add a validation step using `lastEditorWindow`
from the app.

**How to reproduce:** Open a file. Open the structure panel. Close the file. Click on a symbol
in the structure panel and press Enter.

**Fix approach:** In `navigateToSymbol()`, before using `trackedEditor`, verify it matches
the app's `lastEditorWindow->getEditor()`. If `lastEditorWindow` is null or its editor differs,
set `trackedEditor = nullptr` and return. Alternatively, in `refresh()`, if editor is nullptr,
set `trackedEditor = nullptr` (this already happens but doesn't protect against the editor being
freed between refresh calls).

---

## HIGH Issues

### H1. JS/TS variable parser uses find_first_of incorrectly — ✅ Fixed (2026-03-16)

**File:** `src/panels/structure.cpp`, around line 272, inside the JavaScript/TypeScript parser
(`parseJavaScript`).

**Actual behavior:** After the outer condition correctly matches `const `/ `let `/ `var ` at
the line's indent position, the code extracts the variable name using:
```cpp
auto p = ln.find_first_of("cletvar", indent);
p = ln.find(' ', p);
```
`find_first_of("cletvar", indent)` searches for any single character from the set
{c, l, e, t, v, a, r} starting at `indent`. This works by coincidence for typical cases
because "const" starts with 'c', "let" starts with 'l', "var" starts with 'v'. However,
it would fail on indented lines if a matching character appears before the keyword (e.g.,
a comment or different token at the start). It's also confusing and fragile code.

**Expected behavior:** The name extraction should use the same keyword position that the
outer condition already determined. Since we know the keyword starts at `indent`, calculate
the name start directly:
```cpp
size_t kwEnd = ln.find(' ', indent); // end of "const"/"let"/"var"
if (kwEnd == std::string::npos) return;
size_t nameStart = kwEnd + 1;
```

**How to reproduce:** Open a TypeScript file with `export const` variables. Check the
structure panel — the symbols should appear but the extraction logic is fragile.

---

### H2. rebuildLineStates() off-by-one: pos <= bufLen — ✅ Fixed (2026-03-16)

**File:** `src/editor/editor.cpp`, line 84

**Actual behavior:** The loop condition is `while (pos <= bufLen)`. When `pos == bufLen`,
`getLineText(pos, ...)` is called with `lineStart == bufLen`. Inside `getLineText`, the
condition `pos < bufLen` is immediately false, so it returns length 0. This is mostly harmless
but adds an unnecessary empty-line state to `lineStates`. The real risk is if `bufLen` changes
between the check and the call (unlikely in single-threaded code but still wrong).

**Expected behavior:** The loop condition should be `while (pos < bufLen)` to avoid processing
past the end of the buffer. The final empty line (if file doesn't end with newline) is already
handled by the state from the previous line.

**Fix:** Change `while (pos <= bufLen)` to `while (pos < bufLen)`.

---

### H3. Bracket matching can freeze the editor — ✅ Fixed (2026-03-16)

**File:** `src/editor/editor.cpp`, lines 343-359, `matchBracket()` method

**Actual behavior:** `matchBracket()` uses `while (true)` to scan the entire buffer for a
matching bracket. For a large file (e.g., 10MB) with an unmatched opening bracket at the start,
this scans millions of characters with no yield, freezing the UI until the scan completes. There
is no iteration limit or timeout.

**Expected behavior:** The scan should have a maximum iteration count (e.g., 100,000 characters)
after which it gives up and returns without moving the cursor. This prevents UI freezes on large
files or intentionally malformed input.

**Fix:** Add a counter inside the loop. If it exceeds a threshold (e.g., `bufLen` or 100000),
return early.

---

### H4. flattenNode() has no recursion depth limit — ✅ Fixed (2026-03-16)

**File:** `src/panels/filetree.cpp`, lines 179-188, `flattenNode()` method

**Actual behavior:** `flattenNode()` recursively descends into expanded directories with no
depth limit. If a filesystem contains deeply nested directories (hundreds of levels) or symlink
loops (e.g., `a/b/c -> a/`), this causes a stack overflow crash. The `loadChildren()` function
does skip hidden files (entries starting with `.`) but does not detect symlinks that create cycles.

**Expected behavior:** Add a maximum recursion depth constant (e.g., 64 levels). If depth
exceeds the limit, stop recursing and show a truncation indicator. Also consider detecting
symlink cycles by checking if the resolved path has already been visited in the current tree path.

**Fix:** Add a `maxDepth` parameter to `flattenNode()`. If `node.depth > MAX_TREE_DEPTH`,
return without recursing into children.

---

### H5. Structure parser matches keywords inside strings and comments — ✅ Fixed (2026-03-16)

**File:** `src/panels/structure.cpp`, all parser functions (`parsePhp`, `parseJavaScript`,
`parseCss`, `parseHtml`, etc.)

**Actual behavior:** All structure parsers use simple `ln.find("class ")`, `ln.find("function ")`
etc. on raw line text. This means a line like:
```php
$msg = "class Foo extends Bar";  // Not a real class
```
will be incorrectly detected as a class definition named "Foo" and shown in the structure panel.
Similarly, commented-out code like `// function oldHelper()` will appear as a function symbol.

**Expected behavior:** The parsers should skip lines (or portions of lines) that are inside
string literals or comments. A simple heuristic: skip lines where the keyword appears after a
`//` comment marker or inside quote characters. A more robust approach: use the syntax lexer's
tokenizer to determine if the keyword position is inside a string/comment token.

**Fix (minimal):** Before parsing a line, strip trailing `//` comments. Check if the keyword
position falls inside a quoted string by counting quote characters before it. This won't handle
all edge cases (heredocs, multi-line comments) but covers 90% of false positives.

---

## MEDIUM Issues

### M1. winCloseAll() unsafe iteration during deletion — ✅ Fixed (2026-03-16)

**File:** `src/app.cpp`, lines 482-494

**Actual behavior:** `winCloseAll()` iterates the desktop's circular linked list of views using
`v->prev()` to get the next pointer, then calls `w->close()` which removes the window from the
list. After deletion, `next` (obtained before close) may point to a deleted node if the closed
window was adjacent in the list. The termination check `next == deskTop->last` assumes `last`
hasn't changed, but closing a window updates `last`.

**Expected behavior:** Collect all editor windows into a separate list (e.g., `std::vector`)
first, then iterate that list to close each one. This avoids mutating the linked list while
iterating it.

**Fix:**
```cpp
void TVIDEApp::winCloseAll() {
    std::vector<TSyntaxEditWindow *> editors;
    // Collect all editor windows first
    TView *v = deskTop->first();
    if (v) {
        TView *t = v;
        do {
            auto *w = dynamic_cast<TSyntaxEditWindow *>(t);
            if (w) editors.push_back(w);
            t = t->next;
        } while (t != v);
    }
    // Close them all
    for (auto *w : editors) w->close();
}
```

---

### M2. winList() is a stub — ✅ Fixed (2026-03-16)

**File:** `src/app.cpp`, line 496

**Actual behavior:** The menu item "Window list..." (Alt-W) shows a message box saying
"Window list shows all open editor windows." instead of an actual window picker.

**Expected behavior:** Display a dialog with a TListBox showing all open editor windows by
title/filename. The user can select one and press Enter to switch to it, or press Escape to
cancel. This is a standard Turbo Vision IDE feature.

---

### M3. findInFiles() is a stub — ✅ Fixed (2026-03-16)

**File:** `src/app.cpp`, line 470

**Actual behavior:** The dialog for entering search text exists and opens, but pressing OK
only shows the message "Find in files not yet fully implemented." in the message panel. No
actual file searching occurs.

**Expected behavior:** After the user enters a search pattern and presses OK, recursively
search all files under the current project directory (or working directory). Display results
in the message panel as clickable entries showing `filename:line: matched text`. Clicking an
entry should open the file and navigate to that line.

---

### M4. gotoLine() may navigate to wrong position — ✅ Fixed (2026-03-16)

**File:** `src/app.cpp`, lines 454-464

**Actual behavior:** `gotoLine()` scans the gap buffer counting newlines to find the target
line position. It accesses the buffer using the gap-buffer pattern:
```cpp
char ch = (pos < (uint)e->curPtr) ? e->buffer[pos] : e->buffer[pos + e->gapLen];
```
This is correct for random access. However, the final `pos` value passed to `setCurPtr(pos, 0)`
is a logical position (pre-gap), which is what `setCurPtr` expects. The issue is that for very
long files, counting newlines character-by-character is slow. Additionally, there is no upper
bound check — if the user enters a line number larger than the file, `pos` will equal `bufLen`
and the cursor goes to the end of the file with no feedback that the line didn't exist.

**Expected behavior:** If the requested line exceeds the total line count, show a message or
navigate to the last line. Also consider using TEditor's built-in `lineMove()` for navigation
which is already optimized for the gap buffer.

---

### M5. lineStates cache invalidated on every keypress — ✅ Fixed (2026-03-16)

**File:** `src/editor/editor.cpp`, line 261

**Actual behavior:** Every `evKeyDown` event (including arrow keys, modifier keys, etc.)
clears the entire `lineStates` vector:
```cpp
if (event.what == evKeyDown)
    lineStates.clear();
```
On the next `draw()`, `rebuildLineStates()` re-tokenizes every line in the file from scratch.
For a 10,000-line file, this means re-tokenizing 10,000 lines on every keypress, causing
visible lag.

**Expected behavior:** Only invalidate line states from the current editing line onward. Or
better: only clear on text-modifying events (not arrow keys or modifier-only keys). Check if
the event actually changes text (not just navigation) before invalidating.

**Fix (minimal):** Check if the key is a text-modifying key before clearing:
```cpp
if (event.what == evKeyDown) {
    ushort kc = event.keyDown.keyCode;
    // Only invalidate on keys that modify text
    if (kc != kbUp && kc != kbDown && kc != kbLeft && kc != kbRight &&
        kc != kbHome && kc != kbEnd && kc != kbPgUp && kc != kbPgDn)
        lineStates.clear();
}
```

---

### M6. Dialog leak in newTheme() when validView fails — ✅ Fixed (2026-03-16)

**File:** `src/editor/editor.cpp`, line 616

**Actual behavior:** In `TSyntaxColorsDialog::newTheme()`:
```cpp
auto *d = new TDialog(...);
TView *p = TProgram::application->validView(d);
if (!p) return;  // d is leaked here
```
`validView()` returns nullptr if the view is invalid or can't be displayed. In that case, the
allocated dialog `d` is never freed. Note: `validView()` does NOT take ownership of the pointer
on failure — it only takes ownership on success (where it returns the same pointer).

**Expected behavior:** If `validView()` returns nullptr, delete the dialog:
```cpp
if (!p) { TObject::destroy(d); return; }
```

---

### M7. Substr without bounds check in structure parsers — ✅ Fixed (2026-03-16)

**File:** `src/panels/structure.cpp`, multiple parser functions

**Actual behavior:** Several parsers call `ln.substr(s, e - s)` where `s` and `e` are computed
from `find()` results without fully validating that `s < e` and `s < ln.size()`. For example,
in the PHP interface/trait/enum parsing, if the keyword is found at the end of the line with no
name following it:
```php
interface
```
Then `s` points past the end of the line, and `substr(s, e - s)` with `e == s` produces an
empty string (harmless), but if `s > ln.size()`, it throws `std::out_of_range`.

**Expected behavior:** Always validate `s < ln.size()` and `e > s` before calling `substr`.

---

### M8. Theme config parsing has no error checking — ✅ Fixed (2026-03-16)

**File:** `src/syntax/lexer.cpp`, `ThemeManager::load()` method

**Actual behavior:** When loading themes from `~/.config/tvide/themes.conf`, color values are
parsed from hex strings using sscanf or similar. If the config file is malformed (manually edited
with typos, corrupt, or from a different version), parsing produces garbage values silently. No
error is reported to the user.

**Expected behavior:** Validate parsed values are in the expected range (0x00-0xFF for BIOS
colors). If parsing fails, skip the malformed theme and log a warning. Fall back to built-in
defaults for any theme that can't be loaded.

---

### M9. No Ctrl+/ for toggling line comments — ✅ Fixed (2026-03-16)

**Actual behavior:** Pressing Ctrl+/ does nothing. There is no comment toggle feature.

**Expected behavior:** Ctrl+/ should toggle line comments for the current line (or selection).
The comment prefix depends on the file type: `//` for PHP/JS/TS/CSS, `<!-- -->` for HTML/XML,
`#` for YAML/Shell, `--` for SQL. If the line already starts with the comment prefix (ignoring
leading whitespace), remove it. Otherwise, add it.

---

### M10. No external file change detection — ✅ Fixed (2026-03-16)

**Actual behavior:** If a file is modified by another program (e.g., `git checkout`, another
editor), TVIDE continues to show the old content. Saving overwrites the external changes without
warning.

**Expected behavior:** On window focus or at regular intervals (idle), check the file's
modification timestamp against the last known value. If it changed, prompt the user: "File has
been modified externally. Reload?" with Yes/No options.

---

### M11. No visual bracket highlighting — ✅ Fixed (2026-03-16)

**File:** `src/editor/editor.cpp`, lines 324-360

**Actual behavior:** The `matchBracket()` function exists but only jumps the cursor to the
matching bracket. There is no visual indication of bracket pairs during normal editing.

**Expected behavior:** When the cursor is on or adjacent to a bracket character (`()[]{}`)
the matching bracket should be highlighted with a distinct color (e.g., bright cyan background)
in the `draw()` method. This is a standard IDE feature that helps users see code structure.
The jump-to-bracket feature (e.g., on Ctrl+]) should remain separate.

---

## LOW Issues

### L1. No error feedback when opendir() fails — ✅ Fixed (2026-03-16)

**File:** `src/panels/filetree.cpp`, line 143

**Actual behavior:** If `opendir()` fails (permission denied, broken symlink, path doesn't
exist), `loadChildren()` silently returns. The directory appears as expandable but shows no
children with no explanation.

**Expected behavior:** Show a message in the tree like "⛔ Permission denied" or add a message
to the message panel explaining why the directory couldn't be opened.

---

### L2. Hard-coded dialog sizes don't adapt to terminal size — ✅ Fixed (2026-03-16)

**File:** `src/editor/editor.cpp`, line 472 (colors dialog 72×27), line 366 (options dialog
42×18)

**Actual behavior:** Dialog dimensions are fixed constants. On terminals smaller than 72×27,
the colors dialog extends beyond the screen and is unusable.

**Expected behavior:** Dialogs should check `TProgram::deskTop->size` and either scale down
or show a "terminal too small" error. At minimum, cap dialog size to `min(desired, screen)`.

---

### L3. Project open dialog may not extract input correctly — ✅ Fixed (2026-03-16)

**File:** `src/app.cpp`, line 698

**Actual behavior:** `execDialog(d, dirBuf)` passes a raw char buffer to a dialog containing a
TInputLine. TInputLine's `getData`/`setData` use a length-prefixed format: first byte is string
length, followed by the characters. The `dirBuf` is initialized from `getcwd()` using `strncpy`
which doesn't add the length prefix.

**Expected behavior:** The buffer passed to `execDialog` for a TInputLine should have the
length prefix format, or use `setData`/`getData` correctly. Test project opening from the dialog
to verify it works end to end.

---

### L4. No cursor position in status bar — ⏸️ WONTFIX — TIndicator already shows line:col

**Actual behavior:** The status bar shows only keyboard shortcut hints (Alt-X Exit, F2 Save,
etc.). There is no display of current cursor position.

**Expected behavior:** The status bar (or a separate indicator) should show: `Line:Col`,
language name (e.g., "PHP"), and optionally the file's modified state. TIndicator already shows
some of this inside the editor window, but a global status bar display would be more visible.

---

### L5. Line number gutter width is fixed — ✅ Fixed (2026-03-16)

**Actual behavior:** The gutter is always 5 columns wide regardless of the file's total line
count.

**Expected behavior:** The gutter width should adapt: 4 columns for files up to 999 lines,
5 for up to 9999, 6 for up to 99999, etc. The `gutterWidth()` method should calculate this
based on `getTotalLines()`. When line count changes (editing), the gutter and editor should
be relaid out.

---

### L6. useTabs setting may not be effective — ✅ Fixed (2026-03-16)

**Actual behavior:** `EditorSettings::useTabs` exists and can be toggled in the options dialog,
but `TSyntaxEditor` inherits from `TFileEditor` which has its own tab handling. The setting may
not actually change whether pressing Tab inserts a tab character or spaces.

**Expected behavior:** When `useTabs` is false, pressing Tab should insert `tabSize` space
characters instead of a `\t`. This requires intercepting the Tab key in
`TSyntaxEditor::handleEvent` and inserting spaces manually.

---

### L7. No line wrapping option — ⏸️ DEFERRED — too complex for base TEditor override

**Actual behavior:** Long lines extend beyond the right edge of the editor. The only way to see
them is horizontal scrolling.

**Expected behavior:** Provide a toggle for soft word-wrap in the options dialog. When enabled,
lines that exceed the editor width should wrap visually to the next screen row. The underlying
buffer remains unchanged (no hard wraps inserted).

---

### L8. No recent files list — ⏸️ OPEN — not yet implemented

**Actual behavior:** There is no File → Recent Files submenu. Every time the user opens TVIDE,
they must navigate to files via File → Open or the file tree.

**Expected behavior:** Maintain a list of the last 10 opened files in a config file
(`~/.config/tvide/recent.conf`). Show them in a submenu under File → Recent Files. Clicking an
entry opens that file directly.

---

### L9. About dialog doesn't list all supported languages — ✅ Fixed (2026-03-16)

**File:** `src/dialogs/about.cpp`, line 19

**Actual behavior:** The About dialog says "JS, TS, Vue, JSON, YAML, SQL" but omits Markdown
and XML, which are both fully supported with lexers.

**Expected behavior:** Update the text to include all supported languages:
"PHP, HTML, CSS, JS, TS, Vue, JSON, YAML, SQL, Markdown, XML"

---

### L10. No Ctrl+D (duplicate line) — ✅ Fixed (2026-03-16)

**Actual behavior:** Pressing Ctrl+D does nothing.

**Expected behavior:** Ctrl+D should duplicate the current line (or the current selection if
multi-line). Insert the duplicated text below the current line and move the cursor to the new
copy.

---

### L11. No Ctrl+Shift+K (delete line) — ✅ Fixed (2026-03-16)

**Actual behavior:** Pressing Ctrl+Shift+K does nothing.

**Expected behavior:** Ctrl+Shift+K should delete the entire current line (including the
newline character) and move the cursor to the start of the next line.

---

### L12. No auto-closing brackets and quotes — ✅ Fixed (2026-03-16)

**Actual behavior:** Typing `(` inserts only `(`. The user must manually type `)`.

**Expected behavior:** When the user types an opening bracket or quote (`(`, `[`, `{`, `"`,
`'`, `` ` ``), automatically insert the matching closing character and place the cursor between
them. If the user types the closing character and it already exists at the cursor position, just
move the cursor past it (don't insert a duplicate). This should be toggleable in the options
dialog.

---

### L13. Static lastStructEditor pointer is a code smell — ✅ Fixed (2026-03-16)

**File:** `src/app.cpp`, line 759

**Actual behavior:** `static TSyntaxEditor *lastStructEditor` in `idle()` holds a raw pointer
used only for comparison to detect when the active editor changes. It is never dereferenced
after the editor is freed (because `lastEditorWindow` validation ensures `cur` becomes nullptr
first), so it's currently safe.

**Expected behavior:** This is fragile — if the validation logic changes, it becomes a
dangling pointer bug. Consider resetting `lastStructEditor` to nullptr when editors close, or
making it a member variable of TVIDEApp that gets properly managed.

---

### L14. About dialog may leak on null return — ✅ Fixed (2026-03-16)

**File:** `src/app.cpp`, line 714

**Actual behavior:** `helpAbout()` passes `createAboutDialog()` directly to `execView`, then
calls `TObject::destroy(d)`. If `createAboutDialog()` returns nullptr (unlikely but possible if
out of memory), `execView(nullptr)` and `destroy(nullptr)` are called. `destroy(nullptr)` is
safe (it checks for null), but `execView(nullptr)` behavior is undefined.

**Expected behavior:** Check the return value of `createAboutDialog()` before passing to
`execView`.
