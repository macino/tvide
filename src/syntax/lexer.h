#ifndef TVIDE_SYNTAX_LEXER_H
#define TVIDE_SYNTAX_LEXER_H

#include "app.h"

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

// Token types for syntax highlighting
enum class TokenType {
    Normal,
    Keyword,
    Type,
    String,
    Number,
    Comment,
    Preprocessor,
    Operator,
    Bracket,
    Function,
    Variable,
    Tag,
    Attribute,
    AttrValue,
    Property,
    Selector,
    PseudoClass,
    HtmlEntity,
    PhpDelimiter,
    Heading,
    Bold,
    Link,
    Key,
    Value,
    Builtin,
};

// Token with position and type
struct Token {
    int start;
    int length;
    TokenType type;
};

// Color scheme for syntax tokens
struct SyntaxColors {
    TColorAttr normal;
    TColorAttr keyword;
    TColorAttr type;
    TColorAttr string;
    TColorAttr number;
    TColorAttr comment;
    TColorAttr preprocessor;
    TColorAttr oper;
    TColorAttr bracket;
    TColorAttr function;
    TColorAttr variable;
    TColorAttr tag;
    TColorAttr attribute;
    TColorAttr attrValue;
    TColorAttr property;
    TColorAttr selector;
    TColorAttr pseudoClass;
    TColorAttr htmlEntity;
    TColorAttr phpDelimiter;
    TColorAttr heading;
    TColorAttr bold;
    TColorAttr link;
    TColorAttr key;
    TColorAttr value;
    TColorAttr builtin;

    static SyntaxColors defaultColors();
    static SyntaxColors darkColors();
    static SyntaxColors classicColors();

    // Serialize: produce 25 hex bytes like "17 1F 1F 1E ..."
    std::string serialize() const;
    // Deserialize from the above format
    static SyntaxColors deserialize(const std::string &s);
};

// Named color theme
struct ColorTheme {
    std::string name;
    SyntaxColors colors;
    bool builtIn;  // built-in themes can't be deleted
};

// Theme manager: holds list of themes, loads/saves from ~/.config/tvide/themes.conf
class ThemeManager {
public:
    static ThemeManager &instance();

    const std::vector<ColorTheme> &themes() const { return themeList; }
    int currentIndex() const { return currentIdx; }
    void selectTheme(int idx);
    void addTheme(const std::string &name, const SyntaxColors &colors);
    void updateTheme(int idx, const SyntaxColors &colors);
    void deleteTheme(int idx);

    void load();
    void save();

private:
    ThemeManager();
    std::vector<ColorTheme> themeList;
    int currentIdx = 0;
    std::string configPath();
};

TColorAttr colorForToken(TokenType t, const SyntaxColors &colors);

// Number of configurable color entries and accessors for the dialog
constexpr int kNumColorEntries = 25;
const char *colorEntryName(int index);
TColorAttr &colorEntryRef(SyntaxColors &colors, int index);

// Lexer state carried between lines (for multi-line constructs)
enum class LexerState {
    Normal = 0,
    InBlockComment,
    InString,
    InHereDoc,
    InPhpBlock,
    InStyleBlock,
    InScriptBlock,
    InTemplateLiteral,
    InMultiLineString,
    // Embedded-language modes (cross-line)
    InHtmlScript,   // HTML/Vue: between <script> and </script>
    InHtmlStyle,    // HTML/Vue: between <style> and </style>
    InPhpHtml,      // PHP file: outside <?php blocks
    InMdCodeBlock,  // Markdown: inside ``` fenced code
};

// Base lexer class
class SyntaxLexer {
public:
    virtual ~SyntaxLexer() = default;

    // Tokenize a single line, given incoming state. Returns outgoing state.
    virtual LexerState tokenizeLine(
        const char *line, int length,
        LexerState inState,
        std::vector<Token> &tokens
    ) = 0;

    virtual const char *languageName() const = 0;

    static SyntaxLexer *createForFile(const std::string &filename);

protected:
    bool isWordChar(char c) const;
    bool matchKeyword(const char *line, int pos, int len,
                      const std::unordered_set<std::string> &keywords,
                      int &matchLen) const;
};

// Concrete lexer classes
class PhpLexer : public SyntaxLexer {
public:
    LexerState tokenizeLine(const char *line, int length,
                            LexerState inState, std::vector<Token> &tokens) override;
    const char *languageName() const override { return "PHP"; }
private:
    static const std::unordered_set<std::string> keywords;
    static const std::unordered_set<std::string> types;
    static const std::unordered_set<std::string> builtins;
};

class HtmlLexer : public SyntaxLexer {
public:
    LexerState tokenizeLine(const char *line, int length,
                            LexerState inState, std::vector<Token> &tokens) override;
    const char *languageName() const override { return "HTML"; }
};

class CssLexer : public SyntaxLexer {
public:
    LexerState tokenizeLine(const char *line, int length,
                            LexerState inState, std::vector<Token> &tokens) override;
    const char *languageName() const override { return "CSS"; }
private:
    static const std::unordered_set<std::string> properties;
    static const std::unordered_set<std::string> pseudoClasses;
};

class JavaScriptLexer : public SyntaxLexer {
public:
    LexerState tokenizeLine(const char *line, int length,
                            LexerState inState, std::vector<Token> &tokens) override;
    const char *languageName() const override { return "JavaScript"; }
private:
    static const std::unordered_set<std::string> keywords;
    static const std::unordered_set<std::string> types;
    static const std::unordered_set<std::string> builtins;
};

