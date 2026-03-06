#include "panels.h"
#include "editor/editor.h"
#include <cstring>
#include <algorithm>
#include <functional>

// ── Symbol types ────────────────────────────────────────────────────────

static const char *symbolIcon(SymbolKind kind)
{
    switch (kind) {
    case SymbolKind::Class:       return "📦 ";
    case SymbolKind::Interface:   return "🔷 ";
    case SymbolKind::Trait:       return "🔸 ";
    case SymbolKind::Enum:        return "📋 ";
    case SymbolKind::Function:    return "🔹 ";
    case SymbolKind::Method:      return "⚙ ";
    case SymbolKind::Property:    return "🔸 ";
    case SymbolKind::Variable:    return "📌 ";
    case SymbolKind::Constant:    return "🔒 ";
    case SymbolKind::Namespace:   return "📁 ";
    case SymbolKind::Constructor: return "🔨 ";
    case SymbolKind::Heading:     return "📝 ";
    case SymbolKind::Tag:         return "🏷 ";
    case SymbolKind::Selector:    return "🎨 ";
    case SymbolKind::Key:         return "🔑 ";
    default:                      return "   ";
    }
}

// ── Symbol extraction from editor buffer ────────────────────────────────

// Helper: extract all text from editor gap buffer
static std::string extractEditorText(TSyntaxEditor *editor)
{
    std::string text;
    text.reserve(editor->bufLen);
    for (uint i = 0; i < editor->bufLen; i++) {
        char ch = (i < (uint)editor->curPtr)
            ? editor->buffer[i]
            : editor->buffer[i + editor->gapLen];
        text += ch;
    }
    return text;
}

// Helper: iterate lines, calling callback(lineNum, lineText)
static void forEachLine(const std::string &text,
    const std::function<void(int, const std::string &)> &cb)
{
    int lineNum = 1;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t eol = text.find('\n', pos);
        if (eol == std::string::npos) eol = text.size();
        cb(lineNum, text.substr(pos, eol - pos));
        pos = eol + 1;
        lineNum++;
    }
}

// Parse symbols for PHP
static void parsePhp(const std::string &text, std::vector<SymbolInfo> &syms)
{
    forEachLine(text, [&](int line, const std::string &ln) {
        // Trim leading whitespace for indent detection
        size_t indent = ln.find_first_not_of(" \t");
        if (indent == std::string::npos) return;

        // namespace
        if (ln.find("namespace ", indent) == indent) {
            size_t s = indent + 10;
            size_t e = ln.find_first_of(";{", s);
            if (e != std::string::npos)
                syms.push_back({SymbolKind::Namespace, ln.substr(s, e - s), line, 0});
        }
        // class / interface / trait / enum
        else if (ln.find("class ", indent) != std::string::npos ||
                 ln.find("abstract class ", indent) != std::string::npos ||
                 ln.find("final class ", indent) != std::string::npos) {
            auto p = ln.find("class ");
            if (p != std::string::npos) {
                size_t s = p + 6;
                size_t e = s;
                while (e < ln.size() && ln[e] != ' ' && ln[e] != '{' && ln[e] != ':')
                    e++;
                syms.push_back({SymbolKind::Class, ln.substr(s, e - s), line, 0});
            }
        }
        else if (ln.find("interface ", indent) != std::string::npos) {
            auto p = ln.find("interface ");
            size_t s = p + 10;
            size_t e = s;
            while (e < ln.size() && ln[e] != ' ' && ln[e] != '{')
                e++;
            syms.push_back({SymbolKind::Interface, ln.substr(s, e - s), line, 0});
        }
        else if (ln.find("trait ", indent) != std::string::npos) {
            auto p = ln.find("trait ");
            size_t s = p + 6;
            size_t e = s;
            while (e < ln.size() && ln[e] != ' ' && ln[e] != '{')
                e++;
            syms.push_back({SymbolKind::Trait, ln.substr(s, e - s), line, 0});
        }
        else if (ln.find("enum ", indent) != std::string::npos) {
            auto p = ln.find("enum ");
            size_t s = p + 5;
            size_t e = s;
            while (e < ln.size() && ln[e] != ' ' && ln[e] != '{' && ln[e] != ':')
                e++;
            syms.push_back({SymbolKind::Enum, ln.substr(s, e - s), line, 0});
        }
        // function / method
        else if (ln.find("function ") != std::string::npos) {
            auto p = ln.find("function ");
            size_t s = p + 9;
            if (s < ln.size() && ln[s] == '&') s++; // reference return
            size_t e = s;
            while (e < ln.size() && ln[e] != '(' && ln[e] != ' ')
                e++;
            if (e > s) {
                SymbolKind kind = (indent > 0) ? SymbolKind::Method : SymbolKind::Function;
                int depth = (indent > 0) ? 1 : 0;
                syms.push_back({kind, ln.substr(s, e - s), line, depth});
            }
        }
        // const
        else if (ln.find("const ", indent) != std::string::npos && indent > 0) {
            auto p = ln.find("const ");
            size_t s = p + 6;
            size_t e = s;
            while (e < ln.size() && ln[e] != ' ' && ln[e] != '=' && ln[e] != ';')
                e++;
            if (e > s)
                syms.push_back({SymbolKind::Constant, ln.substr(s, e - s), line, 1});
        }
    });
}

