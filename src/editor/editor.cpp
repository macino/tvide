#include "editor.h"
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <string>
#include <sys/stat.h>

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
    eventMask |= evMouseWheel;
    std::string fn(aFileName.data(), aFileName.size());
    lexer.reset(SyntaxLexer::createForFile(fn));
    autoIndent = EditorSettings::instance().autoIndent;
    updateModTime();
}

TSyntaxEditor::~TSyntaxEditor() {}

// M10: Track file modification time
void TSyntaxEditor::updateModTime()
{
    if (fileName[0]) {
        struct stat st;
        if (stat(fileName, &st) == 0)
            lastModTime = st.st_mtime;
    }
}

bool TSyntaxEditor::hasExternalChange() const
{
    if (!fileName[0] || lastModTime == 0) return false;
    struct stat st;
    if (stat(fileName, &st) == 0)
        return st.st_mtime != lastModTime;
    return false;
}

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
    if (lexer && strcmp(lexer->languageName(), "PHP") == 0)
        state = LexerState::InPhpHtml; // PHP files start in HTML mode
    uint pos = 0;
    char lineBuf[4096];

    while (pos < bufLen) {
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
    if (lineNum <= 0) {
        if (lexer && strcmp(lexer->languageName(), "PHP") == 0)
            return LexerState::InPhpHtml;
        return LexerState::Normal;
    }
    if (lineStates.empty()) rebuildLineStates();
    if ((size_t)(lineNum - 1) < lineStates.size())
        return lineStates[lineNum - 1];
    return LexerState::Normal;
}

// ── Syntax-aware line drawing ──────────────────────────────────────────

void TSyntaxEditor::drawSyntaxLine(TDrawBuffer &b, uint linePtr,
                                   int hScroll, int width,
                                   TAttrPair baseColors, int lineNum,
                                   uint matchPos)
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

    // M11: Bracket highlight color — bright cyan on same background
    uint8_t biosNorm = normalColor.toBIOS();
    TColorAttr bracketHighlight((biosNorm & 0xF0) | 0x0B); // light cyan fg

    // Whitespace color: dark cyan on same background as normal text
    uint8_t biosNormal = normalColor.toBIOS();
    TColorAttr wsColor((biosNormal & 0xF0) | 0x03);

    // Compute which character offsets in this line should be bracket-highlighted
    int bracketHL1 = -1, bracketHL2 = -1;
    if (matchPos < bufLen) {
        uint lineEnd = linePtr + (uint)lineLen;
        if (curPtr >= linePtr && curPtr < lineEnd)
            bracketHL1 = (int)(curPtr - linePtr);
        if (matchPos >= linePtr && matchPos < lineEnd)
            bracketHL2 = (int)(matchPos - linePtr);
    }

    // Build screen buffer
    int pos = 0; // screen column position
    int x = 0;   // output column (after hScroll adjustment)

    int i = 0;
    while (i < lineLen) {
        unsigned char ch = (unsigned char)lineBuf[i];
        bool selected = ((uint)i >= selS && (uint)i < selE);
        bool isBracketHL = (i == bracketHL1 || i == bracketHL2);
        TColorAttr color = selected ? selColor
                           : isBracketHL ? bracketHighlight
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
            i++;
            continue;
        }

        if (ch < 0x80) {
            if (pos >= hScroll && x < width) {
                if (showWS && ch == ' ')
                    b.moveChar(x, settings.spaceChar,
                               selected ? selColor : wsColor, 1);
                else
                    b.moveChar(x, (char)ch, color, 1);
                x++;
            }
            pos++;
            i++;
            continue;
        }

        // UTF-8 multibyte: gather full sequence and emit as one moveStr.
        int seqLen = 1;
        if      ((ch & 0xE0) == 0xC0) seqLen = 2;
        else if ((ch & 0xF0) == 0xE0) seqLen = 3;
        else if ((ch & 0xF8) == 0xF0) seqLen = 4;
        if (i + seqLen > lineLen) seqLen = 1;
        for (int k = 1; k < seqLen; k++) {
            if (((unsigned char)lineBuf[i + k] & 0xC0) != 0x80) { seqLen = k; break; }
        }
        if (pos >= hScroll && x < width) {
            ushort drawn = b.moveStr(x, TStringView(lineBuf + i, seqLen), color);
            int colsAdvanced = drawn > 0 ? (int)drawn : 1;
            x += colsAdvanced;
            pos += colsAdvanced;
        } else {
            pos += 1;
        }
        i += seqLen;
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

    // M11: Find matching bracket position for highlight
    uint matchPos = settings.bracketMatch ? findMatchingBracket() : bufLen;

    TDrawBuffer b;
    TAttrPair color = getColor(0x0201);
    uint linePtr = drawPtr;
    int lineNum = delta.y;

    for (int y = 0; y < size.y; y++) {
        drawSyntaxLine(b, linePtr, delta.x, size.x, color, lineNum, matchPos);
        writeBuf(0, y, size.x, 1, b);
        linePtr = nextLine(linePtr);
        lineNum++;
    }
}

