#include "app.h"
#include "editor/editor.h"
#include "panels/panels.h"
#include "dialogs/dialogs.h"
#include "project/project.h"
#include "syntax/lexer.h"
// SuggestionIndex lives in editor.h

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <sstream>
#include <fstream>
#include <regex>
#include <memory>
#include <dirent.h>
#include <sys/stat.h>

// ── Dialog helpers ──────────────────────────────────────────────────────

ushort execDialog(TDialog *d, void *data)
{
    TView *p = TProgram::application->validView(d);
    if (!p)
        return cmCancel;
    if (data)
        p->setData(data);
    ushort result = TProgram::deskTop->execView(p);
    if (result != cmCancel && data)
        p->getData(data);
    TObject::destroy(p);
    return result;
}

TDialog *createFindDialog()
{
    auto *d = new TDialog(TRect(0, 0, 38, 12), "Find");
    d->options |= ofCentered;

    auto *control = new TInputLine(TRect(3, 3, 32, 4), 256);
    d->insert(control);
    d->insert(new TLabel(TRect(2, 2, 15, 3), "~T~ext to find", control));
    d->insert(new THistory(TRect(32, 3, 35, 4), control, 10));

    d->insert(new TCheckBoxes(TRect(3, 5, 35, 7),
        new TSItem("~C~ase sensitive",
        new TSItem("~W~hole words only", nullptr))));

    d->insert(new TButton(TRect(14, 9, 24, 11), "O~K~", cmOK, bfDefault));
    d->insert(new TButton(TRect(26, 9, 36, 11), "Cancel", cmCancel, bfNormal));
    d->selectNext(False);
    return d;
}

TDialog *createReplaceDialog()
{
    auto *d = new TDialog(TRect(0, 0, 40, 16), "Replace");
    d->options |= ofCentered;

    auto *control = new TInputLine(TRect(3, 3, 34, 4), 256);
    d->insert(control);
    d->insert(new TLabel(TRect(2, 2, 15, 3), "~T~ext to find", control));
    d->insert(new THistory(TRect(34, 3, 37, 4), control, 10));

    control = new TInputLine(TRect(3, 6, 34, 7), 256);
    d->insert(control);
    d->insert(new TLabel(TRect(2, 5, 12, 6), "~N~ew text", control));
    d->insert(new THistory(TRect(34, 6, 37, 7), control, 11));

    d->insert(new TCheckBoxes(TRect(3, 8, 37, 12),
        new TSItem("~C~ase sensitive",
        new TSItem("~W~hole words only",
        new TSItem("~P~rompt on replace",
        new TSItem("~R~eplace all", nullptr))))));

    d->insert(new TButton(TRect(17, 13, 27, 15), "O~K~", cmOK, bfDefault));
    d->insert(new TButton(TRect(28, 13, 38, 15), "Cancel", cmCancel, bfNormal));
    d->selectNext(False);
    return d;
}

ushort doEditDialog(int dialog, ...)
{
    va_list arg;
    char buf[256] = {0};

    switch (dialog) {
    case edOutOfMemory:
        return messageBox("Not enough memory for this operation.",
                          mfError | mfOKButton);
    case edReadError: {
        va_start(arg, dialog);
        const char *fn = va_arg(arg, const char *);
        va_end(arg);
        snprintf(buf, sizeof(buf), "Error reading file %s.", fn);
        return messageBox(buf, mfError | mfOKButton);
    }
    case edWriteError: {
        va_start(arg, dialog);
        const char *fn = va_arg(arg, const char *);
        va_end(arg);
        snprintf(buf, sizeof(buf), "Error writing file %s.", fn);
        return messageBox(buf, mfError | mfOKButton);
    }
    case edCreateError: {
        va_start(arg, dialog);
        const char *fn = va_arg(arg, const char *);
        va_end(arg);
        snprintf(buf, sizeof(buf), "Error creating file %s.", fn);
        return messageBox(buf, mfError | mfOKButton);
    }
    case edSaveModify: {
        va_start(arg, dialog);
        const char *fn = va_arg(arg, const char *);
        va_end(arg);
        snprintf(buf, sizeof(buf), "%s has been modified. Save?", fn);
        return messageBox(buf, mfInformation | mfYesNoCancel);
    }
    case edSaveUntitled:
        return messageBox("Save untitled file?",
                          mfInformation | mfYesNoCancel);
    case edSaveAs: {
        va_start(arg, dialog);
        char *fn = va_arg(arg, char *);
        va_end(arg);
        return execDialog(new TFileDialog("*.*", "Save file as",
                                          "~N~ame", fdOKButton, 101), fn);
    }
    case edFind: {
        va_start(arg, dialog);
        char *fn = va_arg(arg, char *);
        va_end(arg);
        return execDialog(createFindDialog(), fn);
    }
    case edSearchFailed:
        return messageBox("Search string not found.",
                          mfError | mfOKButton);
    case edReplace: {
        va_start(arg, dialog);
        char *fn = va_arg(arg, char *);
        va_end(arg);
        return execDialog(createReplaceDialog(), fn);
    }
    case edReplacePrompt: {
        TRect r(0, 1, 40, 8);
        r.move((TProgram::deskTop->size.x - r.b.x) / 2, 0);
        TPoint t = TProgram::deskTop->makeGlobal(r.b);
        t.y++;
        va_start(arg, dialog);
        TPoint *pt = va_arg(arg, TPoint *);
        if (pt->y <= t.y)
            r.move(0, TProgram::deskTop->size.y - r.b.y - 2);
        va_end(arg);
        return messageBoxRect(r, "Replace this occurrence?",
                              mfYesNoCancel | mfInformation);
    }
    }
    return cmCancel;
}

// ── Menu bar ────────────────────────────────────────────────────────────

