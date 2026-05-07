#include "panels.h"
#include "app.h"
#include <dirent.h>
#include <sys/stat.h>
#include <climits>
#include <cstring>
#include <algorithm>

static const int MAX_TREE_DEPTH = 64;

// ── TFileTreeListBox (colors directories in blue) ───────────────────────

TFileTreeListBox::TFileTreeListBox(const TRect &bounds, ushort aNumCols,
                                   TScrollBar *aScrollBar,
                                   std::vector<FileNode *> &flatListRef)
    : TListBox(bounds, aNumCols, aScrollBar),
      flatList(flatListRef)
{
}

void TFileTreeListBox::draw()
{
    TColorAttr normalColor, focusedColor, selectedColor, color;
    short colWidth, curCol, indent;
    TDrawBuffer b;

    // Dark blue on light gray for directories
    const TColorAttr dirColor = 0x71;

    if ((state & (sfSelected | sfActive)) == (sfSelected | sfActive)) {
        normalColor = getColor(1);
        focusedColor = getColor(3);
        selectedColor = getColor(4);
    } else {
        normalColor = getColor(2);
        selectedColor = getColor(4);
        focusedColor = normalColor;
    }

    if (hScrollBar != 0)
        indent = hScrollBar->value;
    else
        indent = 0;

    colWidth = size.x / numCols + 1;
    for (short i = 0; i < size.y; i++) {
        for (short j = 0; j < numCols; j++) {
            short item = j * size.y + i + topItem;
            curCol = j * colWidth;

            bool isFocused = false;
            if ((state & (sfSelected | sfActive)) == (sfSelected | sfActive) &&
                focused == item && range > 0) {
                color = focusedColor;
                setCursor(curCol + 1, i);
                isFocused = true;
            } else if (item < range && isSelected(item)) {
                color = selectedColor;
            } else {
                color = normalColor;
            }

            // Expanded directories shown in blue
            bool isExpandedDir = (item >= 0 && item < (short)flatList.size() &&
                          flatList[item] && flatList[item]->isDir &&
                          flatList[item]->expanded);
            if (isExpandedDir && !isFocused)
                color = dirColor;

            b.moveChar(curCol, ' ', color, colWidth);
            if (item < range) {
                if (indent < 255) {
                    char text[256];
                    getText(text, item, 255);
                    b.moveStr(curCol + 1, text, color, colWidth, indent);
                }
            }

            b.moveChar(curCol + colWidth - 1, '\xB3', getColor(5), 1);
        }
        writeLine(0, i, size.x, 1, b);
    }
}

// ── File icon helper ────────────────────────────────────────────────────

const char *TFileTreePanel::fileIcon(const std::string &name, bool isDir, bool expanded)
{
    if (isDir)
        return expanded ? "📂 " : "📁 ";

    // Match by extension
    auto dot = name.rfind('.');
    if (dot != std::string::npos) {
        std::string ext = name.substr(dot + 1);
        // Lowercase for comparison
        for (auto &c : ext) c = tolower(c);

        if (ext == "php")                        return "🐘 ";
        if (ext == "js" || ext == "mjs")         return "🟨 ";
        if (ext == "ts" || ext == "tsx")          return "🔷 ";
        if (ext == "vue")                        return "💚 ";
        if (ext == "html" || ext == "htm")       return "🌐 ";
        if (ext == "css" || ext == "scss" || ext == "less") return "🎨 ";
        if (ext == "json")                       return "📋 ";
        if (ext == "yaml" || ext == "yml")       return "📋 ";
        if (ext == "xml")                        return "📰 ";
        if (ext == "md" || ext == "markdown")    return "📝 ";
        if (ext == "sql")                        return "🗃 ";
        if (ext == "sh" || ext == "bash")        return "⚙ ";
        if (ext == "py")                         return "🐍 ";
        if (ext == "rb")                         return "💎 ";
        if (ext == "c" || ext == "cpp" || ext == "h" || ext == "hpp") return "⚙ ";
        if (ext == "java")                       return "☕ ";
        if (ext == "go")                         return "🔵 ";
        if (ext == "rs")                         return "🦀 ";
        if (ext == "lock")                       return "🔒 ";
        if (ext == "log")                        return "📜 ";
        if (ext == "env")                        return "🔑 ";
        if (ext == "png" || ext == "jpg" || ext == "jpeg" ||
            ext == "gif" || ext == "svg" || ext == "ico" ||
            ext == "webp")                       return "🖼 ";
        if (ext == "zip" || ext == "tar" || ext == "gz" ||
            ext == "bz2" || ext == "xz")         return "📦 ";
        if (ext == "txt")                        return "📄 ";
    }

    // Special filenames
    if (name == "Makefile" || name == "CMakeLists.txt") return "⚙ ";
    if (name == "Dockerfile" || name == ".dockerignore") return "🐳 ";
    if (name == ".gitignore" || name == ".gitmodules")  return "📌 ";
    if (name == "README" || name == "LICENSE")          return "📝 ";

    return "📄 ";
}

