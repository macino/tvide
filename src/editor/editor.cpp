#include "editor.h"
#include <cstring>
#include <algorithm>
#include <cstdio>

// ── EditorSettings singleton ────────────────────────────────────────────

EditorSettings &EditorSettings::instance()
{
    static EditorSettings s;
    static bool init = false;
    if (!init) {
        s.colors = SyntaxColors::defaultColors();
        init = true;
    }
    return s;
}

// ── TSyntaxEditor ──────────────────────────────────────────────────────

TSyntaxEditor::TSyntaxEditor(const TRect &bounds,
                             TScrollBar *aHScrollBar,
                             TScrollBar *aVScrollBar,
                             TIndicator *aIndicator,
                             TStringView aFileName)
    : TFileEditor(bounds, aHScrollBar, aVScrollBar, aIndicator, aFileName)
{
    std::string fn(aFileName.data(), aFileName.size());
    lexer.reset(SyntaxLexer::createForFile(fn));
    autoIndent = EditorSettings::instance().autoIndent;
}

TSyntaxEditor::~TSyntaxEditor() {}

void TSyntaxEditor::setLexer(SyntaxLexer *newLexer)
{
    lexer.reset(newLexer);
    lineStates.clear();
    drawView();
}

const char *TSyntaxEditor::getLanguageName() const
{
    return lexer ? lexer->languageName() : "Text";
}

int TSyntaxEditor::getTotalLines() const
{
    int count = 1;
    for (uint i = 0; i < bufLen; i++) {
        char ch = (i < (uint)curPtr) ? buffer[i] : buffer[i + gapLen];
        if (ch == '\n') count++;
    }
    return count;
}

// ── Line text extraction from gap buffer ────────────────────────────────

int TSyntaxEditor::getLineText(uint lineStart, char *buf, int maxLen)
{
    int len = 0;
    uint pos = lineStart;
    while (pos < bufLen && len < maxLen) {
        char ch = (pos < (uint)curPtr) ? buffer[pos] : buffer[pos + gapLen];
        if (ch == '\r' || ch == '\n')
            break;
        buf[len++] = ch;
        pos++;
    }
    return len;
}

// ── Lexer state management ─────────────────────────────────────────────

void TSyntaxEditor::rebuildLineStates()
{
    lineStates.clear();
    if (!lexer || !buffer || bufLen == 0) return;

    LexerState state = LexerState::Normal;
    uint pos = 0;
    char lineBuf[4096];

    while (pos <= bufLen) {
        int lineLen = getLineText(pos, lineBuf, sizeof(lineBuf));
        std::vector<Token> tokens;
        state = lexer->tokenizeLine(lineBuf, lineLen, state, tokens);
        lineStates.push_back(state);

        // Advance past line content
        uint oldPos = pos;
        pos += lineLen;
        // Skip line ending
        if (pos < bufLen) {
            char ch = (pos < (uint)curPtr) ? buffer[pos] : buffer[pos + gapLen];
            if (ch == '\r') pos++;
            if (pos < bufLen) {
                ch = (pos < (uint)curPtr) ? buffer[pos] : buffer[pos + gapLen];
                if (ch == '\n') pos++;
            }
        } else {
            break;
        }
        // Safety: ensure forward progress
        if (pos <= oldPos) break;
    }
}

LexerState TSyntaxEditor::getStateForLine(int lineNum)
{
    if (lineNum <= 0) return LexerState::Normal;
    if (lineStates.empty()) rebuildLineStates();
    if ((size_t)(lineNum - 1) < lineStates.size())
        return lineStates[lineNum - 1];
    return LexerState::Normal;
}

// ── Syntax-aware line drawing ──────────────────────────────────────────