TMenuBar *TVIDEApp::initMenuBar(TRect r)
{
    r.b.y = r.a.y + 1;

    TSubMenu &sub1 = *new TSubMenu("~F~ile", kbAltF) +
        *new TMenuItem("~O~pen...", cmOpenFile, kbF3, hcNoContext, "F3") +
        *new TMenuItem("~N~ew", cmFileNew, kbCtrlN, hcNoContext, "Ctrl-N") +
        *new TMenuItem("New ~t~erminal", cmFileNewTerm, kbCtrlT, hcNoContext, "Ctrl-T") +
        *new TMenuItem("~S~ave", cmSave, kbF2, hcNoContext, "F2") +
        *new TMenuItem("S~a~ve as...", cmSaveAs, kbNoKey) +
        *new TMenuItem("~C~lose", cmFileClose, kbCtrlW, hcNoContext, "Ctrl-W") +
             newLine() +
        *new TMenuItem("Change ~d~ir...", cmChangeDrct, kbNoKey) +
        *new TMenuItem("~S~hell", cmDosShell, kbNoKey) +
             newLine() +
        *new TMenuItem("E~x~it", cmQuit, kbAltX, hcNoContext, "Alt-X");

    TSubMenu &sub2 = *new TSubMenu("~P~roject", kbAltP) +
        *new TMenuItem("~O~pen project...", cmProjectOpen, kbNoKey) +
        *new TMenuItem("~C~lose project", cmProjectClose, kbNoKey);

    TSubMenu &sub3 = *new TSubMenu("~E~dit", kbAltE) +
        *new TMenuItem("~U~ndo", cmUndo, kbCtrlZ, hcNoContext, "Ctrl-Z") +
             newLine() +
        *new TMenuItem("Cu~t~", cmCut, kbShiftDel, hcNoContext, "Shift-Del") +
        *new TMenuItem("~C~opy", cmCopy, kbCtrlIns, hcNoContext, "Ctrl-Ins") +
        *new TMenuItem("~P~aste", cmPaste, kbShiftIns, hcNoContext, "Shift-Ins") +
        *new TMenuItem("C~l~ear", cmClear, kbCtrlDel, hcNoContext, "Ctrl-Del") +
             newLine() +
        *new TMenuItem("~S~elect all", cmEditSelectAll, kbCtrlA, hcNoContext, "Ctrl-A");

    TSubMenu &sub4 = *new TSubMenu("~S~earch", kbAltS) +
        *new TMenuItem("~F~ind...", cmFind, kbCtrlF, hcNoContext, "Ctrl-F") +
        *new TMenuItem("~R~eplace...", cmReplace, kbCtrlH, hcNoContext, "Ctrl-H") +
        *new TMenuItem("~S~earch again", cmSearchAgain, kbF4, hcNoContext, "F4") +
             newLine() +
        *new TMenuItem("~G~o to line...", cmGotoLine, kbCtrlG, hcNoContext, "Ctrl-G") +
        *new TMenuItem("Find in ~f~iles...", cmFindInFiles, kbShiftF7, hcNoContext, "Shift-F7") +
        *new TMenuItem("Find sy~m~bol...", cmFindSymbol, kbCtrlF12, hcNoContext, "Ctrl-F12");

    TSubMenu &sub5 = *new TSubMenu("~W~indows", kbAltW) +
        *new TMenuItem("~S~ize/move", cmResize, kbCtrlF5, hcNoContext, "Ctrl-F5") +
        *new TMenuItem("~Z~oom", cmZoom, kbF5, hcNoContext, "F5") +
        *new TMenuItem("~T~ile", cmTile, kbNoKey) +
        *new TMenuItem("C~a~scade", cmCascade, kbNoKey) +
        *new TMenuItem("~N~ext", cmNext, kbF6, hcNoContext, "F6") +
        *new TMenuItem("~P~revious", cmPrev, kbShiftF6, hcNoContext, "Shift-F6") +
        *new TMenuItem("~C~lose all", cmWinCloseAll, kbNoKey) +
             newLine() +
        *new TMenuItem("Window ~l~ist...", cmWinList, kbAltW, hcNoContext, "Alt-W");

    TSubMenu &sub6 = *new TSubMenu("~T~ools", kbAltT) +
        *new TMenuItem("~F~ile tree", cmToolFileTree, kbNoKey) +
        *new TMenuItem("~S~tructure", cmToolStructure, kbNoKey) +
        *new TMenuItem("~W~indow list", cmToolWinList, kbNoKey) +
        *new TMenuItem("~T~erminal", cmToolTerminal, kbNoKey) +
        *new TMenuItem("~M~essages", cmToolMessages, kbNoKey) +
             newLine() +
        *new TMenuItem("~O~ptions...", cmToolOptions, kbNoKey) +
        *new TMenuItem("~C~olors...", cmToolColors, kbNoKey);

    TSubMenu &sub7 = *new TSubMenu("~H~elp", kbAltH) +
        *new TMenuItem("~A~bout...", cmHelpAbout, kbNoKey);

    return new TMenuBar(r, sub1 + sub2 + sub3 + sub4 + sub5 + sub6 + sub7);
}

// ── Status line ─────────────────────────────────────────────────────────

TStatusLine *TVIDEApp::initStatusLine(TRect r)
{
    r.a.y = r.b.y - 1;
    return new TStatusLine(r,
        *new TStatusDef(0, 0xFFFF) +
            *new TStatusItem("~Alt-X~ Exit", kbAltX, cmQuit) +
            *new TStatusItem("~F2~ Save", kbF2, cmSave) +
            *new TStatusItem("~F3~ Open", kbF3, cmOpenFile) +
            *new TStatusItem("~F5~ Zoom", kbF5, cmZoom) +
            *new TStatusItem("~F6~ Next", kbF6, cmNext) +
            *new TStatusItem("~F10~ Menu", kbF10, cmMenu) +
            *new TStatusItem(nullptr, kbShiftDel, cmCut) +
            *new TStatusItem(nullptr, kbCtrlIns, cmCopy) +
            *new TStatusItem(nullptr, kbShiftIns, cmPaste) +
            *new TStatusItem(nullptr, kbCtrlF5, cmResize)
    );
}