// ── Tree management ─────────────────────────────────────────────────────

void TFileTreePanel::loadChildren(FileNode &node)
{
    if (node.childrenLoaded) return;
    node.childrenLoaded = true;
    node.children.clear();

    DIR *dir = opendir(node.fullPath.c_str());
    if (!dir) {
        // L1: show error indicator for failed directories
        FileNode errNode;
        errNode.name = "⛔ Permission denied";
        errNode.fullPath = node.fullPath;
        errNode.isDir = false;
        errNode.depth = node.depth + 1;
        node.children.push_back(std::move(errNode));
        return;
    }

    std::vector<FileNode> dirs, files;
    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_name[0] == '.' &&
            (ent->d_name[1] == '\0' ||
             (ent->d_name[1] == '.' && ent->d_name[2] == '\0')))
            continue; // skip . and ..
        std::string name = ent->d_name;
        std::string full = node.fullPath + "/" + name;
        struct stat st;
        bool isDir = false;
        if (stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            isDir = true;

        FileNode child;
        child.name = name;
        child.fullPath = full;
        child.isDir = isDir;
        child.expanded = false;
        child.childrenLoaded = false;
        child.depth = node.depth + 1;
        child.mtime = (long)st.st_mtime;

        if (isDir) dirs.push_back(std::move(child));
        else       files.push_back(std::move(child));
    }
    closedir(dir);

    auto cmp = [this](const FileNode &a, const FileNode &b) {
        switch (sortMode) {
        case FileSortMode::NameAsc:  return a.name < b.name;
        case FileSortMode::NameDesc: return a.name > b.name;
        case FileSortMode::DateAsc:
            return a.mtime != b.mtime ? a.mtime < b.mtime : a.name < b.name;
        case FileSortMode::DateDesc:
            return a.mtime != b.mtime ? a.mtime > b.mtime : a.name < b.name;
        }
        return a.name < b.name;
    };
    if (dirsFirst) {
        std::sort(dirs.begin(), dirs.end(), cmp);
        std::sort(files.begin(), files.end(), cmp);
        for (auto &d : dirs) node.children.push_back(std::move(d));
        for (auto &f : files) node.children.push_back(std::move(f));
    } else {
        std::vector<FileNode> mixed;
        mixed.reserve(dirs.size() + files.size());
        for (auto &d : dirs) mixed.push_back(std::move(d));
        for (auto &f : files) mixed.push_back(std::move(f));
        std::sort(mixed.begin(), mixed.end(), cmp);
        for (auto &m : mixed) node.children.push_back(std::move(m));
    }
}

