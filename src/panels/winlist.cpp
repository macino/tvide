#include "panels.h"
#include "app.h"
#include <algorithm>
#include <cstring>

TWindowListPanel::TWindowListPanel(const TRect &bounds)
    : TWindowInit(&TWindowListPanel::initFrame),
      TWindow(bounds, "Windows", 0),
      items(nullptr)
{
    flags = 0;
    growMode = 0;
    state &= ~sfShadow;
    options |= ofFirstClick;
    options &= ~ofTopSelect;
    eventMask |= evMouseWheel | evBroadcast;

    TRect r = getExtent();
    r.grow(-1, -1);

    TRect sr = getExtent();
    sr.a.x = sr.b.x - 1;
    sr.a.y = 1;
    sr.b.y--;
    scrollBar = new TScrollBar(sr);
    scrollBar->growMode = gfGrowLoX | gfGrowHiX | gfGrowHiY;
    insert(scrollBar);

    r.b.x--;
    listBox = new TListBox(r, 1, scrollBar);
    listBox->growMode = gfGrowHiX | gfGrowHiY;
    insert(listBox);

    items = new TUnsortedStringCollection(20, 5);
    listBox->newList(items);
}

TWindowListPanel::~TWindowListPanel()
{
    items = nullptr;
}

TPalette &TWindowListPanel::getPalette() const
{
    static char customPal[] =
        "\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F"
        "\x30\x31\x32\x33\x34\x35\x36\x37\x38"
        "\x20\x06\x21\x20"
        "\x3D\x3E\x3F";
    static TPalette pal(customPal, sizeof(customPal) - 1);
    return pal;
}

void TWindowListPanel::refresh()
{
    int prevFocus = listBox->focused;
    windows.clear();

    auto *desk = TProgram::deskTop;
    if (desk) {
        TView *p = desk->first();
        if (p) {
            TView *t = p;
            do {
                auto *w = dynamic_cast<TWindow *>(t);
                if (w) {
                    bool isPanel = false;
                    if (dynamic_cast<TMessagePanel *>(w)) isPanel = true;
                    if (!isPanel)
                        windows.push_back(w);
                }
                t = t->next;
            } while (t != p);
        }
    }
    std::reverse(windows.begin(), windows.end()); // Z-order top-to-bottom

    auto *newItems = new TUnsortedStringCollection((ccIndex)windows.size() + 1, 5);
    for (auto *w : windows) {
        const char *icon = "📄";
        if (dynamic_cast<TTerminalWindow *>(w)) icon = "💻";
        const char *t = w->getTitle(120);
        char line[256];
        snprintf(line, sizeof(line), "%s %s", icon, t ? t : "Untitled");
        newItems->insert(newStr(line));
    }
    listBox->newList(newItems);
    items = newItems;

    int cnt = (int)windows.size();
    if (cnt > 0) {
        if (prevFocus < 0 || prevFocus >= cnt) prevFocus = 0;
        listBox->focusItem(prevFocus);
    }
    drawView();
}

void TWindowListPanel::activateFocused()
{
    int idx = listBox->focused;
    if (idx < 0 || idx >= (int)windows.size()) return;
    TWindow *w = windows[idx];
    if (w) w->select();
}

void TWindowListPanel::closeFocused()
{
    int idx = listBox->focused;
    if (idx < 0 || idx >= (int)windows.size()) return;
    TWindow *w = windows[idx];
    if (w) {
        w->close();
        refresh();
    }
}

void TWindowListPanel::handleEvent(TEvent &event)
{
    if (event.what == evBroadcast && event.message.command == cmWinListChanged) {
        refresh();
        clearEvent(event);
        return;
    }

    if (event.what == evMouseDown) {
        TPoint mouse = makeLocal(event.mouse.where);
        if (mouse.x == size.x - 1) {
            TPoint lastMouse = event.mouse.where;
            while (mouseEvent(event, evMouseMove)) {
                int dx = event.mouse.where.x - lastMouse.x;
                if (dx != 0) {
                    int newWidth = size.x + dx;
                    if (newWidth >= 15 && newWidth <= 60) {
                        TRect r = getBounds();
                        r.b.x = r.a.x + newWidth;
                        changeBounds(r);
                    }
                    lastMouse = event.mouse.where;
                }
            }
            clearEvent(event);
            return;
        }
    }

    if (event.what == evMouseWheel) {
        TPoint mouse = makeLocal(event.mouse.where);
        if (mouse.x < 0 || mouse.x >= size.x || mouse.y < 0 || mouse.y >= size.y)
            return;
        if (listBox->range <= 0) { clearEvent(event); return; }
        int delta = (event.mouse.wheel == mwUp) ? -3 : 3;
        int newFocus = std::max(0, std::min((int)listBox->range - 1,
                                             listBox->focused + delta));
        listBox->focusItem(newFocus);
        listBox->drawView();
        clearEvent(event);
        return;
    }

    if (event.what == evKeyDown) {
        switch (event.keyDown.keyCode) {
        case kbEnter:
            activateFocused();
            clearEvent(event);
            return;
        case kbDel:
            closeFocused();
            clearEvent(event);
            return;
        case kbUp:
            if (listBox->focused > 0) {
                listBox->focusItem(listBox->focused - 1);
                listBox->drawView();
            }
            clearEvent(event);
            return;
        case kbDown:
            if (listBox->focused < listBox->range - 1) {
                listBox->focusItem(listBox->focused + 1);
                listBox->drawView();
            }
            clearEvent(event);
            return;
        case kbHome:
            listBox->focusItem(0);
            listBox->drawView();
            clearEvent(event);
            return;
        case kbEnd:
            listBox->focusItem(listBox->range - 1);
            listBox->drawView();
            clearEvent(event);
            return;
        }
    }

    TWindow::handleEvent(event);

    if (event.what == evBroadcast &&
        event.message.command == cmListItemSelected) {
        activateFocused();
        clearEvent(event);
    }
}

void TWindowListPanel::changeBounds(const TRect &bounds)
{
    TWindow::changeBounds(bounds);
    message(TProgram::application, evBroadcast, cmRelayout, this);
}

void TWindowListPanel::sizeLimits(TPoint &min, TPoint &max)
{
    TWindow::sizeLimits(min, max);
    min.x = 15;
    max.x = 60;
}