// ── Constructor ─────────────────────────────────────────────────────────

TVIDEApp::TVIDEApp(int argc, char **argv) :
    TProgInit(&TVIDEApp::initStatusLine,
              &TVIDEApp::initMenuBar,
              &TVIDEApp::initDeskTop),
    windowCount(0),
    terminalCount(0),
    fileTreePanel(nullptr),
    messagePanel(nullptr),
    structurePanel(nullptr),
    winListPanel(nullptr),
    lastEditorWindow(nullptr),
    lastStructEditor(nullptr),
    fileTreeWidth(28),
    structureWidth(30),
    winListWidth(28)
{
    TCommandSet ts;
    ts.enableCmd(cmSave);
    ts.enableCmd(cmSaveAs);
    ts.enableCmd(cmCut);
    ts.enableCmd(cmCopy);
    ts.enableCmd(cmPaste);
    ts.enableCmd(cmClear);
    ts.enableCmd(cmUndo);
    ts.enableCmd(cmFind);
    ts.enableCmd(cmReplace);
    ts.enableCmd(cmSearchAgain);
    disableCommands(ts);

    TEditor::editorDialog = doEditDialog;

    // Load saved themes and apply current
    ThemeManager::instance().load();
    auto &tm = ThemeManager::instance();
    if (tm.currentIndex() >= 0 && tm.currentIndex() < (int)tm.themes().size())
        EditorSettings::instance().colors = tm.themes()[tm.currentIndex()].colors;

    // Process command line arguments
    for (int i = 1; i < argc; i++) {
        struct stat st;
        if (stat(argv[i], &st) == 0 && S_ISDIR(st.st_mode)) {
            projectOpen(std::string(argv[i]));
        } else {
            openEditor(argv[i], True);
        }
    }

    // Default workspace: filetree + structure + winlist all open.
    if (!fileTreePanel)  toolFileTree();
    if (!structurePanel) toolStructure();
    if (!winListPanel)   toolWinList();
    relayout();
}

TVIDEApp::~TVIDEApp()
{
    ThemeManager::instance().save();
}

// ── Open / New editor ───────────────────────────────────────────────────

TSyntaxEditWindow *TVIDEApp::openEditor(const char *fileName, Boolean visible)
{
    TRect r = deskTop->getExtent();
    auto *w = new TSyntaxEditWindow(r, fileName, ++windowCount);
    if (!visible)
        w->hide();
    deskTop->insert(w);
    message(this, evBroadcast, cmWinListChanged, nullptr);
    return w;
}

void TVIDEApp::fileNewTerminal()
{
    if (!deskTop) return;
    TRect r = deskTop->getExtent();
    r.grow(-2, -2);
    TPoint termSize = TTerminalWindow::viewSize(r);
    tvterm::TerminalController *ctrl = TTerminalWindow::createController(termSize);
    if (!ctrl) {
        messageBox("Failed to spawn terminal.", mfError | mfOKButton);
        return;
    }
    auto *w = new TTerminalWindow(r, *ctrl, ++terminalCount);
    deskTop->insert(w);
    message(this, evBroadcast, cmWinListChanged, nullptr);
}

TSyntaxEditWindow *TVIDEApp::getActiveEditor()
{
    TView *v = deskTop->current;
    if (v) {
        auto *w = dynamic_cast<TSyntaxEditWindow *>(v);
        if (w)
            return w;
    }
    return nullptr;
}

// ── Event handling ──────────────────────────────────────────────────────

void TVIDEApp::handleEvent(TEvent &event)
{
    TApplication::handleEvent(event);

    if (event.what == evBroadcast && event.message.command == cmRelayout) {
        relayout();
        clearEvent(event);
        return;
    }

    if (event.what == evCommand) {
        switch (event.message.command) {
        case cmOpenFile:     fileOpen();    break;
        case cmFileTreeOpen: {
            const char *fn = (const char *)event.message.infoPtr;
            if (fn) openEditor(fn, True);
            break;
        }
        case cmFileNew:     fileNew();     break;
        case cmFileNewTerm: fileNewTerminal(); break;
        case cmFileClose:   fileClose();   break;
        case cmChangeDrct:  changeDir();   break;
        case cmDosShell:    dosShell();    break;

        case cmEditSelectAll: editSelectAll(); break;

        case cmGotoLine:    gotoLine();    break;
        case cmFindInFiles: findInFiles(); break;
        case cmFindSymbol:  findSymbol();  break;

        case cmWinCloseAll: winCloseAll(); break;
        case cmWinList:     winList();     break;

        case cmToolFileTree: toolFileTree(); break;
        case cmSortNameAsc:
            if (fileTreePanel) fileTreePanel->setSortMode(FileSortMode::NameAsc);
            break;
        case cmSortNameDesc:
            if (fileTreePanel) fileTreePanel->setSortMode(FileSortMode::NameDesc);
            break;
        case cmSortDateAsc:
            if (fileTreePanel) fileTreePanel->setSortMode(FileSortMode::DateAsc);
            break;
        case cmSortDateDesc:
            if (fileTreePanel) fileTreePanel->setSortMode(FileSortMode::DateDesc);
            break;
        case cmSortDirsFirst:
            if (fileTreePanel) fileTreePanel->toggleDirsFirst();
            break;
        case cmToolStructure: toolStructure(); break;
        case cmToolWinList:  toolWinList();  break;
        case cmToolTerminal: toolTerminal(); break;
        case cmToolMessages: toolMessages(); break;
        case cmToolOptions:  toolOptions();  break;
        case cmToolColors:   toolColors();   break;

        case cmProjectOpen:  projectOpen();  break;
        case cmProjectClose: projectClose(); break;

        case cmHelpAbout:   helpAbout();   break;

        default: return;
        }
        clearEvent(event);
    }
}

// ── File operations ─────────────────────────────────────────────────────