// ── Event handling ─────────────────────────────────────────────────────

void TSyntaxEditor::handleEvent(TEvent &event)
{
    // ── Track keystrokes for auto-suggest ──────────────────────────────
    if (event.what == evKeyDown) {
        ushort cks = event.keyDown.controlKeyState;
        bool isCtrl = (cks & (kbLeftCtrl | kbRightCtrl)) != 0;
        char ch = event.keyDown.charScan.charCode;
        if (!isCtrl &&
            ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
             (ch >= '0' && ch <= '9') || ch == '_')) {
            noteKeystroke();
            indexedFromText = false; // text will change; re-scan on next tick
        }
    }

    // ── Auto-suggest popup interception ────────────────────────────────
    if (suggestPopup && event.what == evKeyDown) {
        ushort kc = event.keyDown.keyCode;
        ushort cks = event.keyDown.controlKeyState;
        bool isCtrl = (cks & (kbLeftCtrl | kbRightCtrl)) != 0;

        // Letters/digits/underscore: extend prefix → refilter, don't intercept,
        // let editor insert the char then we'll re-trigger on idle.
        char ch = event.keyDown.charScan.charCode;
        bool isWordChar = !isCtrl &&
            ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
             (ch >= '0' && ch <= '9') || ch == '_');

        if (!isWordChar) {
            suggestPopup->handleEvent(event);
            if (suggestPopup->lastResult == TSuggestionPopup::ResAccepted) {
                std::string text = suggestPopup->acceptedText;
                closeSuggestPopup();
                acceptSuggestion(text);
                clearEvent(event);
                return;
            }
            if (suggestPopup->lastResult == TSuggestionPopup::ResCancelled) {
                closeSuggestPopup();
                clearEvent(event);
                return;
            }
            // Popup didn't consume; if event was Up/Down etc it would have.
            // Other keys (arrow horizontals, Home, etc.) — close popup, let
            // editor handle.
            if (kc == kbLeft || kc == kbRight || kc == kbHome || kc == kbEnd ||
                kc == kbDel || kc == kbBack || kc == kbCtrlLeft || kc == kbCtrlRight) {
                closeSuggestPopup();
                // fall through
            }
        }
    }

    // Ctrl-Space → manual completion
    if (event.what == evKeyDown) {
        ushort cks = event.keyDown.controlKeyState;
        bool ctrlHeld = (cks & (kbLeftCtrl | kbRightCtrl)) != 0;
        if (ctrlHeld && event.keyDown.charScan.charCode == 0 &&
            event.keyDown.keyCode == 0x0300) {
            manualSuggest();
            clearEvent(event);
            return;
        }
    }

    // Mouse wheel scrolling — scroll 3 lines, only when cursor is over us
    if (event.what == evMouseWheel) {
        TPoint mouse = makeLocal(event.mouse.where);
        if (mouse.x >= 0 && mouse.x < size.x && mouse.y >= 0 && mouse.y < size.y) {
            int lines = (event.mouse.wheel == mwUp) ? -3 : 3;
            scrollTo(delta.x, delta.y + lines);
            clearEvent(event);
            return;
        }
        return; // not over us — don't consume
    }

    // M5: Only invalidate line state cache on text-modifying keys, not navigation
    if (event.what == evKeyDown) {
        ushort kc = event.keyDown.keyCode;
        if (kc != kbUp && kc != kbDown && kc != kbLeft && kc != kbRight &&
            kc != kbHome && kc != kbEnd && kc != kbPgUp && kc != kbPgDn &&
            kc != kbShiftTab && kc != kbCtrlHome && kc != kbCtrlEnd &&
            kc != kbCtrlLeft && kc != kbCtrlRight &&
            !(event.keyDown.controlKeyState & (kbLeftCtrl | kbRightCtrl) &&
              (kc == kbCtrlC || kc == kbCtrlA)))
            lineStates.clear();
    }

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
                TFileEditor::handleEvent(event);
                return;
            }
        }

        // M9: Ctrl+/ → toggle line comment
        if (ctrl && !shift && event.keyDown.charScan.charCode == '/') {
            toggleLineComment();
            clearEvent(event);
            return;
        }

        // L10: Ctrl+D → duplicate line
        if (ctrl && !shift && keyCode == kbCtrlD) {
            duplicateLine();
            clearEvent(event);
            return;
        }

        // L11: Ctrl+Shift+K → delete line
        if (ctrl && shift && (event.keyDown.charScan.charCode == 'K' ||
                              event.keyDown.charScan.charCode == 'k' ||
                              event.keyDown.charScan.charCode == 11)) {
            deleteLine();
            clearEvent(event);
            return;
        }

        // L6: Tab → insert spaces when useTabs is false
        if (keyCode == kbTab && !ctrl && !shift && !EditorSettings::instance().useTabs) {
            int tabSz = EditorSettings::instance().tabSize;
            char spaces[16];
            memset(spaces, ' ', tabSz);
            insertText(spaces, tabSz, False);
            clearEvent(event);
            return;
        }

        // L12: Auto-closing brackets/quotes
        if (!ctrl && EditorSettings::instance().autoCloseBrackets) {
            char ch = event.keyDown.charScan.charCode;
            // For closing brackets/quotes, check if already at cursor and skip
            if (ch == ')' || ch == ']' || ch == '}' || ch == '"' || ch == '\'' || ch == '`') {
                if (curPtr < bufLen && bufChar(curPtr) == ch) {
                    setCurPtr(curPtr + 1, 0);
                    trackCursor(True);
                    clearEvent(event);
                    return;
                }
            }
            char closing = 0;
            switch (ch) {
            case '(': closing = ')'; break;
            case '[': closing = ']'; break;
            case '{': closing = '}'; break;
            case '"': closing = '"'; break;
            case '\'': closing = '\''; break;
            case '`': closing = '`'; break;
            }
            if (closing) {
                char pair[2] = {ch, closing};
                insertText(pair, 2, False);
                setCurPtr(curPtr - 1, 0);
                trackCursor(True);
                clearEvent(event);
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

    // Note keystroke time after a typed word-char (used by auto-suggest)
    if (event.what == evNothing) return;
    // event.what may have been cleared by parent; check the original
}

// ── Auto-suggest methods ────────────────────────────────────────────────

#include <chrono>

static long long nowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void TSyntaxEditor::noteKeystroke()
{
    lastKeystrokeMs = nowMs();
}

std::string TSyntaxEditor::currentWordPrefix(int maxLen) const
{
    std::string out;
    if (curPtr == 0) return out;
    uint p = curPtr;
    while (p > 0 && (int)out.size() < maxLen) {
        char ch = (p - 1 < (uint)curPtr) ? buffer[p - 1] : buffer[p - 1 + gapLen];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '_' || ch == '$') {
            out.insert(out.begin(), ch);
            p--;
        } else {
            break;
        }
    }
    // First char must be letter or underscore (not digit)
    if (!out.empty() && out[0] >= '0' && out[0] <= '9')
        out.clear();
    return out;
}

