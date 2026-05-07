#include "dialogs.h"

TDialog *createFindInFilesDialog()
{
    auto *d = new TDialog(TRect(0, 0, 50, 15), "Find in Files");
    d->options |= ofCentered;

    auto *searchInput = new TInputLine(TRect(3, 3, 44, 4), 256);
    d->insert(searchInput);
    d->insert(new TLabel(TRect(2, 2, 18, 3), "~S~earch text", searchInput));
    d->insert(new THistory(TRect(44, 3, 47, 4), searchInput, 20));

    auto *pathInput = new TInputLine(TRect(3, 6, 44, 7), 256);
    d->insert(pathInput);
    d->insert(new TLabel(TRect(2, 5, 18, 6), "~P~ath / mask", pathInput));
    d->insert(new THistory(TRect(44, 6, 47, 7), pathInput, 21));

    d->insert(new TCheckBoxes(TRect(3, 8, 32, 11),
        new TSItem("~C~ase sensitive",
        new TSItem("~R~ecursive",
        new TSItem("Reg~e~xp (ERE)", nullptr)))));

    d->insert(new TButton(TRect(20, 12, 30, 14), "O~K~", cmOK, bfDefault));
    d->insert(new TButton(TRect(32, 12, 42, 14), "Cancel", cmCancel, bfNormal));
    d->selectNext(False);
    return d;
}