void TSyntaxEditor::drawSyntaxLine(TDrawBuffer &b, uint linePtr,
                                   int hScroll, int width,
                                   TAttrPair baseColors, int lineNum)
{
    auto &settings = EditorSettings::instance();
    bool doSyntax = settings.syntaxHighlight && lexer;
    bool showWS = settings.showWhitespace;
    int tabSz = settings.tabSize;

    hScroll = std::max(hScroll, 0);
    width = std::max(width, 0);

    // Extract line text
    char lineBuf[4096];
    int lineLen = getLineText(linePtr, lineBuf, sizeof(lineBuf));

    // Tokenize if syntax enabled
    const SyntaxColors &syntaxColors = settings.colors;
    std::vector<TColorAttr> colorMap;
    if (doSyntax) {
        LexerState inState = getStateForLine(lineNum);
        std::vector<Token> tokens;
        lexer->tokenizeLine(lineBuf, lineLen, inState, tokens);

        colorMap.resize(lineLen, syntaxColors.normal);
        for (auto &tok : tokens) {
            TColorAttr c = colorForToken(tok.type, syntaxColors);
            for (int i = tok.start; i < tok.start + tok.length && i < lineLen; i++)
                colorMap[i] = c;
        }
    }

    // Selection range (careful with unsigned arithmetic)
    uint selS = 0, selE = 0;
    if (selStart < selEnd) {
        uint lineEnd_ = linePtr + (uint)lineLen;
        if (selEnd > linePtr && selStart < lineEnd_) {
            selS = (selStart > linePtr) ? selStart - linePtr : 0;
            selE = (selEnd < lineEnd_) ? selEnd - linePtr : (uint)lineLen;
        }
    }

    TColorAttr normalColor = doSyntax ? syntaxColors.normal : TColorAttr(baseColors);
    TColorAttr selColor(baseColors >> 8);

    // Whitespace color: dark cyan on same background as normal text
    uint8_t biosNormal = normalColor.toBIOS();
    TColorAttr wsColor((biosNormal & 0xF0) | 0x03);

    // Build screen buffer
    int pos = 0; // screen column position
    int x = 0;   // output column (after hScroll adjustment)

    for (int i = 0; i < lineLen; i++) {
        char ch = lineBuf[i];
        bool selected = ((uint)i >= selS && (uint)i < selE);
        TColorAttr color = selected ? selColor
                           : (doSyntax ? colorMap[i] : normalColor);

        if (ch == '\t') {
            int tabStop = tabSz - (pos % tabSz);
            for (int t = 0; t < tabStop; t++) {
                if (pos >= hScroll && x < width) {
                    if (showWS && t == 0)
                        b.moveChar(x, settings.tabChar,
                                   selected ? selColor : wsColor, 1);
                    else
                        b.moveChar(x, ' ', color, 1);
                    x++;
                }
                pos++;
            }
        } else {
            if (pos >= hScroll && x < width) {
                if (showWS && ch == ' ')
                    b.moveChar(x, settings.spaceChar,
                               selected ? selColor : wsColor, 1);
                else
                    b.moveChar(x, ch, color, 1);
                x++;
            }
            pos++;
        }
    }

    // Show EOL marker if enabled
    if (settings.showEOL && x < width) {
        b.moveChar(x, settings.eolChar, wsColor, 1);
        x++;
    }

    // Fill remainder
    if (x < width) {
        TColorAttr fillColor = normalColor;
        // Check if selection extends past line end
        if (selStart < selEnd && selEnd > linePtr + (uint)lineLen &&
            selStart <= linePtr + (uint)lineLen)
            fillColor = selColor;
        b.moveChar(x, ' ', fillColor, width - x);
    }
}

// ── draw() override — the key method ───────────────────────────────────