void TVIDEApp::fileOpen()
{
    char fileName[MAXPATH];
    std::strcpy(fileName, "*.*");

    if (execDialog(new TFileDialog("*.*", "Open file",
            "~N~ame", fdOpenButton, 100), fileName) != cmCancel)
        openEditor(fileName, True);
}

void TVIDEApp::fileNew()
{
    openEditor(nullptr, True);
}

void TVIDEApp::fileSave()
{
    // Handled by TFileEditor's cmSave
}

void TVIDEApp::fileSaveAs()
{
    // Handled by TFileEditor's cmSaveAs
}

void TVIDEApp::fileClose()
{
    auto *w = getActiveEditor();
    if (w)
        w->close();
}

void TVIDEApp::changeDir()
{
    execDialog(new TChDirDialog(cdNormal, 0), nullptr);
    if (fileTreePanel) {
        char cwd[MAXPATH];
        if (getcwd(cwd, sizeof(cwd)))
            fileTreePanel->refreshDir(cwd);
    }
}

void TVIDEApp::dosShell()
{
    suspend();
    if (system("$SHELL") != 0) {}
    resume();
    redraw();
}

// ── Edit ────────────────────────────────────────────────────────────────

void TVIDEApp::editSelectAll()
{
    auto *w = getActiveEditor();
    if (w && w->getEditor()) {
        auto *e = w->getEditor();
        e->setSelect(0, e->bufLen, True);
    }
}

// ── Search ──────────────────────────────────────────────────────────────

void TVIDEApp::searchFind()
{
    // Delegated to editor's built-in cmFind
}

void TVIDEApp::searchReplace()
{
    // Delegated to editor's built-in cmReplace
}

void TVIDEApp::gotoLine()
{
    auto *w = getActiveEditor();
    if (!w || !w->getEditor()) return;

    TDialog *d = createGotoLineDialog();
    char buf[16] = {0};
    if (execDialog(d, buf) == cmOK) {
        int line = atoi(buf);
        if (line > 0) {
            auto *e = w->getEditor();
            int totalLines = e->getTotalLines();
            if (line > totalLines) line = totalLines; // M4: clamp to last line
            // Navigate to line: count newlines
            uint pos = 0;
            int curLine = 1;
            while (pos < e->bufLen && curLine < line) {
                char ch = (pos < (uint)e->curPtr)
                    ? e->buffer[pos]
                    : e->buffer[pos + e->gapLen];
                if (ch == '\n')
                    curLine++;
                pos++;
            }
            e->setCurPtr(pos, 0);
            e->trackCursor(True);
        }
    }
}

void TVIDEApp::findInFiles()
{
    TDialog *d = createFindInFilesDialog();
    // Data layout: searchInput(256) + pathInput(256) + checkboxes(ushort)
    struct {
        char search[257]; // TInputLine: 1 byte length prefix + 256 chars
        char path[257];
        ushort options;
    } data;
    memset(&data, 0, sizeof(data));

    // Pre-fill path with project or cwd
    char cwd[MAXPATH] = {};
    if (fileTreePanel && !fileTreePanel->getCurrentDir().empty())
        strncpy(cwd, fileTreePanel->getCurrentDir().c_str(), MAXPATH - 1);
    else
        getcwd(cwd, sizeof(cwd));
    strncpy(data.path + 1, cwd, 255);
    data.path[0] = (char)strlen(data.path + 1);
    data.options = 0x02; // recursive by default

    if (execDialog(d, &data) == cmOK) {
        std::string searchText(data.search + 1, (unsigned char)data.search[0]);
        std::string searchPath(data.path + 1, (unsigned char)data.path[0]);
        bool caseSensitive = (data.options & 0x01) != 0;
        bool recursive     = (data.options & 0x02) != 0;
        bool useRegex      = (data.options & 0x04) != 0;

        if (searchText.empty()) return;
        if (searchPath.empty()) searchPath = ".";

        if (!messagePanel) toolMessages();
        if (messagePanel) {
            messagePanel->clear();
            messagePanel->addMessage(std::string("Searching for: ") + searchText +
                (useRegex ? "  [regex]" : "  [literal]") +
                (caseSensitive ? "  [case]" : "  [icase]"));
        }

        std::string cmd = "grep -n";
        if (useRegex) cmd += " -E"; else cmd += " -F";
        if (!caseSensitive) cmd += " -i";
        if (recursive) cmd += " -r";
        cmd += " --include='*.*' -- ";
        // Escape search text for shell
        cmd += "'";
        for (char c : searchText) {
            if (c == '\'') cmd += "'\\''";
            else cmd += c;
        }
        cmd += "' '";
        for (char c : searchPath) {
            if (c == '\'') cmd += "'\\''";
            else cmd += c;
        }
        cmd += "' 2>/dev/null | head -500";

        FILE *fp = popen(cmd.c_str(), "r");
        if (!fp) {
            if (messagePanel)
                messagePanel->addMessage("Error: could not run search.");
            return;
        }

        char line[1024];
        int count = 0;
        while (fgets(line, sizeof(line), fp)) {
            // Remove trailing newline
            int len = strlen(line);
            if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
            if (messagePanel)
                messagePanel->addMessage(line);
            count++;
        }
        pclose(fp);

        if (messagePanel) {
            char summary[128];
            snprintf(summary, sizeof(summary), "Found %d match%s.", count, count == 1 ? "" : "es");
            messagePanel->addMessage(summary);
        }
    }
}

// ── Find symbol ─────────────────────────────────────────────────────────

