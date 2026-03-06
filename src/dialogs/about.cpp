#include "dialogs.h"

TDialog *createAboutDialog()
{
    auto *d = new TDialog(TRect(0, 0, 44, 14), "About TVIDE");
    d->options |= ofCentered;

    d->insert(new TStaticText(TRect(3, 2, 41, 3),
        "\003TVIDE - Turbo Vision IDE"));
    d->insert(new TStaticText(TRect(3, 3, 41, 4),
        "\003Version 1.0.0"));
    d->insert(new TStaticText(TRect(3, 5, 41, 6),
        "\003A modern PHP/Web IDE"));
    d->insert(new TStaticText(TRect(3, 6, 41, 7),
        "\003Built with magiblot/tvision"));
    d->insert(new TStaticText(TRect(3, 8, 41, 9),
        "\003Supports PHP, HTML, CSS,"));
    d->insert(new TStaticText(TRect(3, 9, 41, 10),
        "\003JS, TS, Vue, JSON, YAML, SQL"));

    d->insert(new TButton(TRect(16, 11, 28, 13), "O~K~", cmOK, bfDefault));

    return d;
}