void TSyntaxEditor::draw()
{
    auto &settings = EditorSettings::instance();

    if (!buffer || !settings.syntaxHighlight || !lexer) {
        TFileEditor::draw();
        return;
    }

    // Rebuild line states if needed (invalidated on edits)
    if (lineStates.empty())
        rebuildLineStates();

    // Compute drawPtr for the first visible line (same as TEditor::draw)
    if (drawLine != delta.y) {
        drawPtr = lineMove(drawPtr, delta.y - drawLine);
        drawLine = delta.y;
    }

    TDrawBuffer b;
    TAttrPair color = getColor(0x0201);
    uint linePtr = drawPtr;
    int lineNum = delta.y;

    for (int y = 0; y < size.y; y++) {
        drawSyntaxLine(b, linePtr, delta.x, size.x, color, lineNum);
        writeBuf(0, y, size.x, 1, b);
        linePtr = nextLine(linePtr);
        lineNum++;
    }
}

// ── Event handling ─────────────────────────────────────────────────────

void TSyntaxEditor::handleEvent(TEvent &event)
{
    // Invalidate line state cache on any text-modifying key
    if (event.what == evKeyDown)
        lineStates.clear();

    // Modern key bindings: intercept before TEditor's WordStar convertEvent
    if (event.what == evKeyDown) {
        ushort keyCode = event.keyDown.keyCode;
        ushort ctrlState = event.keyDown.controlKeyState;
        bool ctrl = (ctrlState & (kbLeftCtrl | kbRightCtrl)) != 0;
        bool shift = (ctrlState & kbShift) != 0;

        // Ctrl+C → Copy, Ctrl+X → Cut, Ctrl+V → Paste, Ctrl+Z → Undo
        if (ctrl && !shift) {
            ushort cmd = 0;
            switch (keyCode) {
            case kbCtrlC: cmd = cmCopy;  break;
            case kbCtrlX: cmd = cmCut;   break;
            case kbCtrlV: cmd = cmPaste; break;
            case kbCtrlZ: cmd = cmUndo;  break;
            }
            if (cmd) {
                event.what = evCommand;
                event.message.command = cmd;
                // Fall through to TFileEditor which handles these commands
                TFileEditor::handleEvent(event);
                return;
            }
        }

        // Arrow Left/Right without Shift collapses selection to start/end
        if (!shift && !ctrl && hasSelection()) {
            if (keyCode == kbLeft) {
                uint dest = selStart;
                hideSelect();
                setCurPtr(dest, 0);
                trackCursor(True);
                clearEvent(event);
                return;
            }
            if (keyCode == kbRight) {
                uint dest = selEnd;
                hideSelect();
                setCurPtr(dest, 0);
                trackCursor(True);
                clearEvent(event);
                return;
            }
            // Home/End without Shift also deselects first (standard behavior)
            if (keyCode == kbHome) {
                hideSelect();
                // Fall through to parent for line-start movement
            }
            if (keyCode == kbEnd) {
                hideSelect();
                // Fall through to parent for line-end movement
            }
        }
    }

    TFileEditor::handleEvent(event);
}

// ── Bracket matching ───────────────────────────────────────────────────

void TSyntaxEditor::matchBracket()
{
    if (curPtr >= bufLen) return;
    char ch = bufChar(curPtr);

    char match = 0;
    int dir = 0;
    switch (ch) {
    case '(': match = ')'; dir = 1; break;
    case ')': match = '('; dir = -1; break;
    case '[': match = ']'; dir = 1; break;
    case ']': match = '['; dir = -1; break;
    case '{': match = '}'; dir = 1; break;
    case '}': match = '{'; dir = -1; break;
    default: return;
    }

    int depth = 1;
    uint pos = curPtr;
    while (true) {
        if (dir > 0) {
            pos = nextChar(pos);
            if (pos >= bufLen) return;
        } else {
            if (pos == 0) return;
            pos = prevChar(pos);
        }
        char c = bufChar(pos);
        if (c == ch) depth++;
        if (c == match) depth--;
        if (depth == 0) {
            setCurPtr(pos, 0);
            trackCursor(True);
            return;
        }
    }
}

// ── Options dialog ─────────────────────────────────────────────────────