namespace {

bool isSourceExt(const std::string &name)
{
    auto dot = name.rfind('.');
    if (dot == std::string::npos) return false;
    std::string e = name.substr(dot + 1);
    for (auto &c : e) c = (char)tolower((unsigned char)c);
    static const char *exts[] = {
        "php","phtml","html","htm","css","scss","less",
        "js","jsx","mjs","ts","tsx","vue",
        "json","md","markdown","yml","yaml","sql","xml","svg","xsl",
        "twig","blade","latte", nullptr
    };
    for (int i = 0; exts[i]; i++) if (e == exts[i]) return true;
    return false;
}

bool shouldSkipDir(const std::string &name)
{
    if (name.empty()) return true;
    if (name[0] == '.') return true;
    static const char *skip[] = {
        "node_modules","vendor","build","dist","target","out",
        "__pycache__","bower_components","coverage", nullptr
    };
    for (int i = 0; skip[i]; i++) if (name == skip[i]) return true;
    return false;
}

void walkSourceFiles(const std::string &root, std::vector<std::string> &out, int depth = 0)
{
    if (depth > 16) return;
    DIR *d = opendir(root.c_str());
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != nullptr) {
        if (e->d_name[0] == '.' &&
            (e->d_name[1] == '\0' ||
             (e->d_name[1] == '.' && e->d_name[2] == '\0')))
            continue;
        std::string name = e->d_name;
        std::string full = root + "/" + name;
        struct stat st;
        if (stat(full.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (shouldSkipDir(name)) continue;
            walkSourceFiles(full, out, depth + 1);
        } else if (S_ISREG(st.st_mode) && isSourceExt(name)) {
            out.push_back(full);
        }
    }
    closedir(d);
}

bool readFileToString(const std::string &path, std::string &out, size_t maxBytes = 2 * 1024 * 1024)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    auto sz = (size_t)f.tellg();
    if (sz > maxBytes) return false;
    f.seekg(0, std::ios::beg);
    out.resize(sz);
    if (sz > 0) f.read(&out[0], sz);
    return true;
}

} // namespace

void TVIDEApp::findSymbol()
{
    TDialog *d = createFindSymbolDialog();
    if (!d) return;

    struct {
        char query[257];
        char path[257];
        ushort options;
    } data;
    memset(&data, 0, sizeof(data));

    char defPath[MAXPATH] = {};
    if (ProjectManager::instance().hasProject())
        strncpy(defPath, ProjectManager::instance().getProject().rootPath.c_str(), MAXPATH - 1);
    else if (fileTreePanel && !fileTreePanel->getCurrentDir().empty())
        strncpy(defPath, fileTreePanel->getCurrentDir().c_str(), MAXPATH - 1);
    else
        getcwd(defPath, sizeof(defPath));
    strncpy(data.path + 1, defPath, 255);
    data.path[0] = (char)strlen(data.path + 1);
    data.options = 0x02; // recursive default

    if (execDialog(d, &data) != cmOK) return;

    std::string query(data.query + 1, (unsigned char)data.query[0]);
    std::string root(data.path + 1, (unsigned char)data.path[0]);
    bool caseSensitive = (data.options & 0x01) != 0;
    bool recursive     = (data.options & 0x02) != 0;
    bool useRegex      = (data.options & 0x04) != 0;

    if (query.empty()) return;
    if (root.empty()) root = ".";

    std::regex rx;
    std::string lowerQuery = query;
    if (!caseSensitive)
        for (auto &c : lowerQuery) c = (char)tolower((unsigned char)c);
    bool rxOk = false;
    if (useRegex) {
        try {
            auto flags = std::regex::ECMAScript;
            if (!caseSensitive) flags = flags | std::regex::icase;
            rx = std::regex(query, flags);
            rxOk = true;
        } catch (const std::regex_error &) {
            messageBox("Invalid regular expression.", mfError | mfOKButton);
            return;
        }
    }

    if (!messagePanel) toolMessages();
    if (messagePanel) {
        messagePanel->clear();
        messagePanel->addMessage(std::string("Symbol search: ") + query +
            (useRegex ? "  [regex]" : "  [literal]") +
            (caseSensitive ? "  [case]" : "  [icase]") +
            "  in " + root);
    }

    std::vector<std::string> files;
    if (recursive) {
        walkSourceFiles(root, files);
    } else {
        DIR *dr = opendir(root.c_str());
        if (dr) {
            struct dirent *e;
            while ((e = readdir(dr)) != nullptr) {
                if (e->d_name[0] == '.') continue;
                std::string name = e->d_name;
                std::string full = root + "/" + name;
                struct stat st;
                if (stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode) && isSourceExt(name))
                    files.push_back(full);
            }
            closedir(dr);
        }
    }

    int matches = 0;
    int filesScanned = 0;
    for (const auto &path : files) {
        if (matches >= 1000) break;
        filesScanned++;
        std::unique_ptr<SyntaxLexer> lex(SyntaxLexer::createForFile(path));
        if (!lex) continue;
        const char *lang = lex->languageName();

        std::string text;
        if (!readFileToString(path, text)) continue;

        std::vector<SymbolInfo> syms;
        parseStructureText(lang, text, syms);
        if (syms.empty()) continue;

        for (const auto &s : syms) {
            bool hit = false;
            if (useRegex && rxOk) {
                hit = std::regex_search(s.name, rx);
            } else if (caseSensitive) {
                hit = s.name.find(query) != std::string::npos;
            } else {
                std::string lo = s.name;
                for (auto &c : lo) c = (char)tolower((unsigned char)c);
                hit = lo.find(lowerQuery) != std::string::npos;
            }
            if (!hit) continue;

            char line[1024];
            snprintf(line, sizeof(line), "%s:%d: [%s] %s",
                     path.c_str(), s.line, symbolKindName(s.kind), s.name.c_str());
            if (messagePanel) messagePanel->addMessage(line);
            matches++;
            if (matches >= 1000) break;
        }
    }

    if (messagePanel) {
        char summary[128];
        snprintf(summary, sizeof(summary),
                 "Found %d symbol%s in %d file%s.",
                 matches, matches == 1 ? "" : "s",
                 filesScanned, filesScanned == 1 ? "" : "s");
        messagePanel->addMessage(summary);
    }
}

// ── Windows ─────────────────────────────────────────────────────────────