void TSyntaxEditor::closeSuggestPopup()
{
    if (!suggestPopup) return;
    if (owner) {
        owner->remove(suggestPopup);
        delete suggestPopup;
    }
    suggestPopup = nullptr;
    suggestPrefixLen = 0;
}

void TSyntaxEditor::acceptSuggestion(const std::string &text)
{
    if (text.empty() || (int)text.size() <= suggestPrefixLen) return;
    std::string suffix = text.substr(suggestPrefixLen);
    insertText(suffix.data(), (int)suffix.size(), False);
    trackCursor(True);
}

void TSyntaxEditor::tickAutoSuggest()
{
    auto &s = EditorSettings::instance();
    if (!s.autoSuggest) {
        if (suggestPopup) closeSuggestPopup();
        return;
    }
    if (lastKeystrokeMs == 0) return;
    if (nowMs() - lastKeystrokeMs < s.autoSuggestDelayMs) return;

    std::string prefix = currentWordPrefix();
    if ((int)prefix.size() < s.autoSuggestMinChars) {
        if (suggestPopup) closeSuggestPopup();
        return;
    }

    // Refresh local-text contributions if it's been a while
    if (!indexedFromText && bufLen > 0) {
        std::string text;
        text.reserve(bufLen);
        for (uint p = 0; p < bufLen; p++) {
            char c = (p < (uint)curPtr) ? buffer[p] : buffer[p + gapLen];
            text.push_back(c);
        }
        SuggestionIndex::instance().scanText(text);
        indexedFromText = true;
    }

    auto items = SuggestionIndex::instance().suggestions(prefix, 12);
    if (items.empty()) {
        if (suggestPopup) closeSuggestPopup();
        return;
    }

    // Position popup just below the cursor, in owner-window coordinates
    if (!owner) return;
    TPoint cursorGlobal = makeGlobal(cursor);
    TPoint cursorLocal  = owner->makeLocal(cursorGlobal);
    int ownerW = owner->size.x;
    int ownerH = owner->size.y;
    int w = 30;
    for (auto &it : items)
        if ((int)it.size() + 2 > w) w = std::min((int)it.size() + 2, 50);
    int h = std::min((int)items.size(), 8);
    int col = cursorLocal.x;
    int row = cursorLocal.y + 1;
    if (col + w > ownerW) col = std::max(0, ownerW - w);
    if (row + h > ownerH) row = std::max(0, cursorLocal.y - h);

    TRect r(col, row, col + w, row + h);
    if (!suggestPopup) {
        suggestPopup = new TSuggestionPopup(r);
        if (owner) owner->insert(suggestPopup);
    } else {
        suggestPopup->locate(r);
    }
    suggestPopup->setItems(items);
    suggestPrefixLen = (int)prefix.size();
}