TDialog *createOptionsDialog()
{
    auto *d = new TDialog(TRect(0, 0, 42, 18), "Editor Options");
    d->options |= ofCentered;

    d->insert(new TCheckBoxes(TRect(3, 2, 39, 9),
        new TSItem("~L~ine numbers",
        new TSItem("~S~yntax highlighting",
        new TSItem("Show ~w~hitespace",
        new TSItem("Show ~E~OL markers",
        new TSItem("~A~uto indent",
        new TSItem("~B~racket matching",
        new TSItem("Use ~t~abs (uncheck = spaces)", nullptr)))))))));

    auto *tabInput = new TInputLine(TRect(16, 10, 22, 11), 4);
    d->insert(tabInput);
    d->insert(new TLabel(TRect(3, 10, 15, 11), "~T~ab size", tabInput));

    d->insert(new TButton(TRect(8, 14, 20, 16), "O~K~", cmOK, bfDefault));
    d->insert(new TButton(TRect(22, 14, 34, 16), "Cancel", cmCancel, bfNormal));

    d->selectNext(False);
    return d;
}

// ── Color preview view ────────────────────────────────────────────────

class TColorPreview : public TView {
public:
    TColorAttr color;
    TColorPreview(const TRect &bounds) : TView(bounds), color(0x17) {}
    void draw() override {
        TDrawBuffer b;
        b.moveStr(0, " Sample Text ", color);
        for (int i = 13; i < size.x; i++)
            b.moveChar(i, ' ', color, 1);
        writeBuf(0, 0, size.x, 1, b);
    }
};

// ── Color names for 16 BIOS colors ────────────────────────────────────

static const char *biosColorNames[16] = {
    "Black", "Blue", "Green", "Cyan",
    "Red", "Magenta", "Brown", "LtGrey",
    "DkGrey", "LtBlue", "LtGreen", "LtCyan",
    "LtRed", "LtMagenta", "Yellow", "White",
};

// ── Internal command IDs for the dialog ───────────────────────────────

static const ushort cmTokenChanged  = 3001;
static const ushort cmThemeChanged  = 3002;
static const ushort cmThemeSave     = 3010;
static const ushort cmThemeNew      = 3011;
static const ushort cmThemeDelete   = 3012;

// ── Theme list viewer ─────────────────────────────────────────────────

class TThemeList : public TListViewer {
public:
    TThemeList(const TRect &bounds, TScrollBar *sb)
        : TListViewer(bounds, 1, nullptr, sb)
    {
        updateRange();
    }
    void getText(char *dest, short item, short maxLen) override {
        auto &themes = ThemeManager::instance().themes();
        if (item >= 0 && item < (short)themes.size()) {
            strncpy(dest, themes[item].name.c_str(), maxLen);
            dest[maxLen - 1] = '\0';
        } else {
            dest[0] = '\0';
        }
    }
    void focusItem(short item) override {
        TListViewer::focusItem(item);
        message(owner, evBroadcast, cmThemeChanged, this);
    }
    void updateRange() {
        setRange((short)ThemeManager::instance().themes().size());
    }
};

// ── Token list viewer ─────────────────────────────────────────────────

class TTokenList : public TListViewer {
public:
    TTokenList(const TRect &bounds, TScrollBar *sb)
        : TListViewer(bounds, 1, nullptr, sb)
    {
        setRange(kNumColorEntries);
    }
    void getText(char *dest, short item, short maxLen) override {
        const char *name = colorEntryName(item);
        strncpy(dest, name, maxLen);
        dest[maxLen - 1] = '\0';
    }
    void focusItem(short item) override {
        TListViewer::focusItem(item);
        message(owner, evBroadcast, cmTokenChanged, this);
    }
};

// ── Colors dialog constructor ─────────────────────────────────────────