void TVIDEApp::winCloseAll()
{
    std::vector<TWindow *> wins;
    TView *v = deskTop->first();
    if (v) {
        TView *t = v;
        do {
            auto *w = dynamic_cast<TWindow *>(t);
            if (w && !dynamic_cast<TMessagePanel *>(w))
                wins.push_back(w);
            t = t->next;
        } while (t != v);
    }
    for (auto *w : wins)
        w->close();
    message(this, evBroadcast, cmWinListChanged, nullptr);
}

void TVIDEApp::winList()
{
    std::vector<TWindow *> wins;
    TView *v = deskTop->first();
    if (v) {
        TView *t = v;
        do {
            auto *w = dynamic_cast<TWindow *>(t);
            if (w && !dynamic_cast<TMessagePanel *>(w))
                wins.push_back(w);
            t = t->next;
        } while (t != v);
    }
    if (wins.empty()) {
        messageBox("No open windows.", mfInformation | mfOKButton);
        return;
    }

    auto *items = new TUnsortedStringCollection(wins.size() + 1, 5);
    for (auto *w : wins) {
        const char *title = w->getTitle(256);
        items->insert(newStr(title ? title : "Untitled"));
    }

    int dH = std::min((int)wins.size() + 6, (int)size.y - 4);
    int dW = std::min(50, (int)size.x - 4);
    auto *d = new TDialog(TRect(0, 0, dW, dH), "Window List");
    d->options |= ofCentered;

    TRect sr(dW - 2, 2, dW - 1, dH - 3);
    auto *sb = new TScrollBar(sr);
    d->insert(sb);

    TRect lr(2, 2, dW - 2, dH - 3);
    auto *lb = new TListBox(lr, 1, sb);
    lb->newList(items);
    d->insert(lb);

    d->insert(new TButton(TRect(dW / 2 - 10, dH - 2, dW / 2, dH), "~S~witch", cmOK, bfDefault));
    d->insert(new TButton(TRect(dW / 2 + 1, dH - 2, dW / 2 + 11, dH), "Cancel", cmCancel, bfNormal));
    d->selectNext(False);

    TView *p = validView(d);
    if (!p) return;
    ushort result = deskTop->execView(p);
    int sel = lb->focused;
    TObject::destroy(p);

    if (result == cmOK && sel >= 0 && sel < (int)wins.size()) {
        wins[sel]->select();
    }
}

// ── Tools ───────────────────────────────────────────────────────────────

void TVIDEApp::toolFileTree()
{
    if (fileTreePanel) {
        if (fileTreePanel->state & sfVisible) {
            fileTreePanel->hide();
        } else {
            fileTreePanel->show();
        }
        relayout();
        return;
    }

    // Create with correct initial bounds
    int top = menuBar ? menuBar->size.y : 1;
    int bot = size.y - (statusLine ? statusLine->size.y : 1);
    TRect r(0, top, fileTreeWidth, bot);
    fileTreePanel = new TFileTreePanel(r);
    char cwd[MAXPATH];
    if (getcwd(cwd, sizeof(cwd)))
        fileTreePanel->refreshDir(cwd);
    insert(fileTreePanel); // insert into application, not desktop
    relayout();
}

void TVIDEApp::toolTerminal()
{
    fileNewTerminal();
}

void TVIDEApp::toolWinList()
{
    if (winListPanel) {
        if (winListPanel->state & sfVisible) {
            winListPanel->hide();
        } else {
            winListPanel->show();
            winListPanel->refresh();
        }
        relayout();
        return;
    }

    int top = menuBar ? menuBar->size.y : 1;
    int bot = size.y - (statusLine ? statusLine->size.y : 1);
    TRect r(0, top, winListWidth, bot);
    winListPanel = new TWindowListPanel(r);
    insert(winListPanel);
    winListPanel->refresh();
    relayout();
}

void TVIDEApp::toolMessages()
{
    if (messagePanel) {
        if (messagePanel->state & sfVisible) {
            messagePanel->hide();
        } else {
            messagePanel->show();
            messagePanel->select();
        }
        return;
    }

    TRect r = deskTop->getExtent();
    r.a.y = r.b.y - 8;
    messagePanel = new TMessagePanel(r);
    deskTop->insert(messagePanel);
}

void TVIDEApp::toolStructure()
{
    if (structurePanel) {
        if (structurePanel->state & sfVisible) {
            structurePanel->hide();
        } else {
            structurePanel->show();
            if (lastEditorWindow && lastEditorWindow->getEditor())
                structurePanel->refresh(lastEditorWindow->getEditor());
        }
        relayout();
        return;
    }

    // Create with correct initial bounds
    int top = menuBar ? menuBar->size.y : 1;
    int bot = size.y - (statusLine ? statusLine->size.y : 1);
    TRect r(size.x - structureWidth, top, size.x, bot);
    structurePanel = new TStructurePanel(r);
    insert(structurePanel); // insert into application, not desktop
    relayout();

    // Populate from last known editor
    if (lastEditorWindow && lastEditorWindow->getEditor())
        structurePanel->refresh(lastEditorWindow->getEditor());
}