void TFileTreePanel::resortAll(FileNode &node)
{
    auto cmp = [this](const FileNode &a, const FileNode &b) {
        switch (sortMode) {
        case FileSortMode::NameAsc:  return a.name < b.name;
        case FileSortMode::NameDesc: return a.name > b.name;
        case FileSortMode::DateAsc:
            return a.mtime != b.mtime ? a.mtime < b.mtime : a.name < b.name;
        case FileSortMode::DateDesc:
            return a.mtime != b.mtime ? a.mtime > b.mtime : a.name < b.name;
        }
        return a.name < b.name;
    };
    if (dirsFirst) {
        std::vector<FileNode> dirs, files;
        for (auto &c : node.children) {
            if (c.isDir) dirs.push_back(std::move(c));
            else         files.push_back(std::move(c));
        }
        std::sort(dirs.begin(), dirs.end(), cmp);
        std::sort(files.begin(), files.end(), cmp);
        node.children.clear();
        for (auto &d : dirs) node.children.push_back(std::move(d));
        for (auto &f : files) node.children.push_back(std::move(f));
    } else {
        std::sort(node.children.begin(), node.children.end(), cmp);
    }
    for (auto &c : node.children)
        if (c.isDir && c.childrenLoaded) resortAll(c);
}

void TFileTreePanel::setDirsFirst(bool on)
{
    if (dirsFirst == on) return;
    dirsFirst = on;
    resortAll(root);
    rebuildFlatList();
}

void TFileTreePanel::toggleDirsFirst()
{
    setDirsFirst(!dirsFirst);
}

void TFileTreePanel::setSortMode(FileSortMode mode)
{
    if (sortMode == mode) return;
    sortMode = mode;
    resortAll(root);
    rebuildFlatList();
}

void TFileTreePanel::cycleSortMode()
{
    switch (sortMode) {
    case FileSortMode::NameAsc:  setSortMode(FileSortMode::NameDesc); break;
    case FileSortMode::NameDesc: setSortMode(FileSortMode::DateDesc); break;
    case FileSortMode::DateDesc: setSortMode(FileSortMode::DateAsc);  break;
    case FileSortMode::DateAsc:  setSortMode(FileSortMode::NameAsc);  break;
    }
}

void TFileTreePanel::flattenNode(FileNode &node)
{
    for (auto &child : node.children) {
        flatList.push_back(&child);
        if (child.isDir && child.expanded && child.depth < MAX_TREE_DEPTH) {
            loadChildren(child);
            flattenNode(child);
        }
    }
}

void TFileTreePanel::rebuildFlatList()
{
    flatList.clear();
    flattenNode(root);

    int focused = listBox->focused;
    auto *newItems = new TUnsortedStringCollection(
        (ccIndex)flatList.size() + 1, 10);

    for (auto *node : flatList) {
        std::string display;
        for (int i = 0; i < node->depth; i++)
            display += "  ";
        display += fileIcon(node->name, node->isDir, node->expanded);
        display += node->name;
        newItems->insert(newStr(display.c_str()));
    }

    listBox->newList(newItems);
    fileList = newItems;

    // Restore focus position
    if (focused >= 0 && focused < (int)flatList.size())
        listBox->focusItem(focused);

    drawView();
}

void TFileTreePanel::toggleOrOpen()
{
    int idx = listBox->focused;
    if (idx < 0 || idx >= (int)flatList.size())
        return;

    FileNode *node = flatList[idx];
    if (node->isDir) {
        // Toggle expand/collapse
        if (node->expanded) {
            node->expanded = false;
        } else {
            node->expanded = true;
            loadChildren(*node);
        }
        rebuildFlatList();
        // Keep focus on the toggled directory
        if (idx < (int)flatList.size())
            listBox->focusItem(idx);
    } else {
        // Open file — copy path to static buffer for safe message passing
        static char pathBuf[PATH_MAX];
        strncpy(pathBuf, node->fullPath.c_str(), PATH_MAX - 1);
        pathBuf[PATH_MAX - 1] = '\0';
        message(TProgram::application, evCommand, cmFileTreeOpen,
                (void *)pathBuf);
    }
}

// ── TFileTreePanel ──────────────────────────────────────────────────────

