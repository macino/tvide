#ifndef TVIDE_DIALOGS_H
#define TVIDE_DIALOGS_H

#include "app.h"

TDialog *createGotoLineDialog();
TDialog *createFindInFilesDialog();
TDialog *createFindSymbolDialog();
TDialog *createAboutDialog();

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