class TypeScriptLexer : public SyntaxLexer {
public:
    LexerState tokenizeLine(const char *line, int length,
                            LexerState inState, std::vector<Token> &tokens) override;
    const char *languageName() const override { return "TypeScript"; }
private:
    static const std::unordered_set<std::string> keywords;
    static const std::unordered_set<std::string> types;
};

class VueLexer : public SyntaxLexer {
public:
    LexerState tokenizeLine(const char *line, int length,
                            LexerState inState, std::vector<Token> &tokens) override;
    const char *languageName() const override { return "Vue"; }
};

class JsonLexer : public SyntaxLexer {
public:
    LexerState tokenizeLine(const char *line, int length,
                            LexerState inState, std::vector<Token> &tokens) override;
    const char *languageName() const override { return "JSON"; }
};

class MarkdownLexer : public SyntaxLexer {
public:
    LexerState tokenizeLine(const char *line, int length,
                            LexerState inState, std::vector<Token> &tokens) override;
    const char *languageName() const override { return "Markdown"; }
private:
    std::string codeLang; // current fenced-block language (empty if none / unknown)
};

class YamlLexer : public SyntaxLexer {
public:
    LexerState tokenizeLine(const char *line, int length,
                            LexerState inState, std::vector<Token> &tokens) override;
    const char *languageName() const override { return "YAML"; }
};

class SqlLexer : public SyntaxLexer {
public:
    LexerState tokenizeLine(const char *line, int length,
                            LexerState inState, std::vector<Token> &tokens) override;
    const char *languageName() const override { return "SQL"; }
private:
    static const std::unordered_set<std::string> keywords;
};

class XmlLexer : public SyntaxLexer {
public:
    LexerState tokenizeLine(const char *line, int length,
                            LexerState inState, std::vector<Token> &tokens) override;
    const char *languageName() const override { return "XML"; }
};

class PlainTextLexer : public SyntaxLexer {
public:
    LexerState tokenizeLine(const char *line, int length,
                            LexerState inState, std::vector<Token> &tokens) override;
    const char *languageName() const override { return "Text"; }
};

// ── C-family lexer (shared tokenizer, per-language keyword sets) ────────
class CLikeLexer : public SyntaxLexer {
public:
    LexerState tokenizeLine(const char *line, int length,
                            LexerState inState, std::vector<Token> &tokens) override;
protected:
    virtual const std::unordered_set<std::string> &keywords() const = 0;
    virtual const std::unordered_set<std::string> &types() const;
    virtual bool hasPreprocessor() const { return false; } // # directives at line start
};

#define DECL_CLIKE(Cls, Name) \
class Cls : public CLikeLexer { \
public: \
    const char *languageName() const override { return Name; } \
protected: \
    const std::unordered_set<std::string> &keywords() const override; \
    const std::unordered_set<std::string> &types() const override; \
};

DECL_CLIKE(CppLexer,    "C/C++")
DECL_CLIKE(JavaLexer,   "Java")
DECL_CLIKE(CSharpLexer, "C#")
DECL_CLIKE(GoLexer,     "Go")
DECL_CLIKE(RustLexer,   "Rust")
DECL_CLIKE(KotlinLexer, "Kotlin")
DECL_CLIKE(SwiftLexer,  "Swift")
DECL_CLIKE(LuaLexer,    "Lua")

class CppLexerWithPP : public CppLexer {
protected:
    bool hasPreprocessor() const override { return true; }
};

#undef DECL_CLIKE

// ── Python ─────────────────────────────────────────────────────────────
class PythonLexer : public SyntaxLexer {
public:
    LexerState tokenizeLine(const char *line, int length,
                            LexerState inState, std::vector<Token> &tokens) override;
    const char *languageName() const override { return "Python"; }
};

// ── Shell (sh/bash/zsh) ────────────────────────────────────────────────
class ShellLexer : public SyntaxLexer {
public:
    LexerState tokenizeLine(const char *line, int length,
                            LexerState inState, std::vector<Token> &tokens) override;
    const char *languageName() const override { return "Shell"; }
};

// ── Ruby ───────────────────────────────────────────────────────────────
class RubyLexer : public SyntaxLexer {
public:
    LexerState tokenizeLine(const char *line, int length,
                            LexerState inState, std::vector<Token> &tokens) override;
    const char *languageName() const override { return "Ruby"; }
};

// ── Dockerfile ─────────────────────────────────────────────────────────
class DockerfileLexer : public SyntaxLexer {
public:
    LexerState tokenizeLine(const char *line, int length,
                            LexerState inState, std::vector<Token> &tokens) override;
    const char *languageName() const override { return "Dockerfile"; }
};

// ── Makefile ───────────────────────────────────────────────────────────
class MakefileLexer : public SyntaxLexer {
public:
    LexerState tokenizeLine(const char *line, int length,
                            LexerState inState, std::vector<Token> &tokens) override;
    const char *languageName() const override { return "Makefile"; }
};

// ── INI / TOML / .env / .conf ──────────────────────────────────────────
class IniLexer : public SyntaxLexer {
public:
    LexerState tokenizeLine(const char *line, int length,
                            LexerState inState, std::vector<Token> &tokens) override;
    const char *languageName() const override { return "INI"; }
};

class TomlLexer : public IniLexer {
public:
    const char *languageName() const override { return "TOML"; }
};

class DotEnvLexer : public IniLexer {
public:
    const char *languageName() const override { return ".env"; }
};

#endif // TVIDE_SYNTAX_LEXER_H
