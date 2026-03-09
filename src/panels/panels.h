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

// Tree node for file browser
struct FileNode {
    std::string name;
    std::string fullPath;
    bool isDir = false;
    bool expanded = false;
    bool childrenLoaded = false;
    int depth = 0;
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

private:
    TFileTreeListBox *listBox;
    TScrollBar *scrollBar;
    TUnsortedStringCollection *fileList;
    std::string rootPath;
    FileNode root;
    std::vector<FileNode *> flatList; // parallel to listBox items

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

private:
    TStructureListBox *listBox;
    TScrollBar *scrollBar;
    TUnsortedStringCollection *items;
    std::vector<SymbolInfo> symbols; // parallel to list items
    TSyntaxEditor *trackedEditor;

    void navigateToSymbol();
};

#endif // TVIDE_PANELS_H
