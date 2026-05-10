#ifndef TVIDE_DIALOGS_H
#define TVIDE_DIALOGS_H

#include "app.h"

TDialog *createGotoLineDialog();
TDialog *createFindInFilesDialog();
TDialog *createFindSymbolDialog();
TDialog *createAboutDialog();

// Live, type-as-you-search dialogs. Run via deskTop->execView. After cmOK
// the result fields are populated; caller jumps to file/line.

class LiveSymbolDialog : public TDialog {
public:
    explicit LiveSymbolDialog(const std::string &rootPath);
    void handleEvent(TEvent &event) override;

    // Result after cmOK
    std::string resultPath;
    int resultLine = 0;
    std::string resultName;

private:
    struct Entry { std::string path; int line; int kind; std::string name; };

    std::string rootPath;
    class TInputLine *queryInput;
    class TCheckBoxes *opts;          // bit 0 = case, bit 1 = regex
    class TListBox *resultsList;
    class TScrollBar *resultsSB;
    class TUnsortedStringCollection *items;

    std::vector<Entry> all;     // built once
    std::vector<int>   filtered;

    std::string lastQuery;
    ushort lastOpts = 0xFFFF;

    void buildIndex();
    void refilter();
    std::string currentQuery() const;
    ushort currentOpts() const;
};

class LiveTextSearchDialog : public TDialog {
public:
    explicit LiveTextSearchDialog(const std::string &rootPath);
    ~LiveTextSearchDialog();
    void handleEvent(TEvent &event) override;

    // Result after cmOK
    std::string resultPath;
    int resultLine = 0;

private:
    std::string rootPath;
    class TInputLine *queryInput;
    class TCheckBoxes *opts;          // bit 0 = case, bit 1 = regex
    class TListBox *resultsList;
    class TScrollBar *resultsSB;
    class TUnsortedStringCollection *items;

    std::string lastQuery;
    ushort lastOpts = 0xFFFF;
    long long lastEditMs = 0;
    bool dirty = false;
    struct Hit { std::string path; int line; std::string text; };
    std::vector<Hit> hits;

    void scheduleSearch();
    void doSearch();
    std::string currentQuery() const;
    ushort currentOpts() const;
};

// Custom directory chooser for "Open Project"
// Run via deskTop->execView. On cmOK, getSelectedPath() yields the chosen path.
class OpenProjectDialog : public TDialog {
public:
    OpenProjectDialog();
    virtual void handleEvent(TEvent &event) override;
    const std::string &getSelectedPath() const { return currentPath; }

private:
    std::string currentPath;
    int sortMode; // 0=NameAsc 1=NameDesc 2=DateAsc 3=DateDesc
    class TListBox *dirList;
    class TScrollBar *dirSB;
    class TPathLabel *pathLabel;
    class TRadioButtons *sortRadio;
    class TUnsortedStringCollection *items;
    std::vector<std::string> dirNames;
    std::vector<long> dirMTimes;

    void loadDir(const std::string &path);
    void rebuildList();
    void enterFocused();
    void parentDir();
};

#endif // TVIDE_DIALOGS_H
