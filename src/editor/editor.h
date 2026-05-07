#ifndef TVIDE_EDITOR_H
#define TVIDE_EDITOR_H

#include "app.h"
#include "syntax/lexer.h"
#include <memory>
#include <vector>
#include <string>
#include <unordered_set>

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
    bool autoCloseBrackets = true;    // auto-insert matching brackets/quotes

    bool autoSuggest        = true;   // popup completions while typing
    int  autoSuggestDelayMs = 250;    // idle time before popup appears (ms)
    int  autoSuggestMinChars = 2;     // need this many word chars to trigger

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
    void toggleLineComment();
    void duplicateLine();
    void deleteLine();
    uint findMatchingBracket();

    // M10: External file change detection
    time_t lastModTime = 0;
    void updateModTime();
    bool hasExternalChange() const;

    // Accessors for line number gutter
    int getTopLine() const { return delta.y; }
    int getTotalLines() const;

    // Auto-suggest hooks (called from app->idle() and key events)
    void noteKeystroke();          // call after each typed character
    void tickAutoSuggest();        // call from idle to maybe trigger popup
    void manualSuggest();          // Ctrl-Space — force trigger
    void closeSuggestPopup();
    void acceptSuggestion(const std::string &text);
    bool suggestPopupActive() const { return suggestPopup != nullptr; }

private:
    std::unique_ptr<SyntaxLexer> lexer;
    long long lastKeystrokeMs = 0;
    bool indexedFromText = false;
    class TSuggestionPopup *suggestPopup = nullptr;
    int suggestPrefixLen = 0;

    std::string currentWordPrefix(int maxLen = 64) const;

    // Cache lexer state per line for multi-line constructs
    std::vector<LexerState> lineStates;
    void rebuildLineStates();
    LexerState getStateForLine(int lineNum);

    // Draw one line into a TDrawBuffer with syntax coloring
    void drawSyntaxLine(TDrawBuffer &b, uint linePtr, int hScroll,
                        int width, TAttrPair baseColors, int lineNum,
                        uint matchPos = 0xFFFFFFFF);

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

// ── Auto-suggest infrastructure ─────────────────────────────────────────

// Singleton holding identifiers we've seen across project + open files.
class SuggestionIndex {
public:
    static SuggestionIndex &instance();

    // Add identifiers from a chunk of source text.
    void scanText(const std::string &text);

    // Scan a project tree (limited by ProjectManager) once and cache.
    void scanProject(const std::string &rootPath);

    // Add words from a lexer's keyword/type/builtin sets, given a filename.
    void addLexerWords(const std::string &filename);

    // Get up to maxResults suggestions starting with the given prefix.
    // Sorted alphabetically. Case-insensitive prefix match, exact-case results.
    std::vector<std::string> suggestions(const std::string &prefix,
                                         size_t maxResults = 100) const;

    void clear();

private:
    SuggestionIndex() = default;
    std::unordered_set<std::string> words_;
    bool projectScanned_ = false;
    std::string lastProjectRoot_;
};

// Lightweight word-list popup view. Lives as a child of the editor window,
// positioned just below the cursor. Handles Up/Down/Enter/Esc only —
// any other key returns control to the editor (closes popup).
class TSuggestionPopup : public TView {
public:
    enum Result { ResNone, ResAccepted, ResCancelled };

    TSuggestionPopup(const TRect &bounds);
    void setItems(const std::vector<std::string> &items);
    void draw() override;
    void handleEvent(TEvent &event) override;

    Result lastResult = ResNone;
    std::string acceptedText;

private:
    std::vector<std::string> items;
    int focused = 0;
    int topItem = 0;
    int rowsVisible() const { return std::max(1, (int)size.y); }
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
