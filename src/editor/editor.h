#ifndef TVIDE_EDITOR_H
#define TVIDE_EDITOR_H

#include "app.h"
#include "syntax/lexer.h"
#include <memory>
#include <vector>

// Forward declarations
class TLineGutter;

// Editor display settings (shared across all editors)
struct EditorSettings {
    bool showLineNumbers   = true;
    bool showWhitespace    = false;   // show spaces/tabs as visible chars
    bool showEOL           = false;   // show end-of-line markers
    bool syntaxHighlight   = true;
    bool autoIndent        = true;
    bool bracketMatch      = true;
    int  tabSize           = 4;
    bool useTabs           = true;    // false = insert spaces
    char tabChar            = '\xAF'; // » visible tab
    char spaceChar          = '\xFA'; // · visible space
    char eolChar            = '\xAE'; // ¶ visible EOL

    SyntaxColors colors;              // current color scheme

    static EditorSettings &instance();
};

// Syntax-highlighting editor that overrides draw()
class TSyntaxEditor : public TFileEditor {
public:
    TSyntaxEditor(const TRect &bounds,
                  TScrollBar *aHScrollBar,
                  TScrollBar *aVScrollBar,
                  TIndicator *aIndicator,
                  TStringView aFileName);
    virtual ~TSyntaxEditor();

    virtual void handleEvent(TEvent &event) override;
    virtual void draw() override;

    void setLexer(SyntaxLexer *newLexer);
    SyntaxLexer *getLexer() const { return lexer.get(); }
    const char *getLanguageName() const;

    void matchBracket();

    // Accessors for line number gutter
    int getTopLine() const { return delta.y; }
    int getTotalLines() const;

private:
    std::unique_ptr<SyntaxLexer> lexer;

    // Cache lexer state per line for multi-line constructs
    std::vector<LexerState> lineStates;
    void rebuildLineStates();
    LexerState getStateForLine(int lineNum);

    // Draw one line into a TDrawBuffer with syntax coloring
    void drawSyntaxLine(TDrawBuffer &b, uint linePtr, int hScroll,
                        int width, TAttrPair baseColors, int lineNum);

    // Helper: get line text from gap buffer
    int getLineText(uint lineStart, char *buf, int maxLen);
};

// Editor window wrapping TSyntaxEditor with optional line number gutter
class TSyntaxEditWindow : public TWindow {
public:
    TSyntaxEditWindow(const TRect &bounds,
                      const char *fileName,
                      int aNumber);
    virtual ~TSyntaxEditWindow();

    virtual void handleEvent(TEvent &event) override;
    virtual void close() override;
    virtual const char *getTitle(short maxSize) override;

    TSyntaxEditor *getEditor() const { return editor; }
    TLineGutter *getGutter() const { return gutter; }
    void relayout(); // recalculate gutter + editor sizes

private:
    TSyntaxEditor *editor;
    TIndicator *indicator;
    TScrollBar *hScrollBar;
    TScrollBar *vScrollBar;

    TLineGutter *gutter;

    void createContents(const char *fileName);
};

// Line number gutter view drawn at the left of the editor
class TLineGutter : public TView {
public:
    TLineGutter(const TRect &bounds, TSyntaxEditor *anEditor);
    virtual void draw() override;

    int gutterWidth() const; // number of columns needed

private:
    TSyntaxEditor *editor;
};

// Options dialog
TDialog *createOptionsDialog();

// Colors dialog - manages themes and per-token color editing.
// After execView returns cmOK, colors have been applied.
class TSyntaxColorsDialog : public TDialog {
public:
    SyntaxColors editColors; // working copy

    TSyntaxColorsDialog();
    void handleEvent(TEvent &event) override;
    int selectedThemeIndex() const;

private:
    class TThemeList   *themeListBox;
    class TTokenList   *tokenList;
    TRadioButtons      *fgRadio;
    TRadioButtons      *bgRadio;
    class TColorPreview *preview;
    TScrollBar         *themeSB;
    TScrollBar         *tokenSB;

    void syncToSelection();
    void syncFromSelection();
    void loadTheme(int idx);
    void saveTheme();
    void newTheme();
    void deleteTheme();
};

#endif // TVIDE_EDITOR_H
