#include "dialogs.h"
#include "panels/panels.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <algorithm>

// Lightweight one-line label whose text can be replaced after construction.
class TPathLabel : public TView {
public:
    std::string text;
    TPathLabel(const TRect &b) : TView(b) { eventMask = 0; }
    void draw() override {
        TDrawBuffer buf;
        TColorAttr c = mapColor(2);
        buf.moveChar(0, ' ', c, size.x);
        buf.moveStr(0, TStringView(text.data(), text.size()), c);
        writeLine(0, 0, size.x, 1, buf);
    }
};

namespace {
constexpr int kDlgW = 72;
constexpr int kDlgH = 22;
constexpr ushort cmDirEnter  = 4001;
constexpr ushort cmDirParent = 4002;
constexpr ushort cmSortChange = 4003;
}

OpenProjectDialog::OpenProjectDialog()
    : TWindowInit(&OpenProjectDialog::initFrame),
      TDialog(TRect(0, 0, kDlgW, kDlgH), "Open Project"),
      sortMode(0), dirList(nullptr), dirSB(nullptr),
      pathLabel(nullptr), sortRadio(nullptr), items(nullptr)
{
    options |= ofCentered;

    pathLabel = new TPathLabel(TRect(2, 1, kDlgW - 2, 2));
    insert(pathLabel);

    int listX0 = 2, listX1 = 48;
    int listY0 = 3, listY1 = kDlgH - 4;

    TRect sr(listX1 - 1, listY0, listX1, listY1);
    dirSB = new TScrollBar(sr);
    insert(dirSB);

    TRect lr(listX0, listY0, listX1 - 1, listY1);
    dirList = new TListBox(lr, 1, dirSB);
    insert(dirList);

    insert(new TStaticText(TRect(50, 2, kDlgW - 2, 3), "Sort:"));
    sortRadio = new TRadioButtons(TRect(50, 3, kDlgW - 2, 7),
        new TSItem("~N~ame asc",
        new TSItem("Name ~D~esc",
        new TSItem("Da~t~e asc",
        new TSItem("Date des~c~", nullptr)))));
    insert(sortRadio);

    insert(new TStaticText(TRect(50, 8, kDlgW - 2, 9), "Keys:"));
    insert(new TStaticText(TRect(50, 9,  kDlgW - 2, 10), "Enter ▸ enter"));
    insert(new TStaticText(TRect(50, 10, kDlgW - 2, 11), "Bksp ▸ parent"));
    insert(new TStaticText(TRect(50, 11, kDlgW - 2, 12), "OK ▸ choose"));

    int btnY = kDlgH - 3;
    insert(new TButton(TRect(kDlgW/2 - 11, btnY, kDlgW/2 - 1, btnY + 2),
                       "O~K~", cmOK, bfDefault));
    insert(new TButton(TRect(kDlgW/2 + 1, btnY, kDlgW/2 + 11, btnY + 2),
                       "Cancel", cmCancel, bfNormal));

    items = new TUnsortedStringCollection(50, 10);
    dirList->newList(items);

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) loadDir(cwd);
    else loadDir("/");

    selectNext(False);
}

void OpenProjectDialog::loadDir(const std::string &path)
{
    char resolved[PATH_MAX];
    if (realpath(path.c_str(), resolved))
        currentPath = resolved;
    else
        currentPath = path;

    dirNames.clear();
    dirMTimes.clear();

    DIR *d = opendir(currentPath.c_str());
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != nullptr) {
            if (e->d_name[0] == '.' &&
                (e->d_name[1] == '\0' ||
                 (e->d_name[1] == '.' && e->d_name[2] == '\0')))
                continue;
            std::string full = currentPath + "/" + e->d_name;
            struct stat st;
            if (stat(full.c_str(), &st) != 0) continue;
            if (!S_ISDIR(st.st_mode)) continue;
            dirNames.push_back(e->d_name);
            dirMTimes.push_back((long)st.st_mtime);
        }
        closedir(d);
    }

    rebuildList();

    if (pathLabel) {
        pathLabel->text = std::string("Path: ") + currentPath;
        pathLabel->drawView();
    }
}

void OpenProjectDialog::rebuildList()
{
    std::vector<int> idx(dirNames.size());
    for (size_t i = 0; i < idx.size(); i++) idx[i] = (int)i;

    auto &names = dirNames;
    auto &times = dirMTimes;
    int mode = sortMode;
    std::sort(idx.begin(), idx.end(), [&](int a, int b) {
        switch (mode) {
        case 0: return names[a] < names[b];
        case 1: return names[a] > names[b];
        case 2:
            if (times[a] != times[b]) return times[a] < times[b];
            return names[a] < names[b];
        case 3:
            if (times[a] != times[b]) return times[a] > times[b];
            return names[a] < names[b];
        }
        return names[a] < names[b];
    });

    auto *newItems = new TUnsortedStringCollection((ccIndex)idx.size() + 2, 10);
    newItems->insert(newStr("[..]"));
    std::vector<std::string> sortedNames;
    sortedNames.push_back("..");
    for (int i : idx) {
        std::string disp = "[" + names[i] + "]";
        newItems->insert(newStr(disp.c_str()));
        sortedNames.push_back(names[i]);
    }
    dirNames = std::move(sortedNames);
    dirMTimes.clear();

    dirList->newList(newItems);
    items = newItems;
    dirList->focusItem(0);
    dirList->drawView();
}

void OpenProjectDialog::enterFocused()
{
    int f = dirList->focused;
    if (f < 0 || f >= (int)dirNames.size()) return;
    if (dirNames[f] == "..") { parentDir(); return; }
    loadDir(currentPath + "/" + dirNames[f]);
}

void OpenProjectDialog::parentDir()
{
    auto slash = currentPath.rfind('/');
    if (slash == std::string::npos || slash == 0)
        loadDir("/");
    else
        loadDir(currentPath.substr(0, slash));
}

void OpenProjectDialog::handleEvent(TEvent &event)
{
    if (event.what == evKeyDown) {
        switch (event.keyDown.keyCode) {
        case kbEnter:
            if (dirList && dirList->focused >= 0) {
                enterFocused();
                clearEvent(event);
                return;
            }
            break;
        case kbBack:
            parentDir();
            clearEvent(event);
            return;
        }
    }

    if (event.what == evBroadcast && event.message.command == cmReceivedFocus) {
        // ignore
    }

    TDialog::handleEvent(event);

    if (event.what == evCommand && event.message.command == cmDefault) {
        // suppress
    }

    if (sortRadio) {
        ushort cur = 0;
        sortRadio->getData(&cur);
        if ((int)cur != sortMode) {
            sortMode = (int)cur;
            // Reload current path with new sort
            std::string p = currentPath;
            loadDir(p);
        }
    }
}
