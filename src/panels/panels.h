#ifndef TVIDE_PANELS_H
#define TVIDE_PANELS_H

#include "app.h"
#include <string>
#include <vector>

// Forward declarations
class TSyntaxEditor;

// Non-sorted string collection (preserves insertion order)
class TUnsortedStringCollection : public TCollection {
public:
    TUnsortedStringCollection(ccIndex aLimit, ccIndex aDelta) noexcept
        : TCollection(aLimit, aDelta) {}
    virtual void freeItem(void *item) override { delete[] (char *)item; }
private:
    // Required by TCollection but we never stream
    virtual void *readItem(ipstream &) override { return nullptr; }
    virtual void writeItem(void *, opstream &) override {}
};

// File-tree sort mode
enum class FileSortMode {
    NameAsc,
    NameDesc,
    DateAsc,
    DateDesc,
};

// Tree node for file browser
struct FileNode {
    std::string name;
    std::string fullPath;
    bool isDir = false;
    bool expanded = false;
    bool childrenLoaded = false;
    int depth = 0;
    long mtime = 0; // last-modified time (seconds since epoch)
    std::vector<FileNode> children;
};

// Forward declaration
class TFileTreePanel;

// Custom list box that colors directories in blue
class TFileTreeListBox : public TListBox {
public:
    TFileTreeListBox(const TRect &bounds, ushort aNumCols, TScrollBar *aScrollBar,
                     std::vector<FileNode *> &flatListRef);
    virtual void draw() override;
private:
    std::vector<FileNode *> &flatList;
};

// File tree panel (left sidebar — docked)
class TFileTreePanel : public TWindow {
public:
    TFileTreePanel(const TRect &bounds);
    virtual ~TFileTreePanel();

    virtual void handleEvent(TEvent &event) override;
    virtual void changeBounds(const TRect &bounds) override;
    virtual void sizeLimits(TPoint &min, TPoint &max) override;
    virtual TPalette &getPalette() const override;
    void refreshDir(const std::string &path);
    const std::string &getCurrentDir() const { return rootPath; }

    void setSortMode(FileSortMode mode);
    FileSortMode getSortMode() const { return sortMode; }
    void cycleSortMode();

    void setDirsFirst(bool on);
    bool getDirsFirst() const { return dirsFirst; }
    void toggleDirsFirst();

private:
    TFileTreeListBox *listBox;
    TScrollBar *scrollBar;
    TUnsortedStringCollection *fileList;
    std::string rootPath;
    FileNode root;
    FileSortMode sortMode = FileSortMode::NameAsc;
    bool dirsFirst = true;
    std::vector<FileNode *> flatList; // parallel to listBox items
    void resortAll(FileNode &node);

    void loadChildren(FileNode &node);
    void rebuildFlatList();
    void flattenNode(FileNode &node);
    void toggleOrOpen();
    static const char *fileIcon(const std::string &name, bool isDir, bool expanded);
};

// Message/output panel (bottom)
class TMessagePanel : public TWindow {
public:
    TMessagePanel(const TRect &bounds);
    virtual ~TMessagePanel();

    void addMessage(const std::string &msg);
    void clear();

    virtual void handleEvent(TEvent &event) override;

private:
    TListBox *listBox;
    TScrollBar *scrollBar;
    TUnsortedStringCollection *messages;
};

// Sort mode for the structure panel
enum class SymbolSortMode {
    LineAsc,    // source order (default)
    LineDesc,
    NameAsc,
    NameDesc,
    KindAsc,    // group by kind, then by line
    KindDesc,
};

// Symbol kinds for the structure panel
enum class SymbolKind {
    Class, Interface, Trait, Enum,
    Function, Method, Constructor,
    Property, Variable, Constant,
    Namespace,
    Heading,   // Markdown headings
    Tag,       // HTML tags
    Selector,  // CSS selectors
    Key,       // JSON/YAML keys
};

// A parsed symbol from the source code
struct SymbolInfo {
    SymbolKind  kind;
    std::string name;
    int         line;   // 1-based line number
    int         depth;  // nesting depth (for indentation)
};

// Parse symbols from the current editor
void parseStructure(TSyntaxEditor *editor, std::vector<SymbolInfo> &syms);

// Parse symbols from raw text + language name (e.g. "PHP", "JavaScript", ...)
void parseStructureText(const char *languageName, const std::string &text,
                        std::vector<SymbolInfo> &syms);

// Symbol-kind name as a short string (e.g. "class", "function") for display.
const char *symbolKindName(SymbolKind k);

// ── Window list tool panel (left-docked) ───────────────────────────────
// Lists open windows on the desktop (editors + terminals). Enter / dbl-click focuses.

class TWindowListPanel : public TWindow {
public:
    TWindowListPanel(const TRect &bounds);
    virtual ~TWindowListPanel();

    virtual void handleEvent(TEvent &event) override;
    virtual void changeBounds(const TRect &bounds) override;
    virtual void sizeLimits(TPoint &min, TPoint &max) override;
    virtual TPalette &getPalette() const override;

    void refresh(); // rebuild from current desktop state

private:
    TListBox *listBox;
    TScrollBar *scrollBar;
    TUnsortedStringCollection *items;
    std::vector<TWindow *> windows; // parallel to list items

    void activateFocused();
    void closeFocused();
};

// ── Terminal window (libvterm-backed, lives on desktop like an editor) ──
namespace tvterm {
    class TerminalController;
    struct TVTermConstants;
}
#include <tvterm/termwnd.h>

class TTerminalWindow : public tvterm::BasicTerminalWindow {
public:
    static const tvterm::TVTermConstants appConsts;

    TTerminalWindow(const TRect &bounds, tvterm::TerminalController &term,
                    int aNumber, const char *aTitle = nullptr) noexcept;

    virtual void handleEvent(TEvent &ev) override;
    virtual void sizeLimits(TPoint &min, TPoint &max) override;
    virtual const char *getTitle(short maxSize) override;

    static tvterm::TerminalController *createController(TPoint size);

private:
    using Super = tvterm::BasicTerminalWindow;
    tvterm::TerminalController &termCtrl;
    int windowNumber;
    std::string customTitle;
};

// Custom list box with blue focused item
class TStructureListBox : public TListBox {
public:
    TStructureListBox(const TRect &bounds, ushort aNumCols, TScrollBar *aScrollBar);
    virtual void draw() override;
};

// Structure panel (right sidebar — docked) — shows classes, functions, etc.
class TStructurePanel : public TWindow {
public:
    TStructurePanel(const TRect &bounds);
    virtual ~TStructurePanel();

    virtual void handleEvent(TEvent &event) override;
    virtual void changeBounds(const TRect &bounds) override;
    virtual void sizeLimits(TPoint &min, TPoint &max) override;
    virtual TPalette &getPalette() const override;

    // Refresh the symbol list from the given editor
    void refresh(TSyntaxEditor *editor);

    void setSortMode(SymbolSortMode mode);
    SymbolSortMode getSortMode() const { return sortMode; }

private:
    TStructureListBox *listBox;
    TScrollBar *scrollBar;
    TUnsortedStringCollection *items;
    std::vector<SymbolInfo> symbols; // parallel to list items
    TSyntaxEditor *trackedEditor;
    SymbolSortMode sortMode = SymbolSortMode::LineAsc;

    void navigateToSymbol();
    void rebuildList();
};

#endif // TVIDE_PANELS_H
