#include "panels.h"
#include "app.h"
#include <dirent.h>
#include <sys/stat.h>
#include <climits>
#include <cstring>
#include <algorithm>

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
    if (!dir) return;

    std::vector<FileNode> dirs, files;
    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_name[0] == '.') continue; // skip hidden and . / ..
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

        if (isDir) dirs.push_back(std::move(child));
        else       files.push_back(std::move(child));
    }
    closedir(dir);

    std::sort(dirs.begin(), dirs.end(),
        [](const FileNode &a, const FileNode &b) { return a.name < b.name; });
    std::sort(files.begin(), files.end(),
        [](const FileNode &a, const FileNode &b) { return a.name < b.name; });

    for (auto &d : dirs) node.children.push_back(std::move(d));
    for (auto &f : files) node.children.push_back(std::move(f));
}

void TFileTreePanel::flattenNode(FileNode &node)
{
    for (auto &child : node.children) {
        flatList.push_back(&child);
        if (child.isDir && child.expanded) {
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
        // Open file
        message(TProgram::application, evCommand, cmFileTreeOpen,
                (void *)node->fullPath.c_str());
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
    listBox = new TListBox(r, 1, scrollBar);
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
    static TPalette pal(cpGrayDialog, sizeof(cpGrayDialog) - 1);
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

    TWindow::handleEvent(event);

    if (event.what == evKeyDown && event.keyDown.keyCode == kbEnter) {
        toggleOrOpen();
        clearEvent(event);
    }

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
