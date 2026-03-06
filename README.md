# TVIDE - Turbo Vision IDE

A modern text-mode PHP/web development IDE built with [magiblot/tvision](https://github.com/magiblot/tvision), inspired by the classic RHIDE IDE.

![License](https://img.shields.io/badge/license-MIT-blue.svg)

## Features

### Editor
- **Syntax highlighting** for PHP, HTML, CSS, JavaScript, TypeScript, Vue, JSON, Markdown, YAML, SQL, XML
- **Line numbers** gutter with automatic width
- **Whitespace visualization** — visible tabs (`»`), spaces (`·`), EOL markers (`¶`)
- Multi-document editing with tiled/cascaded windows
- Find & Replace with search history
- Go to Line dialog
- Find in Files
- Bracket matching
- Full undo support
- Configurable via Options dialog (Tools → Options)

### IDE
- RHIDE-style menu-driven interface
- File tree panel (sidebar) with directory navigation and `..` parent link
- Messages/output panel
- Terminal/shell access
- Window management (tile, cascade, next, previous, close all)
- Status bar with keyboard shortcuts
- File open dialog with type filters
- Change directory support

### Project Support
- **JetBrains-compatible projects** — opens `.idea` directory projects from IntelliJ, PhpStorm, WebStorm
- Parses `modules.xml`, `.name`, `vcs.xml`, and `.iml` files
- Recognizes source roots and excluded directories
- Auto-detects VCS (Git, SVN, etc.)

### Editor Options (Tools → Options)
- Toggle line numbers on/off
- Toggle syntax highlighting on/off
- Toggle whitespace visualization
- Toggle EOL markers
- Toggle auto indent
- Toggle bracket matching
- Tab size (1-16)
- Use tabs or spaces

### Keyboard Shortcuts
| Key | Action |
|-----|--------|
| F2 | Save |
| F3 | Open file |
| F4 | Search again |
| F5 | Zoom window |
| F6 | Next window |
| F10 | Menu |
| Ctrl-N | New file |
| Ctrl-W | Close file |
| Ctrl-F | Find |
| Ctrl-H | Replace |
| Ctrl-G | Go to line |
| Ctrl-A | Select all |
| Ctrl-Z | Undo |
| Alt-X | Exit |

## Supported Languages

| Extension | Language |
|-----------|----------|
| `.php`, `.phtml` | PHP |
| `.html`, `.htm` | HTML |
| `.css`, `.scss`, `.less` | CSS |
| `.js`, `.jsx`, `.mjs` | JavaScript |
| `.ts`, `.tsx` | TypeScript |
| `.vue` | Vue SFC |
| `.json` | JSON |
| `.md`, `.markdown` | Markdown |
| `.yml`, `.yaml` | YAML |
| `.sql` | SQL |
| `.xml`, `.svg`, `.xsl` | XML |
| `.twig`, `.blade`, `.latte` | Template (HTML mode) |

## Building

### Prerequisites
- C++17 compiler (GCC 7+ or Clang 5+)
- CMake 3.16+
- ncurses development library
- GPM development library (optional, Linux)

```bash
# Ubuntu/Debian
sudo apt install build-essential cmake libncurses-dev libgpm-dev

# Clone with submodules
git clone --recursive https://github.com/youruser/tvide.git
cd tvide

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run
./build/tvide [files...]
```

## Usage

```bash
# Open the IDE
./build/tvide

# Open specific files
./build/tvide src/index.php public/style.css

# Open a JetBrains project
# Menu: Project > Open project...
# Select any file in the project root, and TVIDE will find the .idea directory
```

## Architecture

```
src/
├── main.cpp              # Entry point
├── app.h / app.cpp       # Main application, menus, event handling
├── editor/
│   ├── editor.h          # TSyntaxEditor, TSyntaxEditWindow, TLineGutter, EditorSettings
│   ├── editor.cpp         # Syntax-highlighted editor (draw() override), options dialog
│   ├── editwindow.cpp     # Editor window with gutter layout
│   └── linenums.cpp       # Line number gutter view
├── syntax/
│   ├── lexer.h            # Base lexer, token types, color scheme
│   ├── lexer.cpp          # Lexer factory, color mapping
│   ├── php.cpp            # PHP lexer
│   ├── html.cpp           # HTML lexer
│   ├── css.cpp            # CSS lexer
│   ├── javascript.cpp     # JavaScript lexer
│   ├── typescript.cpp     # TypeScript lexer
│   ├── vue.cpp            # Vue SFC lexer
│   ├── json.cpp           # JSON lexer
│   ├── markdown.cpp       # Markdown lexer (headings, bold, italic, code, links, tables, etc.)
│   ├── yaml.cpp           # YAML lexer
│   ├── sql.cpp            # SQL lexer
│   ├── xml.cpp            # XML lexer
│   └── plaintext.cpp      # Plain text fallback
├── dialogs/
│   ├── dialogs.h          # Dialog declarations
│   ├── gotoline.cpp       # Go to Line dialog
│   ├── findinfiles.cpp    # Find in Files dialog
│   └── about.cpp          # About dialog
├── project/
│   ├── project.h          # ProjectManager, ProjectInfo
│   └── project.cpp        # JetBrains .idea project parser
└── panels/
    ├── panels.h           # Panel declarations
    ├── filetree.cpp       # File tree sidebar (unsorted, preserves order)
    ├── terminal.cpp       # Terminal support
    └── messages.cpp       # Message/output panel
```

## License

MIT License. Turbo Vision is used under its original license terms.
