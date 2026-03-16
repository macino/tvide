#include "lexer.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

// ── Helper methods ──────────────────────────────────────────────────────

bool SyntaxLexer::isWordChar(char c) const
{
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           c == '_' || c == '$';
}

bool SyntaxLexer::matchKeyword(const char *line, int pos, int len,
                               const std::unordered_set<std::string> &keywords,
                               int &matchLen) const
{
    int end = pos;
    while (end < len && isWordChar(line[end]))
        end++;
    matchLen = end - pos;
    if (matchLen == 0) return false;
    std::string word(line + pos, matchLen);
    return keywords.count(word) > 0;
}

// ── File extension → lexer mapping ──────────────────────────────────────

SyntaxLexer *SyntaxLexer::createForFile(const std::string &filename)
{
    if (filename.empty())
        return new PlainTextLexer();

    // Extract extension
    auto dot = filename.rfind('.');
    std::string ext;
    if (dot != std::string::npos) {
        ext = filename.substr(dot + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    // Also check for special filenames
    auto slash = filename.rfind('/');
    std::string basename = (slash != std::string::npos)
        ? filename.substr(slash + 1)
        : filename;

    if (ext == "php" || ext == "phtml" || ext == "php3" ||
        ext == "php4" || ext == "php5" || ext == "php7" || ext == "phps")
        return new PhpLexer();
    if (ext == "html" || ext == "htm" || ext == "xhtml")
        return new HtmlLexer();
    if (ext == "css" || ext == "scss" || ext == "less" || ext == "sass")
        return new CssLexer();
    if (ext == "js" || ext == "jsx" || ext == "mjs" || ext == "cjs")
        return new JavaScriptLexer();
    if (ext == "ts" || ext == "tsx")
        return new TypeScriptLexer();
    if (ext == "vue")
        return new VueLexer();
    if (ext == "json" || ext == "jsonc" || ext == "json5")
        return new JsonLexer();
    if (ext == "md" || ext == "markdown" || ext == "mkd")
        return new MarkdownLexer();
    if (ext == "yml" || ext == "yaml")
        return new YamlLexer();
    if (ext == "sql" || ext == "mysql" || ext == "pgsql")
        return new SqlLexer();
    if (ext == "xml" || ext == "xsl" || ext == "xslt" || ext == "svg")
        return new XmlLexer();
    if (ext == "twig" || ext == "blade" || ext == "latte")
        return new HtmlLexer();  // Template engines → HTML-ish

    return new PlainTextLexer();
}

// ── Default color scheme ───────────────────────────────────────────────
// Classic Borland IDE style: blue background with bright syntax colors
// Matches the look of Turbo C, Borland C++, and Turbo Pascal editors.
// BIOS color byte: high nibble = background, low nibble = foreground.
//   0 = black, 1 = blue, 2 = green, 3 = cyan, 4 = red, 5 = magenta,
//   6 = brown, 7 = light grey, 8 = dark grey, 9 = light blue,
//   A = light green, B = light cyan, C = light red, D = light magenta,
//   E = yellow, F = bright white

SyntaxColors SyntaxColors::defaultColors()
{
    SyntaxColors c{};
    c.normal       = {0x17}; // light grey on blue
    c.keyword      = {0x1F}; // bright white on blue
    c.type         = {0x1F}; // bright white on blue
    c.string       = {0x1E}; // yellow on blue
    c.number       = {0x1A}; // light green on blue
    c.comment      = {0x13}; // dark cyan on blue
    c.preprocessor = {0x1A}; // light green on blue
    c.oper         = {0x1E}; // yellow on blue
    c.bracket      = {0x1E}; // yellow on blue
    c.function     = {0x1F}; // bright white on blue
    c.variable     = {0x1B}; // light cyan on blue
    c.tag          = {0x1F}; // bright white on blue
    c.attribute    = {0x1E}; // yellow on blue
    c.attrValue    = {0x1E}; // yellow on blue
    c.property     = {0x1B}; // light cyan on blue
    c.selector     = {0x1F}; // bright white on blue
    c.pseudoClass  = {0x1D}; // light magenta on blue
    c.htmlEntity   = {0x1D}; // light magenta on blue
    c.phpDelimiter = {0x1C}; // light red on blue
    c.heading      = {0x1F}; // bright white on blue
    c.bold         = {0x1F}; // bright white on blue
    c.link         = {0x1B}; // light cyan on blue
    c.key          = {0x1F}; // bright white on blue
    c.value        = {0x1E}; // yellow on blue
    c.builtin      = {0x1D}; // light magenta on blue
    return c;
}

// ── Dark color scheme (black background) ───────────────────────────────

SyntaxColors SyntaxColors::darkColors()
{
    SyntaxColors c{};
    c.normal       = {0x07}; // light grey on black
    c.keyword      = {0x0F}; // bright white on black
    c.type         = {0x0B}; // light cyan on black
    c.string       = {0x0A}; // light green on black
    c.number       = {0x0D}; // light magenta on black
    c.comment      = {0x08}; // dark grey on black
    c.preprocessor = {0x0E}; // yellow on black
    c.oper         = {0x07}; // light grey on black
    c.bracket      = {0x0F}; // bright white on black
    c.function     = {0x0E}; // yellow on black
    c.variable     = {0x0B}; // light cyan on black
    c.tag          = {0x09}; // light blue on black
    c.attribute    = {0x0A}; // light green on black
    c.attrValue    = {0x0C}; // light red on black
    c.property     = {0x0B}; // light cyan on black
    c.selector     = {0x0E}; // yellow on black
    c.pseudoClass  = {0x0D}; // light magenta on black
    c.htmlEntity   = {0x0D}; // light magenta on black
    c.phpDelimiter = {0x0C}; // light red on black
    c.heading      = {0x0F}; // bright white on black
    c.bold         = {0x0F}; // bright white on black
    c.link         = {0x09}; // light blue on black
    c.key          = {0x0E}; // yellow on black
    c.value        = {0x0A}; // light green on black
    c.builtin      = {0x0D}; // light magenta on black
    return c;
}

// ── Classic scheme (cyan background, like early Turbo Pascal) ──────────

SyntaxColors SyntaxColors::classicColors()
{
    SyntaxColors c{};
    c.normal       = {0x30}; // black on cyan
    c.keyword      = {0x3F}; // bright white on cyan
    c.type         = {0x3E}; // yellow on cyan
    c.string       = {0x34}; // red on cyan
    c.number       = {0x35}; // magenta on cyan
    c.comment      = {0x37}; // light grey on cyan
    c.preprocessor = {0x32}; // green on cyan
    c.oper         = {0x30}; // black on cyan
    c.bracket      = {0x30}; // black on cyan
    c.function     = {0x31}; // blue on cyan
    c.variable     = {0x30}; // black on cyan
    c.tag          = {0x31}; // blue on cyan
    c.attribute    = {0x34}; // red on cyan
    c.attrValue    = {0x34}; // red on cyan
    c.property     = {0x31}; // blue on cyan
    c.selector     = {0x3F}; // bright white on cyan
    c.pseudoClass  = {0x35}; // magenta on cyan
    c.htmlEntity   = {0x35}; // magenta on cyan
    c.phpDelimiter = {0x3C}; // light red on cyan
    c.heading      = {0x3F}; // bright white on cyan
    c.bold         = {0x3F}; // bright white on cyan
    c.link         = {0x31}; // blue on cyan
    c.key          = {0x3E}; // yellow on cyan
    c.value        = {0x34}; // red on cyan
    c.builtin      = {0x35}; // magenta on cyan
    return c;
}

TColorAttr colorForToken(TokenType t, const SyntaxColors &colors)
{
    switch (t) {
    case TokenType::Keyword:      return colors.keyword;
    case TokenType::Type:         return colors.type;
    case TokenType::String:       return colors.string;
    case TokenType::Number:       return colors.number;
    case TokenType::Comment:      return colors.comment;
    case TokenType::Preprocessor: return colors.preprocessor;
    case TokenType::Operator:     return colors.oper;
    case TokenType::Bracket:      return colors.bracket;
    case TokenType::Function:     return colors.function;
    case TokenType::Variable:     return colors.variable;
    case TokenType::Tag:          return colors.tag;
    case TokenType::Attribute:    return colors.attribute;
    case TokenType::AttrValue:    return colors.attrValue;
    case TokenType::Property:     return colors.property;
    case TokenType::Selector:     return colors.selector;
    case TokenType::PseudoClass:  return colors.pseudoClass;
    case TokenType::HtmlEntity:   return colors.htmlEntity;
    case TokenType::PhpDelimiter: return colors.phpDelimiter;
    case TokenType::Heading:      return colors.heading;
    case TokenType::Bold:         return colors.bold;
    case TokenType::Link:         return colors.link;
    case TokenType::Key:          return colors.key;
    case TokenType::Value:        return colors.value;
    case TokenType::Builtin:      return colors.builtin;
    default:                      return colors.normal;
    }
}

// ── Index-based color entry access (for the Colors dialog) ─────────────

static const char *colorNames[kNumColorEntries] = {
    "Normal text",    "Keyword",       "Type",          "String",
    "Number",         "Comment",       "Preprocessor",  "Operator",
    "Bracket",        "Function",      "Variable",      "Tag",
    "Attribute",      "Attr value",    "Property",      "Selector",
    "Pseudo-class",   "HTML entity",   "PHP delimiter",
    "Heading",        "Bold",          "Link",
    "Key",            "Value",         "Builtin",
};

const char *colorEntryName(int index)
{
    if (index >= 0 && index < kNumColorEntries)
        return colorNames[index];
    return "?";
}

TColorAttr &colorEntryRef(SyntaxColors &colors, int index)
{
    static TColorAttr dummy(0x07);
    switch (index) {
    case  0: return colors.normal;
    case  1: return colors.keyword;
    case  2: return colors.type;
    case  3: return colors.string;
    case  4: return colors.number;
    case  5: return colors.comment;
    case  6: return colors.preprocessor;
    case  7: return colors.oper;
    case  8: return colors.bracket;
    case  9: return colors.function;
    case 10: return colors.variable;
    case 11: return colors.tag;
    case 12: return colors.attribute;
    case 13: return colors.attrValue;
    case 14: return colors.property;
    case 15: return colors.selector;
    case 16: return colors.pseudoClass;
    case 17: return colors.htmlEntity;
    case 18: return colors.phpDelimiter;
    case 19: return colors.heading;
    case 20: return colors.bold;
    case 21: return colors.link;
    case 22: return colors.key;
    case 23: return colors.value;
    case 24: return colors.builtin;
    default: return dummy;
    }
}

// ── SyntaxColors serialization ─────────────────────────────────────────

std::string SyntaxColors::serialize() const
{
    // Cast away const to use colorEntryRef
    SyntaxColors &self = const_cast<SyntaxColors &>(*this);
    std::string result;
    for (int i = 0; i < kNumColorEntries; i++) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02X", colorEntryRef(self, i).toBIOS());
        if (i > 0) result += ' ';
        result += buf;
    }
    return result;
}

SyntaxColors SyntaxColors::deserialize(const std::string &s)
{
    SyntaxColors c = defaultColors();
    std::istringstream iss(s);
    std::string tok;
    int i = 0;
    while (iss >> tok && i < kNumColorEntries) {
        unsigned int val = 0;
        if (sscanf(tok.c_str(), "%X", &val) == 1 && val <= 0xFF) {
            colorEntryRef(c, i) = TColorAttr((uint8_t)val);
        }
        // M8: Skip invalid entries, keeping default colors
        i++;
    }
    return c;
}

// ── ThemeManager ───────────────────────────────────────────────────────

ThemeManager::ThemeManager()
{
    // Initialize with built-in themes
    themeList.push_back({"Borland", SyntaxColors::defaultColors(), true});
    themeList.push_back({"Dark", SyntaxColors::darkColors(), true});
    themeList.push_back({"Classic", SyntaxColors::classicColors(), true});
    currentIdx = 0;
}

ThemeManager &ThemeManager::instance()
{
    static ThemeManager mgr;
    return mgr;
}

std::string ThemeManager::configPath()
{
    std::string home;
    const char *h = getenv("HOME");
    if (h) home = h;
    else home = ".";
    return home + "/.config/tvide/themes.conf";
}

void ThemeManager::load()
{
    std::ifstream f(configPath());
    if (!f.is_open()) return;

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;

        if (line.substr(0, 8) == "current=") {
            currentIdx = atoi(line.c_str() + 8);
            continue;
        }

        // Format: "theme=Name|HH HH HH ..."
        if (line.substr(0, 6) != "theme=") continue;
        auto bar = line.find('|', 6);
        if (bar == std::string::npos) continue;

        std::string name = line.substr(6, bar - 6);
        std::string data = line.substr(bar + 1);

        // Check if this overrides a built-in theme
        bool found = false;
        for (auto &t : themeList) {
            if (t.name == name) {
                t.colors = SyntaxColors::deserialize(data);
                found = true;
                break;
            }
        }
        if (!found)
            themeList.push_back({name, SyntaxColors::deserialize(data), false});
    }

    if (currentIdx < 0 || currentIdx >= (int)themeList.size())
        currentIdx = 0;
}