TSyntaxColorsDialog::TSyntaxColorsDialog()
    : TWindowInit(&TDialog::initFrame),
      TDialog(TRect(0, 0, 72, 27), "Syntax Colors")
{
    options |= ofCentered;
    editColors = EditorSettings::instance().colors;

    // ── Left column: Theme list (top) + Token list (bottom) ──

    // Theme list
    insert(new TLabel(TRect(2, 1, 10, 2), "~T~heme", nullptr));
    themeSB = new TScrollBar(TRect(24, 2, 25, 8));
    insert(themeSB);
    themeListBox = new TThemeList(TRect(2, 2, 24, 8), themeSB);
    insert(themeListBox);

    // Theme buttons
    insert(new TButton(TRect(2, 8, 10, 10), "~S~ave", cmThemeSave, bfNormal));
    insert(new TButton(TRect(10, 8, 17, 10), "~N~ew", cmThemeNew, bfNormal));
    insert(new TButton(TRect(17, 8, 25, 10), "~D~el", cmThemeDelete, bfNormal));

    // Token list
    insert(new TLabel(TRect(2, 10, 15, 11), "Token type", nullptr));
    tokenSB = new TScrollBar(TRect(24, 11, 25, 23));
    insert(tokenSB);
    tokenList = new TTokenList(TRect(2, 11, 24, 23), tokenSB);
    insert(tokenList);

    // ── Right column: FG radio, BG radio, preview ──

    // Foreground
    fgRadio = new TRadioButtons(TRect(27, 2, 48, 18),
        new TSItem(biosColorNames[0],
        new TSItem(biosColorNames[1],
        new TSItem(biosColorNames[2],
        new TSItem(biosColorNames[3],
        new TSItem(biosColorNames[4],
        new TSItem(biosColorNames[5],
        new TSItem(biosColorNames[6],
        new TSItem(biosColorNames[7],
        new TSItem(biosColorNames[8],
        new TSItem(biosColorNames[9],
        new TSItem(biosColorNames[10],
        new TSItem(biosColorNames[11],
        new TSItem(biosColorNames[12],
        new TSItem(biosColorNames[13],
        new TSItem(biosColorNames[14],
        new TSItem(biosColorNames[15], nullptr)))))))))))))))));
    insert(fgRadio);
    insert(new TLabel(TRect(27, 1, 42, 2), "~F~oreground", fgRadio));

    // Background
    bgRadio = new TRadioButtons(TRect(50, 2, 70, 10),
        new TSItem(biosColorNames[0],
        new TSItem(biosColorNames[1],
        new TSItem(biosColorNames[2],
        new TSItem(biosColorNames[3],
        new TSItem(biosColorNames[4],
        new TSItem(biosColorNames[5],
        new TSItem(biosColorNames[6],
        new TSItem(biosColorNames[7], nullptr)))))))));
    insert(bgRadio);
    insert(new TLabel(TRect(50, 1, 64, 2), "~B~ackground", bgRadio));

    // Preview
    insert(new TStaticText(TRect(50, 11, 58, 12), "Preview:"));
    preview = new TColorPreview(TRect(50, 12, 70, 13));
    insert(preview);

    // OK / Cancel
    insert(new TButton(TRect(50, 19, 60, 21), "O~K~", cmOK, bfDefault));
    insert(new TButton(TRect(61, 19, 71, 21), "Cancel", cmCancel, bfNormal));

    selectNext(False);

    // Focus current theme
    int curTheme = ThemeManager::instance().currentIndex();
    themeListBox->focusItem(curTheme);

    // Initialize FG/BG/preview for first token
    syncToSelection();
}

void TSyntaxColorsDialog::syncToSelection()
{
    int idx = tokenList->focused;
    TColorAttr &attr = colorEntryRef(editColors, idx);
    uint8_t bios = attr.toBIOS();
    ushort fg = bios & 0x0F;
    ushort bg = (bios >> 4) & 0x07;

    fgRadio->setData(&fg);
    fgRadio->drawView();
    bgRadio->setData(&bg);
    bgRadio->drawView();

    preview->color = attr;
    preview->drawView();
}

