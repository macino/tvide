#include "panels.h"
#include "app.h"

#include <tvterm/termctrl.h>
#include <tvterm/consts.h>
#include <tvterm/vtermemu.h>

#include <cstdio>
#include <cstring>

const tvterm::TVTermConstants TTerminalWindow::appConsts = {
    cmCheckTerminalUpdates,
    cmTerminalUpdated,
    cmGrabInput,
    cmReleaseInput,
    hcInputGrabbed
};

TTerminalWindow::TTerminalWindow(const TRect &bounds,
                                 tvterm::TerminalController &aTerm,
                                 int aNumber,
                                 const char *aTitle) noexcept
    : TWindowInit(&Super::initFrame),
      Super(bounds, aTerm, appConsts),
      termCtrl(aTerm),
      windowNumber(aNumber),
      customTitle(aTitle ? aTitle : "")
{
    options |= ofTileable;
}

void TTerminalWindow::sizeLimits(TPoint &min, TPoint &max)
{
    Super::sizeLimits(min, max);
    min.x = 20;
    min.y = 6;
}

const char *TTerminalWindow::getTitle(short maxSize)
{
    static char buf[128];
    if (!customTitle.empty()) {
        snprintf(buf, sizeof(buf), "%s %d", customTitle.c_str(), windowNumber);
        return buf;
    }
    snprintf(buf, sizeof(buf), "Terminal %d%s",
             windowNumber, isDisconnected() ? " (exited)" : "");
    return buf;
}

void TTerminalWindow::handleEvent(TEvent &ev)
{
    if (ev.what == evKeyDown && isDisconnected() &&
        !(state & (sfDragging | sfModal))) {
        close();
        return;
    }
    Super::handleEvent(ev);
}

static void termErrorCallback(const char *)
{
}

tvterm::TerminalController *TTerminalWindow::createController(TPoint size)
{
    static tvterm::VTermEmulatorFactory factory;
    return tvterm::TerminalController::create(size, factory, termErrorCallback);
}
