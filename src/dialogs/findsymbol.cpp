#include "dialogs.h"

TDialog *createFindSymbolDialog()
{
    auto *d = new TDialog(TRect(0, 0, 50, 15), "Find Symbol");
    d->options |= ofCentered;

    auto *queryInput = new TInputLine(TRect(3, 3, 44, 4), 256);
    d->insert(queryInput);
    d->insert(new TLabel(TRect(2, 2, 18, 3), "Sy~m~bol query", queryInput));
    d->insert(new THistory(TRect(44, 3, 47, 4), queryInput, 25));

    auto *pathInput = new TInputLine(TRect(3, 6, 44, 7), 256);
    d->insert(pathInput);
    d->insert(new TLabel(TRect(2, 5, 18, 6), "~P~roject path", pathInput));
    d->insert(new THistory(TRect(44, 6, 47, 7), pathInput, 26));

    d->insert(new TCheckBoxes(TRect(3, 8, 32, 11),
        new TSItem("~C~ase sensitive",
        new TSItem("~R~ecursive",
        new TSItem("Reg~e~xp (ECMAScript)", nullptr)))));

    d->insert(new TButton(TRect(20, 12, 30, 14), "O~K~", cmOK, bfDefault));
    d->insert(new TButton(TRect(32, 12, 42, 14), "Cancel", cmCancel, bfNormal));
    d->selectNext(False);
    return d;
}