void TSyntaxColorsDialog::syncFromSelection()
{
    int idx = tokenList->focused;
    ushort fg = 0, bg = 0;
    fgRadio->getData(&fg);
    bgRadio->getData(&bg);
    uint8_t bios = ((bg & 0x07) << 4) | (fg & 0x0F);
    colorEntryRef(editColors, idx) = TColorAttr(bios);

    preview->color = TColorAttr(bios);
    preview->drawView();
}

void TSyntaxColorsDialog::loadTheme(int idx)
{
    auto &themes = ThemeManager::instance().themes();
    if (idx >= 0 && idx < (int)themes.size()) {
        editColors = themes[idx].colors;
        syncToSelection();
    }
}

void TSyntaxColorsDialog::saveTheme()
{
    int idx = themeListBox->focused;
    auto &tm = ThemeManager::instance();
    auto &themes = tm.themes();
    if (idx >= 0 && idx < (int)themes.size()) {
        tm.updateTheme(idx, editColors);
        tm.save();
    }
}

void TSyntaxColorsDialog::newTheme()
{
    auto *d = new TDialog(TRect(0, 0, 40, 8), "New Theme");
    d->options |= ofCentered;
    auto *inp = new TInputLine(TRect(3, 3, 35, 4), 64);
    d->insert(inp);
    d->insert(new TLabel(TRect(2, 2, 20, 3), "Theme ~n~ame", inp));
    d->insert(new TButton(TRect(10, 5, 20, 7), "O~K~", cmOK, bfDefault));
    d->insert(new TButton(TRect(22, 5, 34, 7), "Cancel", cmCancel, bfNormal));
    d->selectNext(False);

    char buf[65] = {};
    TView *p = TProgram::application->validView(d);
    if (!p) return;
    ushort result = TProgram::deskTop->execView(p);
    if (result == cmOK)
        p->getData(buf);
    TObject::destroy(p);

    if (result != cmOK || buf[0] == '\0') return;

    std::string name = buf;
    ThemeManager::instance().addTheme(name, editColors);
    ThemeManager::instance().save();

    themeListBox->updateRange();
    themeListBox->focusItem((short)(ThemeManager::instance().themes().size() - 1));
    themeListBox->drawView();
}

void TSyntaxColorsDialog::deleteTheme()
{
    int idx = themeListBox->focused;
    auto &themes = ThemeManager::instance().themes();
    if (idx < 0 || idx >= (int)themes.size()) return;
    if (themes[idx].builtIn) {
        messageBox("Cannot delete built-in themes.", mfError | mfOKButton);
        return;
    }

    ThemeManager::instance().deleteTheme(idx);
    ThemeManager::instance().save();

    themeListBox->updateRange();
    if (themeListBox->focused >= (short)themes.size())
        themeListBox->focusItem((short)themes.size() - 1);
    themeListBox->drawView();
    loadTheme(themeListBox->focused);
}

void TSyntaxColorsDialog::handleEvent(TEvent &event)
{
    if (event.what == evBroadcast) {
        if (event.message.command == cmTokenChanged) {
            syncToSelection();
            clearEvent(event);
            return;
        }
        if (event.message.command == cmThemeChanged) {
            loadTheme(themeListBox->focused);
            clearEvent(event);
            return;
        }
    }

    if (event.what == evCommand) {
        switch (event.message.command) {
        case cmThemeSave:   saveTheme();   clearEvent(event); return;
        case cmThemeNew:    newTheme();    clearEvent(event); return;
        case cmThemeDelete: deleteTheme(); clearEvent(event); return;
        default: break;
        }
    }

    TDialog::handleEvent(event);

    // Keep preview in sync after radio button changes
    syncFromSelection();
}

int TSyntaxColorsDialog::selectedThemeIndex() const
{
    return themeListBox->focused;
}