void TVIDEApp::toolOptions()
{
    auto &settings = EditorSettings::instance();

    TDialog *d = createOptionsDialog();
    if (!d) return;

    // Layout matches insertion order in createOptionsDialog():
    //   1. Editor checkboxes (8)
    //   2. Tab size input (4 + 1)
    //   3. File tree sort radio
    //   4. Dirs-first checkbox
    //   5. Structure sort radio
    struct {
        ushort editorCB;
        char   tabSize[5];
        ushort autoSuggestCB;
        char   suggestDelay[7];
        ushort ftSort;
        ushort dirsFirstCB;
        ushort structSort;
    } data;
    memset(&data, 0, sizeof(data));

    if (settings.showLineNumbers)    data.editorCB |= 0x01;
    if (settings.syntaxHighlight)    data.editorCB |= 0x02;
    if (settings.showWhitespace)     data.editorCB |= 0x04;
    if (settings.showEOL)            data.editorCB |= 0x08;
    if (settings.autoIndent)         data.editorCB |= 0x10;
    if (settings.bracketMatch)       data.editorCB |= 0x20;
    if (settings.useTabs)            data.editorCB |= 0x40;
    if (settings.autoCloseBrackets)  data.editorCB |= 0x80;

    snprintf(data.tabSize + 1, sizeof(data.tabSize) - 1, "%d", settings.tabSize);
    data.tabSize[0] = (char)strlen(data.tabSize + 1);

    if (settings.autoSuggest) data.autoSuggestCB |= 0x01;
    snprintf(data.suggestDelay + 1, sizeof(data.suggestDelay) - 1, "%d",
             settings.autoSuggestDelayMs);
    data.suggestDelay[0] = (char)strlen(data.suggestDelay + 1);

    if (fileTreePanel) {
        switch (fileTreePanel->getSortMode()) {
        case FileSortMode::NameAsc:  data.ftSort = 0; break;
        case FileSortMode::NameDesc: data.ftSort = 1; break;
        case FileSortMode::DateAsc:  data.ftSort = 2; break;
        case FileSortMode::DateDesc: data.ftSort = 3; break;
        }
        if (fileTreePanel->getDirsFirst()) data.dirsFirstCB |= 0x01;
    } else {
        data.ftSort = 0;
        data.dirsFirstCB = 0x01;
    }

    if (structurePanel) {
        switch (structurePanel->getSortMode()) {
        case SymbolSortMode::LineAsc:  data.structSort = 0; break;
        case SymbolSortMode::LineDesc: data.structSort = 1; break;
        case SymbolSortMode::NameAsc:  data.structSort = 2; break;
        case SymbolSortMode::NameDesc: data.structSort = 3; break;
        case SymbolSortMode::KindAsc:  data.structSort = 4; break;
        case SymbolSortMode::KindDesc: data.structSort = 5; break;
        }
    } else {
        data.structSort = 0;
    }

    if (execDialog(d, &data) == cmOK) {
        settings.showLineNumbers    = (data.editorCB & 0x01) != 0;
        settings.syntaxHighlight    = (data.editorCB & 0x02) != 0;
        settings.showWhitespace     = (data.editorCB & 0x04) != 0;
        settings.showEOL            = (data.editorCB & 0x08) != 0;
        settings.autoIndent         = (data.editorCB & 0x10) != 0;
        settings.bracketMatch       = (data.editorCB & 0x20) != 0;
        settings.useTabs            = (data.editorCB & 0x40) != 0;
        settings.autoCloseBrackets  = (data.editorCB & 0x80) != 0;

        int ts = atoi(data.tabSize + 1);
        if (ts >= 1 && ts <= 16) settings.tabSize = ts;

        settings.autoSuggest = (data.autoSuggestCB & 0x01) != 0;
        int delay = atoi(data.suggestDelay + 1);
        if (delay >= 0 && delay <= 5000) settings.autoSuggestDelayMs = delay;

        if (fileTreePanel) {
            FileSortMode fm = FileSortMode::NameAsc;
            switch (data.ftSort) {
            case 0: fm = FileSortMode::NameAsc;  break;
            case 1: fm = FileSortMode::NameDesc; break;
            case 2: fm = FileSortMode::DateAsc;  break;
            case 3: fm = FileSortMode::DateDesc; break;
            }
            fileTreePanel->setSortMode(fm);
            fileTreePanel->setDirsFirst((data.dirsFirstCB & 0x01) != 0);
        }

        if (structurePanel) {
            SymbolSortMode sm = SymbolSortMode::LineAsc;
            switch (data.structSort) {
            case 0: sm = SymbolSortMode::LineAsc;  break;
            case 1: sm = SymbolSortMode::LineDesc; break;
            case 2: sm = SymbolSortMode::NameAsc;  break;
            case 3: sm = SymbolSortMode::NameDesc; break;
            case 4: sm = SymbolSortMode::KindAsc;  break;
            case 5: sm = SymbolSortMode::KindDesc; break;
            }
            structurePanel->setSortMode(sm);
        }

        redraw();
    }
}

void TVIDEApp::toolColors()
{
    auto *dlg = new TSyntaxColorsDialog();
    TView *p = validView(dlg);
    if (!p) return;

    ushort result = deskTop->execView(p);
    if (result == cmOK) {
        EditorSettings::instance().colors = dlg->editColors;
        // Update theme manager selection
        auto &tm = ThemeManager::instance();
        int sel = dlg->selectedThemeIndex();
        if (sel >= 0 && sel < (int)tm.themes().size()) {
            tm.selectTheme(sel);
            tm.save();
        }
        redraw();
    }
    TObject::destroy(p);
}

// ── Project ─────────────────────────────────────────────────────────────

void TVIDEApp::projectOpen(const std::string &path)
{
    // Resolve to absolute path
    char resolved[PATH_MAX];
    if (!realpath(path.c_str(), resolved))
        std::strncpy(resolved, path.c_str(), PATH_MAX);

    std::string dir = resolved;

    // Try to find a JetBrains project root
    std::string root = ProjectManager::findProjectRoot(dir);
    if (root.empty() && ProjectManager::isProjectDir(dir))
        root = dir;

    if (!root.empty() && ProjectManager::instance().openProject(root)) {
        auto &proj = ProjectManager::instance().getProject();
        char msg[512];
        snprintf(msg, sizeof(msg), "Opened project: %s", proj.name.c_str());
        if (messagePanel)
            messagePanel->addMessage(msg);
        dir = proj.rootPath;
        SuggestionIndex::instance().clear();
        SuggestionIndex::instance().scanProject(proj.rootPath);
    }

    // Always open file tree for the directory
    if (!fileTreePanel) toolFileTree();
    if (fileTreePanel)
        fileTreePanel->refreshDir(dir);
}