void ThemeManager::save()
{
    std::string path = configPath();

    // Ensure directory exists
    std::string dir = path.substr(0, path.rfind('/'));
    std::string parentDir = dir.substr(0, dir.rfind('/'));
    mkdir(parentDir.c_str(), 0755);
    mkdir(dir.c_str(), 0755);

    std::ofstream f(path);
    if (!f.is_open()) return;

    f << "# TVIDE color themes\n";
    f << "current=" << currentIdx << "\n";
    for (auto &t : themeList) {
        f << "theme=" << t.name << "|" << t.colors.serialize() << "\n";
    }
}

void ThemeManager::selectTheme(int idx)
{
    if (idx >= 0 && idx < (int)themeList.size())
        currentIdx = idx;
}

void ThemeManager::addTheme(const std::string &name, const SyntaxColors &colors)
{
    themeList.push_back({name, colors, false});
    currentIdx = (int)themeList.size() - 1;
}

void ThemeManager::updateTheme(int idx, const SyntaxColors &colors)
{
    if (idx >= 0 && idx < (int)themeList.size())
        themeList[idx].colors = colors;
}

void ThemeManager::deleteTheme(int idx)
{
    if (idx >= 0 && idx < (int)themeList.size() && !themeList[idx].builtIn) {
        themeList.erase(themeList.begin() + idx);
        if (currentIdx >= (int)themeList.size())
            currentIdx = (int)themeList.size() - 1;
    }
}