// Parse symbols for JavaScript / TypeScript
static void parseJsTs(const std::string &text, std::vector<SymbolInfo> &syms)
{
    forEachLine(text, [&](int line, const std::string &ln) {
        size_t indent = ln.find_first_not_of(" \t");
        if (indent == std::string::npos) return;

        // class
        if (ln.find("class ", indent) == indent ||
            ln.find("export class ", indent) == indent ||
            ln.find("export default class ", indent) == indent ||
            ln.find("abstract class ", indent) == indent) {
            auto p = ln.find("class ");
            if (p != std::string::npos) {
                size_t s = p + 6;
                size_t e = s;
                while (e < ln.size() && ln[e] != ' ' && ln[e] != '{' && ln[e] != '<')
                    e++;
                if (e > s)
                    syms.push_back({SymbolKind::Class, ln.substr(s, e - s), line, 0});
            }
        }
        // interface (TS)
        else if (ln.find("interface ", indent) == indent ||
                 ln.find("export interface ", indent) == indent) {
            auto p = ln.find("interface ");
            size_t s = p + 10;
            size_t e = s;
            while (e < ln.size() && ln[e] != ' ' && ln[e] != '{' && ln[e] != '<')
                e++;
            if (e > s)
                syms.push_back({SymbolKind::Interface, ln.substr(s, e - s), line, 0});
        }
        // enum (TS)
        else if (ln.find("enum ", indent) == indent ||
                 ln.find("export enum ", indent) == indent ||
                 ln.find("const enum ", indent) == indent) {
            auto p = ln.find("enum ");
            size_t s = p + 5;
            size_t e = s;
            while (e < ln.size() && ln[e] != ' ' && ln[e] != '{')
                e++;
            if (e > s)
                syms.push_back({SymbolKind::Enum, ln.substr(s, e - s), line, 0});
        }
        // function
        else if (ln.find("function ", indent) == indent ||
                 ln.find("export function ", indent) == indent ||
                 ln.find("export default function ", indent) == indent ||
                 ln.find("async function ", indent) == indent ||
                 ln.find("export async function ", indent) == indent) {
            auto p = ln.find("function ");
            size_t s = p + 9;
            if (s < ln.size() && ln[s] == '*') s++; // generator
            size_t e = s;
            while (e < ln.size() && ln[e] != '(' && ln[e] != '<' && ln[e] != ' ')
                e++;
            if (e > s)
                syms.push_back({SymbolKind::Function, ln.substr(s, e - s), line, 0});
        }
        // const/let/var arrow or assigned function
        else if ((ln.find("const ", indent) == indent ||
                  ln.find("let ", indent) == indent ||
                  ln.find("var ", indent) == indent ||
                  ln.find("export const ", indent) == indent) &&
                 (ln.find("=>") != std::string::npos ||
                  ln.find("function") != std::string::npos)) {
            // Extract name after const/let/var
            auto p = ln.find_first_of("cletvar", indent);
            p = ln.find(' ', p);
            if (p == std::string::npos) return;
            p++;
            size_t e = p;
            while (e < ln.size() && ln[e] != ' ' && ln[e] != '=' && ln[e] != ':' && ln[e] != '(')
                e++;
            if (e > p)
                syms.push_back({SymbolKind::Function, ln.substr(p, e - p), line, 0});
        }
    });
}