TFileTreePanel::TFileTreePanel(const TRect &bounds)
    : TWindowInit(&TFileTreePanel::initFrame),
      TWindow(bounds, "Files", 0),
      fileList(nullptr)
{
    flags = 0; // no move, zoom, close — docked panel
    growMode = 0;
    state &= ~sfShadow;
    options |= ofFirstClick;
    options &= ~ofTopSelect; // allow setCurrent so panel receives keyboard events
    eventMask |= evMouseWheel; // receive mouse wheel for scrolling

    TRect r = getExtent();
    r.grow(-1, -1);

    TRect sr = getExtent();
    sr.a.x = sr.b.x - 1;
    sr.a.y = 1;
    sr.b.y--;
    scrollBar = new TScrollBar(sr);
    scrollBar->growMode = gfGrowLoX | gfGrowHiX | gfGrowHiY;
    insert(scrollBar);

    r.b.x--;
    listBox = new TFileTreeListBox(r, 1, scrollBar, flatList);
    listBox->growMode = gfGrowHiX | gfGrowHiY;
    insert(listBox);

    fileList = new TUnsortedStringCollection(50, 10);
    listBox->newList(fileList);
}

TFileTreePanel::~TFileTreePanel()
{
    listBox->newList(nullptr);
    fileList = nullptr;
}

TPalette &TFileTreePanel::getPalette() const
{
    // Custom 32-entry palette based on cpGrayDialog
    // Entries 26-29 (TListViewer) remapped to light gray colors
    static char customPal[] =
        "\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F"
        "\x30\x31\x32\x33\x34\x35\x36\x37\x38"
        "\x20"  // 26: list normal (black on light gray)
        "\x06"  // 27: list focused (menu selected highlight)
        "\x21"  // 28: list selected (white on light gray)
        "\x20"  // 29: list divider
        "\x3D\x3E\x3F";
    static TPalette pal(customPal, sizeof(customPal) - 1);
    return pal;
}

void TFileTreePanel::refreshDir(const std::string &path)
{
    char resolved[PATH_MAX];
    if (realpath(path.c_str(), resolved))
        rootPath = resolved;
    else
        rootPath = path;

    root = FileNode();
    root.name = rootPath;
    root.fullPath = rootPath;
    root.isDir = true;
    root.expanded = true;
    root.childrenLoaded = false;
    root.depth = -1; // root itself is not shown

    loadChildren(root);
    rebuildFlatList();
}

