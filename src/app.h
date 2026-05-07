#ifndef TVIDE_APP_H
#define TVIDE_APP_H

#define Uses_TKeys
#define Uses_TApplication
#define Uses_TEvent
#define Uses_TRect
#define Uses_TDialog
#define Uses_TStaticText
#define Uses_TButton
#define Uses_TMenuBar
#define Uses_TSubMenu
#define Uses_TMenuItem
#define Uses_TStatusLine
#define Uses_TStatusItem
#define Uses_TStatusDef
#define Uses_TDeskTop
#define Uses_TEditWindow
#define Uses_TEditor
#define Uses_TFileEditor
#define Uses_TFileDialog
#define Uses_TChDirDialog
#define Uses_TInputLine
#define Uses_TLabel
#define Uses_THistory
#define Uses_TCheckBoxes
#define Uses_TRadioButtons
#define Uses_MsgBox
#define Uses_TSItem
#define Uses_TView
#define Uses_TWindow
#define Uses_TScrollBar
#define Uses_TScroller
#define Uses_TListBox
#define Uses_TCollection
#define Uses_TSortedListBox
#define Uses_TStringCollection
#define Uses_TPoint
#define Uses_TScreen
#define Uses_TColorAttr
#define Uses_TAttrPair
#define Uses_TColorDialog
#define Uses_TGroup
#define Uses_TIndicator
#define Uses_TDrawBuffer
#define Uses_TTerminal
#define Uses_TTextDevice
#include <tvision/tv.h>

#include <string>
#include <vector>
#include <memory>

// Forward declarations
class TSyntaxEditor;
class TSyntaxEditWindow;
class TFileTreePanel;
class TMessagePanel;
class TStructurePanel;
class TWindowListPanel;
class TTerminalWindow;

// Custom command IDs (avoid conflicts with tvision built-in IDs)
const ushort cmOpenFile      = 2001;
const ushort cmFileNew       = 2002;
const ushort cmFileSave      = 2003;
const ushort cmFileSaveAs    = 2004;
const ushort cmFileClose     = 2005;
const ushort cmChangeDrct    = 2006;

const ushort cmEditSelectAll = 2015;

const ushort cmSearchFind    = 2020;
const ushort cmSearchReplace = 2021;
const ushort cmSearchAgn     = 2022;
const ushort cmGotoLine      = 2023;
const ushort cmFindInFiles   = 2024;
const ushort cmFindSymbol    = 2025;

const ushort cmWinCloseAll   = 2032;
const ushort cmWinList       = 2035;

const ushort cmToolTerminal  = 2040;
const ushort cmToolOptions   = 2041;
const ushort cmToolFileTree  = 2042;
const ushort cmToolMessages  = 2043;
const ushort cmToolColors    = 2044;
const ushort cmToolStructure = 2045;
const ushort cmToolWinList   = 2046; // window list tool panel (persistent)
const ushort cmFileNewTerm   = 2047; // new terminal window
const ushort cmSortNameAsc   = 2080;
const ushort cmSortNameDesc  = 2081;
const ushort cmSortDateAsc   = 2082;
const ushort cmSortDateDesc  = 2083;
const ushort cmSortDirsFirst = 2084; // toggle dirs-first vs mixed

// Terminal infra (consumed by tvterm::BasicTerminalWindow)
const ushort cmCheckTerminalUpdates = 2200;
const ushort cmTerminalUpdated      = 2201;
const ushort cmGrabInput            = 2202;
const ushort cmReleaseInput         = 2203;
const ushort hcInputGrabbed         = 2210;

const ushort cmHelpAbout     = 2050;

const ushort cmProjectOpen  = 2060;
const ushort cmProjectClose = 2061;

const ushort cmFileTreeOpen = 2070; // sent by file tree with infoPtr=filename
const ushort cmRelayout     = 2071; // trigger layout recalculation
const ushort cmWinListSelect = 2072; // sent by win-list panel; infoPtr = TWindow*
const ushort cmWinListClose  = 2073; // sent by win-list panel; infoPtr = TWindow*
const ushort cmWinListChanged = 2074; // broadcast: refresh win-list panels

// Application class
class TVIDEApp : public TApplication {
public:
    TVIDEApp(int argc, char **argv);
    virtual ~TVIDEApp();

    virtual void handleEvent(TEvent &event);
    static TMenuBar *initMenuBar(TRect r);
    static TStatusLine *initStatusLine(TRect r);
    virtual void outOfMemory();
    virtual void idle();
    virtual void changeBounds(const TRect &bounds);

    void relayout(); // reposition docked panels and resize desktop

private:
    void fileOpen();
    void fileNew();
    void fileSave();
    void fileSaveAs();
    void fileClose();
    void changeDir();
    void dosShell();

    void editSelectAll();

    void searchFind();
    void searchReplace();
    void gotoLine();
    void findInFiles();
    void findSymbol();

    void winCloseAll();
    void winList();

    void toolTerminal();
    void toolOptions();
    void toolColors();
    void toolFileTree();
    void toolMessages();
    void toolStructure();
    void toolWinList();
    void fileNewTerminal();

    void projectOpen();
    void projectOpen(const std::string &path);
    void projectClose();

    void helpAbout();

    TSyntaxEditWindow *openEditor(const char *fileName, Boolean visible);
    TSyntaxEditWindow *getActiveEditor();

    int windowCount;
    int terminalCount;
    TFileTreePanel *fileTreePanel;
    TMessagePanel *messagePanel;
    TStructurePanel *structurePanel;
    TWindowListPanel *winListPanel;
    TSyntaxEditWindow *lastEditorWindow;  // last focused editor (for structure panel)
    TSyntaxEditor *lastStructEditor;       // L13: tracks last editor used for structure panel
    int fileTreeWidth;     // persisted width for file tree panel
    int structureWidth;    // persisted width for structure panel
    int winListWidth;      // persisted width for window list panel
};

// Dialog helpers
ushort execDialog(TDialog *d, void *data);
TDialog *createFindDialog();
TDialog *createReplaceDialog();
ushort doEditDialog(int dialog, ...);

#endif // TVIDE_APP_H