// Parse symbols for HTML / Vue / XML
static void parseHtml(const std::string &text, std::vector<SymbolInfo> &syms)
{
    // Extract top-level tags: <template>, <script>, <style>, <div id=...>, etc.
    forEachLine(text, [&](int line, const std::string &ln) {
        size_t indent = ln.find_first_not_of(" \t");
        if (indent == std::string::npos) return;

        if (ln[indent] == '<' && indent + 1 < ln.size() && ln[indent + 1] != '/' && ln[indent + 1] != '!') {
            size_t s = indent + 1;
            size_t e = s;
            while (e < ln.size() && ln[e] != ' ' && ln[e] != '>' && ln[e] != '/')
                e++;
            if (e > s) {
                std::string tagName = ln.substr(s, e - s);
                // Only show structural tags
                if (tagName == "template" || tagName == "script" || tagName == "style" ||
                    tagName == "head" || tagName == "body" || tagName == "header" ||
                    tagName == "nav" || tagName == "main" || tagName == "footer" ||
                    tagName == "section" || tagName == "article" || tagName == "aside" ||
                    tagName == "div" || tagName == "form" || tagName == "table") {
                    // Try to extract id or class
                    std::string label = tagName;
                    auto idp = ln.find("id=\"");
                    if (idp != std::string::npos) {
                        size_t is = idp + 4;
                        size_t ie = ln.find('"', is);
                        if (ie != std::string::npos)
                            label += "#" + ln.substr(is, ie - is);
                    } else {
                        auto cp = ln.find("class=\"");
                        if (cp != std::string::npos) {
                            size_t cs = cp + 7;
                            size_t ce = ln.find('"', cs);
                            if (ce != std::string::npos) {
                                std::string cls = ln.substr(cs, ce - cs);
                                auto sp = cls.find(' ');
                                if (sp != std::string::npos) cls = cls.substr(0, sp);
                                label += "." + cls;
                            }
                        }
                    }
                    int depth = (int)(indent / 2);
                    if (depth > 4) depth = 4;
                    syms.push_back({SymbolKind::Tag, label, line, depth});
                }
            }
        }
    });
}

// Parse symbols for CSS / SCSS / LESS
static void parseCss(const std::string &text, std::vector<SymbolInfo> &syms)
{
    forEachLine(text, [&](int line, const std::string &ln) {
        size_t indent = ln.find_first_not_of(" \t");
        if (indent == std::string::npos) return;
        // CSS rule: line that doesn't start with } or * or / and contains {
        if (ln[indent] != '}' && ln[indent] != '*' && ln[indent] != '/') {
            auto brace = ln.find('{');
            if (brace != std::string::npos && brace > indent) {
                std::string sel = ln.substr(indent, brace - indent);
                // Trim trailing whitespace
                while (!sel.empty() && (sel.back() == ' ' || sel.back() == '\t'))
                    sel.pop_back();
                if (!sel.empty()) {
                    int depth = (int)(indent / 2);
                    if (depth > 4) depth = 4;
                    syms.push_back({SymbolKind::Selector, sel, line, depth});
                }
            }
        }
    });
}

// Parse symbols for JSON
static void parseJson(const std::string &text, std::vector<SymbolInfo> &syms)
{
    forEachLine(text, [&](int line, const std::string &ln) {
        size_t indent = ln.find_first_not_of(" \t");
        if (indent == std::string::npos) return;
        // Look for "key":
        if (ln[indent] == '"') {
            auto end = ln.find('"', indent + 1);
            if (end != std::string::npos) {
                auto colon = ln.find(':', end);
                if (colon != std::string::npos) {
                    std::string key = ln.substr(indent + 1, end - indent - 1);
                    int depth = (int)(indent / 2);
                    if (depth > 4) depth = 4;
                    syms.push_back({SymbolKind::Key, key, line, depth});
                }
            }
        }
    });
}

// Parse symbols for YAML
static void parseYaml(const std::string &text, std::vector<SymbolInfo> &syms)
{
    forEachLine(text, [&](int line, const std::string &ln) {
        size_t indent = ln.find_first_not_of(" \t");
        if (indent == std::string::npos) return;
        if (ln[indent] == '#' || ln[indent] == '-') return;
        auto colon = ln.find(':');
        if (colon != std::string::npos && colon > indent) {
            std::string key = ln.substr(indent, colon - indent);
            int depth = (int)(indent / 2);
            if (depth > 4) depth = 4;
            syms.push_back({SymbolKind::Key, key, line, depth});
        }
    });
}

