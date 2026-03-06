#include "dialogs.h"

TDialog *createGotoLineDialog()
{
    auto *d = new TDialog(TRect(0, 0, 34, 9), "Go to Line");
    d->options |= ofCentered;

    auto *input = new TInputLine(TRect(3, 3, 28, 4), 10);
    d->insert(input);
    d->insert(new TLabel(TRect(2, 2, 18, 3), "~L~ine number", input));

    d->insert(new TButton(TRect(7, 6, 17, 8), "O~K~", cmOK, bfDefault));
    d->insert(new TButton(TRect(19, 6, 29, 8), "Cancel", cmCancel, bfNormal));
    d->selectNext(False);
    return d;
}
