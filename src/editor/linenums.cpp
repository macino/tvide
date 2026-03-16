#include "editor.h"
#include <cstdio>

TLineGutter::TLineGutter(const TRect &bounds, TSyntaxEditor *anEditor)
    : TView(bounds), editor(anEditor)
{
    growMode = gfGrowHiY;
    eventMask = evBroadcast;
    options |= ofPostProcess; // redraw after editor updates
}

int TLineGutter::gutterWidth() const
{
    // L5: Dynamic width based on line count
    if (!editor) return 5;
    int total = editor->getTotalLines();
    int digits = 1;
    int n = total;
    while (n >= 10) { digits++; n /= 10; }
    return digits + 2; // digits + 1 space + 1 separator
}

void TLineGutter::draw()
{
    if (!editor) return;

    TDrawBuffer b;
    // Derive gutter colors from the current normal text color
    auto &settings = EditorSettings::instance();
    uint8_t biosNormal = settings.colors.normal.toBIOS();
    TColorAttr numColor((biosNormal & 0xF0) | 0x03); // dark cyan on same bg
    TColorAttr sepColor = numColor;

    int topLine = editor->getTopLine();

    for (int y = 0; y < size.y; y++) {
        int lineNum = topLine + y + 1;
        int totalLines = editor->getTotalLines();

        char numBuf[16];
        if (lineNum <= totalLines)
            snprintf(numBuf, sizeof(numBuf), "%*d", size.x - 1, lineNum);
        else
            snprintf(numBuf, sizeof(numBuf), "%*s", size.x - 1, "~");

        b.moveStr(0, numBuf, numColor);
        b.moveChar(size.x - 1, '\xB3', sepColor, 1);
        writeBuf(0, y, size.x, 1, b);
    }
}