void TFileTreePanel::handleEvent(TEvent &event)
{
    // Drag right border to resize
    if (event.what == evMouseDown) {
        TPoint mouse = makeLocal(event.mouse.where);
        if (mouse.x == size.x - 1) {
            TPoint lastMouse = event.mouse.where;
            while (mouseEvent(event, evMouseMove)) {
                int dx = event.mouse.where.x - lastMouse.x;
                if (dx != 0) {
                    int newWidth = size.x + dx;
                    if (newWidth >= 15 && newWidth <= 60) {
                        TRect r = getBounds();
                        r.b.x = r.a.x + newWidth;
                        changeBounds(r);
                    }
                    lastMouse = event.mouse.where;
                }
            }
            clearEvent(event);
            return;
        }
    }

    // Handle mouse wheel scrolling — only when cursor is over this panel
    if (event.what == evMouseWheel) {
        TPoint mouse = makeLocal(event.mouse.where);
        if (mouse.x < 0 || mouse.x >= size.x || mouse.y < 0 || mouse.y >= size.y)
            return; // not over us — let someone else handle it
        if (listBox->range <= 0) { clearEvent(event); return; }
        int delta = (event.mouse.wheel == mwUp) ? -3 : 3;
        int newFocus = std::max(0, std::min((int)listBox->range - 1,
                                             listBox->focused + delta));
        listBox->focusItem(newFocus);
        listBox->drawView();
        clearEvent(event);
        return;
    }

    // Handle keyboard navigation before TWindow dispatches to subviews
    if (event.what == evKeyDown) {
        // C1: guard all navigation against empty list
        if (listBox->range <= 0 && event.keyDown.keyCode != kbEnter) {
            switch (event.keyDown.keyCode) {
            case kbUp: case kbDown: case kbPgUp: case kbPgDn:
            case kbHome: case kbEnd: case kbLeft: case kbRight:
                clearEvent(event);
                return;
            }
        }
        switch (event.keyDown.keyCode) {
        case kbEnter:
            toggleOrOpen();
            clearEvent(event);
            return;
        }
        if (event.keyDown.charScan.charCode == 's' ||
            event.keyDown.charScan.charCode == 'S') {
            cycleSortMode();
            clearEvent(event);
            return;
        }
        if (event.keyDown.charScan.charCode == 'd' ||
            event.keyDown.charScan.charCode == 'D') {
            toggleDirsFirst();
            clearEvent(event);
            return;
        }
        switch (event.keyDown.keyCode) {
        case kbUp:
            if (listBox->focused > 0) {
                listBox->focusItem(listBox->focused - 1);
                listBox->drawView();
            }
            clearEvent(event);
            return;
        case kbDown:
            if (listBox->focused < listBox->range - 1) {
                listBox->focusItem(listBox->focused + 1);
                listBox->drawView();
            }
            clearEvent(event);
            return;
        case kbPgUp:
            listBox->focusItem(std::max(0, listBox->focused - listBox->size.y));
            listBox->drawView();
            clearEvent(event);
            return;
        case kbPgDn:
            listBox->focusItem(std::min((int)listBox->range - 1,
                               listBox->focused + listBox->size.y));
            listBox->drawView();
            clearEvent(event);
            return;
        case kbHome:
            listBox->focusItem(0);
            listBox->drawView();
            clearEvent(event);
            return;
        case kbEnd:
            listBox->focusItem(listBox->range - 1);
            listBox->drawView();
            clearEvent(event);
            return;
        case kbRight: {
            int idx = listBox->focused;
            if (idx >= 0 && idx < (int)flatList.size()) {
                FileNode *node = flatList[idx];
                if (node->isDir && !node->expanded) {
                    node->expanded = true;
                    loadChildren(*node);
                    rebuildFlatList();
                    listBox->focusItem(idx);
                }
            }
            clearEvent(event);
            return;
        }
        case kbLeft: {
            int idx = listBox->focused;
            if (idx >= 0 && idx < (int)flatList.size()) {
                FileNode *node = flatList[idx];
                if (node->isDir && node->expanded) {
                    node->expanded = false;
                    rebuildFlatList();
                    listBox->focusItem(idx);
                }
            }
            clearEvent(event);
            return;
        }
        }
    }

    TWindow::handleEvent(event);

    if (event.what == evBroadcast &&
        event.message.command == cmListItemSelected) {
        toggleOrOpen();
        clearEvent(event);
    }
}

void TFileTreePanel::changeBounds(const TRect &bounds)
{
    TWindow::changeBounds(bounds);
    message(TProgram::application, evBroadcast, cmRelayout, this);
}

void TFileTreePanel::sizeLimits(TPoint &min, TPoint &max)
{
    TWindow::sizeLimits(min, max);
    min.x = 15;
    max.x = 60;
}

// ── TMessagePanel ───────────────────────────────────────────────────────

TMessagePanel::TMessagePanel(const TRect &bounds)
    : TWindowInit(&TMessagePanel::initFrame),
      TWindow(bounds, "Messages", 0),
      messages(nullptr)
{
    options |= ofTileable;
    growMode = gfGrowAll;

    TRect r = getExtent();
    r.grow(-1, -1);

    TRect sr = getExtent();
    sr.a.x = sr.b.x - 1;
    sr.a.y = 1;
    sr.b.y--;
    scrollBar = new TScrollBar(sr);
    insert(scrollBar);

    r.b.x--;
    listBox = new TListBox(r, 1, scrollBar);
    insert(listBox);

    messages = new TUnsortedStringCollection(50, 10);
    listBox->newList(messages);
}

TMessagePanel::~TMessagePanel()
{
    listBox->newList(nullptr);
    messages = nullptr;
}

void TMessagePanel::addMessage(const std::string &msg)
{
    messages->insert(newStr(msg.c_str()));
    listBox->setRange(messages->getCount());
    listBox->focusItem(messages->getCount() - 1);
    listBox->drawView();
}

void TMessagePanel::clear()
{
    messages = new TUnsortedStringCollection(50, 10);
    listBox->newList(messages);
}

void TMessagePanel::handleEvent(TEvent &event)
{
    TWindow::handleEvent(event);
}