void TSyntaxEditor::manualSuggest()
{
    lastKeystrokeMs = nowMs() - EditorSettings::instance().autoSuggestDelayMs - 1;
    indexedFromText = false;
    tickAutoSuggest();
}

// M9: Toggle line comment for current line
void TSyntaxEditor::toggleLineComment()
{
    const char *prefix = "// ";
    if (lexer) {
        const char *lang = lexer->languageName();
        if (strcmp(lang, "HTML") == 0 || strcmp(lang, "XML") == 0) return;
        if (strcmp(lang, "CSS") == 0) return;
        if (strcmp(lang, "YAML") == 0) prefix = "# ";
        if (strcmp(lang, "SQL") == 0) prefix = "-- ";
    }
    int prefixLen = strlen(prefix);

    // Find start of current line
    uint ls = curPtr;
    while (ls > 0) {
        char ch = bufChar(ls - 1);
        if (ch == '\n') break;
        ls--;
    }

    // Get line text
    char lineBuf[4096];
    int lineLen = getLineText(ls, lineBuf, sizeof(lineBuf) - 1);
    lineBuf[lineLen] = '\0';

    // Find first non-whitespace
    int firstNonWS = 0;
    while (firstNonWS < lineLen && (lineBuf[firstNonWS] == ' ' || lineBuf[firstNonWS] == '\t'))
        firstNonWS++;

    if (lineLen - firstNonWS >= prefixLen &&
        strncmp(lineBuf + firstNonWS, prefix, prefixLen) == 0) {
        // Remove comment prefix
        deleteRange(ls + firstNonWS, ls + firstNonWS + prefixLen, True);
    } else {
        // Add comment prefix
        setCurPtr(ls + firstNonWS, 0);
        insertText(prefix, prefixLen, False);
    }
    lineStates.clear();
    drawView();
}

// L10: Duplicate current line
void TSyntaxEditor::duplicateLine()
{
    uint ls = curPtr;
    while (ls > 0 && bufChar(ls - 1) != '\n')
        ls--;
    uint le = curPtr;
    while (le < bufLen && bufChar(le) != '\n')
        le++;

    std::string lineText;
    lineText += '\n';
    for (uint i = ls; i < le; i++)
        lineText += bufChar(i);

    setCurPtr(le, 0);
    insertText(lineText.c_str(), lineText.size(), False);

    lineStates.clear();
    drawView();
}

// L11: Delete current line
void TSyntaxEditor::deleteLine()
{
    uint ls = curPtr;
    while (ls > 0 && bufChar(ls - 1) != '\n')
        ls--;
    uint le = curPtr;
    while (le < bufLen && bufChar(le) != '\n')
        le++;
    if (le < bufLen) le++; // include the newline

    setCurPtr(ls, 0);
    setSelect(ls, le, True);
    deleteSelect();

    lineStates.clear();
    drawView();
}