// Parse symbols for Markdown
static void parseMarkdown(const std::string &text, std::vector<SymbolInfo> &syms)
{
    forEachLine(text, [&](int line, const std::string &ln) {
        if (ln.empty()) return;
        // Headings
        int level = 0;
        while (level < (int)ln.size() && ln[level] == '#') level++;
        if (level > 0 && level <= 6 && level < (int)ln.size() && ln[level] == ' ') {
            std::string title = ln.substr(level + 1);
            // Trim trailing #
            while (!title.empty() && title.back() == '#') title.pop_back();
            while (!title.empty() && title.back() == ' ') title.pop_back();
            syms.push_back({SymbolKind::Heading, title, line, level - 1});
        }
    });
}

// Parse symbols for SQL
static void parseSql(const std::string &text, std::vector<SymbolInfo> &syms)
{
    forEachLine(text, [&](int line, const std::string &ln) {
        size_t indent = ln.find_first_not_of(" \t");
        if (indent == std::string::npos) return;

        // Case insensitive check for CREATE TABLE/VIEW/FUNCTION/PROCEDURE
        std::string upper;
        for (size_t i = indent; i < ln.size() && i < indent + 30; i++)
            upper += toupper(ln[i]);

        if (upper.find("CREATE") == 0) {
            if (upper.find("TABLE") != std::string::npos) {
                auto p = upper.find("TABLE");
                size_t s = indent + p + 5;
                while (s < ln.size() && ln[s] == ' ') s++;
                size_t e = s;
                while (e < ln.size() && ln[e] != ' ' && ln[e] != '(')
                    e++;
                if (e > s)
                    syms.push_back({SymbolKind::Class, ln.substr(s, e - s), line, 0});
            } else if (upper.find("VIEW") != std::string::npos) {
                auto p = upper.find("VIEW");
                size_t s = indent + p + 4;
                while (s < ln.size() && ln[s] == ' ') s++;
                size_t e = s;
                while (e < ln.size() && ln[e] != ' ' && ln[e] != '(')
                    e++;
                if (e > s)
                    syms.push_back({SymbolKind::Class, ln.substr(s, e - s), line, 0});
            } else if (upper.find("FUNCTION") != std::string::npos ||
                       upper.find("PROCEDURE") != std::string::npos) {
                std::string kw = (upper.find("FUNCTION") != std::string::npos)
                    ? "FUNCTION" : "PROCEDURE";
                auto p = upper.find(kw);
                size_t s = indent + p + kw.size();
                while (s < ln.size() && ln[s] == ' ') s++;
                size_t e = s;
                while (e < ln.size() && ln[e] != ' ' && ln[e] != '(')
                    e++;
                if (e > s)
                    syms.push_back({SymbolKind::Function, ln.substr(s, e - s), line, 0});
            }
        }
    });
}

// Main dispatcher
void parseStructure(TSyntaxEditor *editor, std::vector<SymbolInfo> &syms)
{
    if (!editor || !editor->getLexer()) return;

    std::string text = extractEditorText(editor);
    if (text.empty()) return;

    const char *lang = editor->getLanguageName();

    if (strcmp(lang, "PHP") == 0)
        parsePhp(text, syms);
    else if (strcmp(lang, "JavaScript") == 0 || strcmp(lang, "TypeScript") == 0)
        parseJsTs(text, syms);
    else if (strcmp(lang, "HTML") == 0 || strcmp(lang, "Vue") == 0 || strcmp(lang, "XML") == 0)
        parseHtml(text, syms);
    else if (strcmp(lang, "CSS") == 0)
        parseCss(text, syms);
    else if (strcmp(lang, "JSON") == 0)
        parseJson(text, syms);
    else if (strcmp(lang, "YAML") == 0)
        parseYaml(text, syms);
    else if (strcmp(lang, "Markdown") == 0)
        parseMarkdown(text, syms);
    else if (strcmp(lang, "SQL") == 0)
        parseSql(text, syms);
    // For other languages, no symbols shown
}

