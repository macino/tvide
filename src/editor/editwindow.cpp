#include "editor.h"
#include <cstring>
#include <cstdio>

TSyntaxEditWindow::TSyntaxEditWindow(const TRect &bounds,
                                     const char *fileName,
                                     int aNumber)
    : TWindowInit(&TSyntaxEditWindow::initFrame),
      TWindow(bounds, fileName ? fileName : "Untitled", aNumber),
      editor(nullptr), indicator(nullptr),
      hScrollBar(nullptr), vScrollBar(nullptr), gutter(nullptr)
{
    options |= ofTileable;
    createContents(fileName);
}

TSyntaxEditWindow::~TSyntaxEditWindow() {}

void TSyntaxEditWindow::createContents(const char *fileName)
{
    TRect r = getExtent();

    // Horizontal scrollbar at bottom
    TRect hr(1, r.b.y - 1, r.b.x - 1, r.b.y);
    hScrollBar = new TScrollBar(hr);
    insert(hScrollBar);

    // Vertical scrollbar at right
    TRect vr(r.b.x - 1, 1, r.b.x, r.b.y - 1);
    vScrollBar = new TScrollBar(vr);
    insert(vScrollBar);

    // Indicator
    TRect ir(2, r.b.y - 1, 16, r.b.y);
    indicator = new TIndicator(ir);
    insert(indicator);

    // Determine gutter width
    int gutterW = 0;
    if (EditorSettings::instance().showLineNumbers)
        gutterW = 5; // e.g. "  42 " = 5 cols

    // Editor area (leave room for gutter)
    TRect er(1 + gutterW, 1, r.b.x - 1, r.b.y - 1);
    TStringView fn = fileName ? TStringView(fileName) : TStringView("");
    editor = new TSyntaxEditor(er, hScrollBar, vScrollBar, indicator, fn);
    insert(editor);

    // Line number gutter
    if (gutterW > 0) {
        TRect gr(1, 1, 1 + gutterW, r.b.y - 1);
        gutter = new TLineGutter(gr, editor);
        insert(gutter);
    }
}

void TSyntaxEditWindow::relayout()
{
    TRect r = getExtent();
    int gutterW = 0;
    if (EditorSettings::instance().showLineNumbers)
        gutterW = 5;

    if (editor) {
        TRect er(1 + gutterW, 1, r.b.x - 1, r.b.y - 1);
        editor->locate(er);
    }

    if (gutter) {
        if (EditorSettings::instance().showLineNumbers) {
            TRect gr(1, 1, 1 + gutterW, r.b.y - 1);
            gutter->locate(gr);
            gutter->show();
        } else {
            gutter->hide();
        }
    } else if (EditorSettings::instance().showLineNumbers) {
        TRect gr(1, 1, 1 + gutterW, r.b.y - 1);
        gutter = new TLineGutter(gr, editor);
        insert(gutter);
    }

    drawView();
}

void TSyntaxEditWindow::handleEvent(TEvent &event)
{
    TWindow::handleEvent(event);
}

void TSyntaxEditWindow::close()
{
    if (editor && editor->isClipboard())
        hide();
    else
        TWindow::close();
}

const char *TSyntaxEditWindow::getTitle(short)
{
    static char titleBuf[512];
    if (editor && editor->fileName[0]) {
        const char *p = strrchr(editor->fileName, '/');
        if (!p) p = strrchr(editor->fileName, '\\');
        const char *name = p ? p + 1 : editor->fileName;
        const char *lang = editor->getLanguageName();
        snprintf(titleBuf, sizeof(titleBuf), "%s [%s]", name, lang);
        return titleBuf;
    }
    return "Untitled";
}