// M11: Find matching bracket position (returns bufLen if not found)
uint TSyntaxEditor::findMatchingBracket()
{
    if (curPtr >= bufLen) return bufLen;
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
    default: return bufLen;
    }

    int depth = 1;
    uint pos = curPtr;
    int iterations = 0;
    while (iterations++ < 100000) {
        if (dir > 0) {
            pos = nextChar(pos);
            if (pos >= bufLen) return bufLen;
        } else {
            if (pos == 0) return bufLen;
            pos = prevChar(pos);
        }
        char c = bufChar(pos);
        if (c == ch) depth++;
        if (c == match) depth--;
        if (depth == 0) return pos;
    }
    return bufLen;
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
    int iterations = 0;
    const int maxIterations = 100000;
    while (iterations++ < maxIterations) {
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
    int dW = 70, dH = 22;
    if (TProgram::deskTop) {
        dW = std::min(dW, (int)TProgram::deskTop->size.x - 2);
        dH = std::min(dH, (int)TProgram::deskTop->size.y - 2);
    }
    auto *d = new TDialog(TRect(0, 0, dW, dH), "Options");
    d->options |= ofCentered;

    int colW = (dW - 6) / 2;
    int leftX = 3;
    int midX = leftX + colW + 1;

    d->insert(new TStaticText(TRect(leftX, 1, leftX + colW, 2), "─ Editor ─"));
    d->insert(new TCheckBoxes(TRect(leftX, 2, leftX + colW, 10),
        new TSItem("~L~ine numbers",
        new TSItem("~S~yntax highlighting",
        new TSItem("Show ~w~hitespace",
        new TSItem("Show ~E~OL markers",
        new TSItem("~A~uto indent",
        new TSItem("~B~racket matching",
        new TSItem("Use ~t~abs (uncheck = spaces)",
        new TSItem("Auto-~c~lose brackets", nullptr))))))))));

    auto *tabInput = new TInputLine(TRect(leftX + 13, 11, leftX + 19, 12), 4);
    d->insert(tabInput);
    d->insert(new TLabel(TRect(leftX, 11, leftX + 12, 12), "~T~ab size", tabInput));

    d->insert(new TStaticText(TRect(leftX, 13, leftX + colW, 14),
                              "─ Auto-suggest ─"));
    d->insert(new TCheckBoxes(TRect(leftX, 14, leftX + colW, 15),
        new TSItem("Enable auto-su~g~gest popup", nullptr)));
    auto *delayInput = new TInputLine(TRect(leftX + 13, 15, leftX + 21, 16), 6);
    d->insert(delayInput);
    d->insert(new TLabel(TRect(leftX, 15, leftX + 12, 16),
                         "Delay (~m~s)", delayInput));

    d->insert(new TStaticText(TRect(midX, 1, dW - 2, 2), "─ File tree sort ─"));
    d->insert(new TRadioButtons(TRect(midX, 2, dW - 2, 6),
        new TSItem("~N~ame asc",
        new TSItem("Name ~d~esc",
        new TSItem("D~a~te asc",
        new TSItem("Date des~c~", nullptr))))));
    d->insert(new TCheckBoxes(TRect(midX, 6, dW - 2, 7),
        new TSItem("~D~irs first", nullptr)));

    d->insert(new TStaticText(TRect(midX, 8, dW - 2, 9), "─ Structure sort ─"));
    d->insert(new TRadioButtons(TRect(midX, 9, dW - 2, 15),
        new TSItem("Li~n~e asc",
        new TSItem("L~i~ne desc",
        new TSItem("Na~m~e asc",
        new TSItem("Name d~e~sc",
        new TSItem("~K~ind asc",
        new TSItem("Kin~d~ desc", nullptr))))))));

    d->insert(new TButton(TRect(dW/2 - 11, dH - 3, dW/2 - 1, dH - 1),
                          "O~K~", cmOK, bfDefault));
    d->insert(new TButton(TRect(dW/2 + 1, dH - 3, dW/2 + 11, dH - 1),
                          "Cancel", cmCancel, bfNormal));

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
      TDialog(TRect(0, 0,
                    std::min(72, TProgram::deskTop ? (int)TProgram::deskTop->size.x - 2 : 72),
                    std::min(27, TProgram::deskTop ? (int)TProgram::deskTop->size.y - 2 : 27)),
              "Syntax Colors")
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
    if (!p) { TObject::destroy(d); return; }
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