void TVIDEApp::projectOpen()
{
    auto *d = new OpenProjectDialog();
    TView *p = validView(d);
    if (!p) return;
    ushort result = deskTop->execView(p);
    std::string chosen = d->getSelectedPath();
    TObject::destroy(p);
    if (result == cmOK && !chosen.empty())
        projectOpen(chosen);
}

void TVIDEApp::projectClose()
{
    ProjectManager::instance().closeProject();
    if (messagePanel)
        messagePanel->addMessage("Project closed.");
}

// ── Help ────────────────────────────────────────────────────────────────

void TVIDEApp::helpAbout()
{
    TDialog *d = createAboutDialog();
    if (!d) return; // L14: null check
    deskTop->execView(d);
    TObject::destroy(d);
}

void TVIDEApp::outOfMemory()
{
    messageBox("Not enough memory for this operation.",
               mfError | mfOKButton);
}

void TVIDEApp::idle()
{
    TApplication::idle();

    if (!deskTop) return;

    message(this, evBroadcast, cmCheckTerminalUpdates, nullptr);

    int deskCount = 0;
    {
        TView *p = deskTop->first();
        if (p) {
            TView *t = p;
            do { deskCount++; t = t->next; } while (t != p);
        }
    }
    static int lastDeskCount = -1;
    if (deskCount != lastDeskCount) {
        lastDeskCount = deskCount;
        if (winListPanel && (winListPanel->state & sfVisible))
            winListPanel->refresh();
    }

    // Find current editor window (if focused view is one)
    TView *v = deskTop->current;
    TSyntaxEditWindow *w = nullptr;
    if (v)
        w = dynamic_cast<TSyntaxEditWindow *>(v);

    // Track last focused editor — only update when an editor IS focused
    if (w)
        lastEditorWindow = w;

    // Validate lastEditorWindow is still alive (still inserted in desktop)
    if (lastEditorWindow) {
        bool found = false;
        TView *p = deskTop->first();
        if (p) {
            TView *t = p;
            do {
                if (t == lastEditorWindow) { found = true; break; }
                t = t->next;
            } while (t != p);
        }
        if (!found)
            lastEditorWindow = nullptr;
    }

    if (lastEditorWindow && lastEditorWindow->getGutter())
        lastEditorWindow->getGutter()->drawView();

    if (lastEditorWindow && lastEditorWindow->getEditor())
        lastEditorWindow->getEditor()->tickAutoSuggest();

    // M10: Check for external file changes on the active editor
    if (lastEditorWindow && lastEditorWindow->getEditor()) {
        auto *ed = lastEditorWindow->getEditor();
        if (ed->hasExternalChange()) {
            ed->updateModTime(); // prevent repeated prompts
            ushort result = messageBox(
                mfConfirmation | mfYesButton | mfNoButton,
                "%s has been modified externally. Reload?",
                ed->fileName);
            if (result == cmYes) {
                // Re-read the file by closing and re-opening
                const char *fn = ed->fileName;
                char nameCopy[MAXPATH];
                strncpy(nameCopy, fn, MAXPATH - 1);
                nameCopy[MAXPATH - 1] = '\0';
                lastEditorWindow->close();
                openEditor(nameCopy, True);
            }
        }
    }

    // Refresh structure panel when tracked editor changes
    if (structurePanel && (structurePanel->state & sfVisible)) {
        TSyntaxEditor *cur = (lastEditorWindow && lastEditorWindow->getEditor())
                             ? lastEditorWindow->getEditor() : nullptr;
        if (cur != lastStructEditor) {
            lastStructEditor = cur;
            structurePanel->refresh(cur);
        }
    }
}

// ── Layout management ──────────────────────────────────────────────────

void TVIDEApp::relayout()
{
    if (!deskTop || !menuBar || !statusLine) return;

    int appW = size.x;
    int top = menuBar->size.y;
    int bot = size.y - statusLine->size.y;

    int leftW = 0;
    int rightW = 0;

    bool ftVis = fileTreePanel && (fileTreePanel->state & sfVisible);
    bool wlVis = winListPanel && (winListPanel->state & sfVisible);

    if (ftVis || wlVis) {
        int w = ftVis ? fileTreePanel->size.x : winListPanel->size.x;
        if (ftVis && wlVis) {
            int alt = winListPanel->size.x;
            if (alt > w) w = alt;
        }
        if (w < 15) w = 15;
        if (w > appW / 3) w = appW / 3;
        leftW = w;
        if (ftVis) fileTreeWidth = leftW;
        if (wlVis) winListWidth = leftW;
    }

    if (ftVis && wlVis) {
        int half = (bot - top) / 2;
        TRect lr(0, top, leftW, top + half);
        fileTreePanel->TWindow::changeBounds(lr);
        TRect wr(0, top + half, leftW, bot);
        winListPanel->TWindow::changeBounds(wr);
    } else if (ftVis) {
        TRect lr(0, top, leftW, bot);
        fileTreePanel->TWindow::changeBounds(lr);
    } else if (wlVis) {
        TRect wr(0, top, leftW, bot);
        winListPanel->TWindow::changeBounds(wr);
    }

    // Position structure panel (right dock)
    if (structurePanel && (structurePanel->state & sfVisible)) {
        rightW = structurePanel->size.x;
        if (rightW < 15) rightW = 15;
        if (rightW > appW / 3) rightW = appW / 3;
        structureWidth = rightW;
        TRect rr(appW - rightW, top, appW, bot);
        structurePanel->TWindow::changeBounds(rr);
    }

    // Resize desktop to fit between docked panels
    TRect dr(leftW, top, appW - rightW, bot);
    if (dr.b.x - dr.a.x < 20)
        dr.b.x = dr.a.x + 20;
    deskTop->changeBounds(dr);
}

void TVIDEApp::changeBounds(const TRect &bounds)
{
    TApplication::changeBounds(bounds);
    relayout();
}