// ── TStructurePanel ─────────────────────────────────────────────────────

TStructurePanel::TStructurePanel(const TRect &bounds)
    : TWindowInit(&TStructurePanel::initFrame),
      TWindow(bounds, "Structure", 0),
      items(nullptr),
      trackedEditor(nullptr)
{
    flags = 0; // no move, zoom, close — docked panel
    growMode = 0;
    state &= ~sfShadow; // no shadow — flush with desktop
    options |= ofFirstClick; // pass clicks through immediately

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

    items = new TUnsortedStringCollection(50, 10);
    listBox->newList(items);
}

TStructurePanel::~TStructurePanel()
{
    listBox->newList(nullptr);
    items = nullptr;
}

void TStructurePanel::refresh(TSyntaxEditor *editor)
{
    trackedEditor = editor;
    symbols.clear();

    if (editor)
        parseStructure(editor, symbols);

    // Build display list
    auto *newItems = new TUnsortedStringCollection(symbols.size() + 1, 10);
    for (auto &sym : symbols) {
        std::string display;
        // Indent based on depth
        for (int i = 0; i < sym.depth; i++)
            display += "  ";
        display += symbolIcon(sym.kind);
        display += sym.name;

        char lineSuffix[16];
        snprintf(lineSuffix, sizeof(lineSuffix), " :%d", sym.line);
        display += lineSuffix;

        newItems->insert(newStr(display.c_str()));
    }

    listBox->newList(newItems);
    items = newItems;
    drawView();
}

void TStructurePanel::navigateToSymbol()
{
    int idx = listBox->focused;
    if (idx < 0 || idx >= (int)symbols.size() || !trackedEditor)
        return;

    int targetLine = symbols[idx].line;

    // Navigate editor to line
    uint pos = 0;
    int curLine = 1;
    while (pos < trackedEditor->bufLen && curLine < targetLine) {
        char ch = (pos < (uint)trackedEditor->curPtr)
            ? trackedEditor->buffer[pos]
            : trackedEditor->buffer[pos + trackedEditor->gapLen];
        if (ch == '\n')
            curLine++;
        pos++;
    }
    trackedEditor->setCurPtr(pos, 0);
    trackedEditor->trackCursor(True);

    // Bring editor window to focus
    TView *v = trackedEditor->owner;
    if (v) v->select();
}

void TStructurePanel::handleEvent(TEvent &event)
{
    // Drag left border to resize
    if (event.what == evMouseDown) {
        TPoint mouse = makeLocal(event.mouse.where);
        if (mouse.x == 0) {
            TPoint lastMouse = event.mouse.where;
            while (mouseEvent(event, evMouseMove)) {
                int dx = event.mouse.where.x - lastMouse.x;
                if (dx != 0) {
                    int newWidth = size.x - dx;
                    if (newWidth >= 15 && newWidth <= 60) {
                        TRect r = getBounds();
                        r.a.x = r.b.x - newWidth;
                        changeBounds(r);
                    }
                    lastMouse = event.mouse.where;
                }
            }
            clearEvent(event);
            return;
        }
    }

    TWindow::handleEvent(event);

    if (event.what == evKeyDown && event.keyDown.keyCode == kbEnter) {
        navigateToSymbol();
        clearEvent(event);
    }

    if (event.what == evBroadcast &&
        event.message.command == cmListItemSelected) {
        navigateToSymbol();
        clearEvent(event);
    }
}

void TStructurePanel::changeBounds(const TRect &bounds)
{
    TWindow::changeBounds(bounds);
    message(TProgram::application, evBroadcast, cmRelayout, this);
}

void TStructurePanel::sizeLimits(TPoint &min, TPoint &max)
{
    TWindow::sizeLimits(min, max);
    min.x = 15;
    max.x = 60;
}

TPalette &TStructurePanel::getPalette() const
{
    static char customPal[] =
        "\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F"
        "\x30\x31\x32\x33\x34\x35\x36\x37\x38"
        "\x20"  // 26: list normal (black on light gray)
        "\x06"  // 27: list focused (menu selected highlight)
        "\x21"  // 28: list selected (white on light gray)
        "\x20"  // 29: list divider
        "\x3D\x3E\x3F";
    static TPalette pal(customPal, sizeof(customPal) - 1);
    return pal;
}
