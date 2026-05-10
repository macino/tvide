#include "dialogs.h"
#include "panels/panels.h"
#include "syntax/lexer.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <memory>
#include <regex>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace {

long long nowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

const char *symKindName(int k)
{
    switch ((SymbolKind)k) {
    case SymbolKind::Class:       return "class";
    case SymbolKind::Interface:   return "interface";
    case SymbolKind::Trait:       return "trait";
    case SymbolKind::Enum:        return "enum";
    case SymbolKind::Function:    return "function";
    case SymbolKind::Method:      return "method";
    case SymbolKind::Constructor: return "ctor";
    case SymbolKind::Property:    return "property";
    case SymbolKind::Variable:    return "var";
    case SymbolKind::Constant:    return "const";
    case SymbolKind::Namespace:   return "namespace";
    case SymbolKind::Heading:     return "heading";
    case SymbolKind::Tag:         return "tag";
    case SymbolKind::Selector:    return "selector";
    case SymbolKind::Key:         return "key";
    }
    return "?";
}

bool isSourceExt(const std::string &name)
{
    auto dot = name.rfind('.');
    if (dot == std::string::npos) return false;
    std::string e = name.substr(dot + 1);
    for (auto &c : e) c = (char)tolower((unsigned char)c);
    static const char *exts[] = {
        "php","phtml","html","htm","css","scss","less","sass",
        "js","jsx","mjs","cjs","ts","tsx","vue",
        "json","md","markdown","yml","yaml","sql","xml","svg","xsl",
        "twig","blade","latte","c","cpp","cc","cxx","h","hpp","hxx","hh",
        "java","cs","go","rs","kt","kts","swift","lua",
        "py","pyw","sh","bash","rb","rake", nullptr
    };
    for (int i = 0; exts[i]; i++) if (e == exts[i]) return true;
    return false;
}

bool shouldSkipDir(const std::string &name)
{
    if (name.empty() || name[0] == '.') return true;
    static const char *skip[] = {
        "node_modules","vendor","build","dist","target","out","cmake-build-debug",
        "__pycache__","bower_components","coverage", nullptr
    };
    for (int i = 0; skip[i]; i++) if (name == skip[i]) return true;
    return false;
}

void walkSourceFiles(const std::string &root, std::vector<std::string> &out, int depth = 0)
{
    if (depth > 16) return;
    DIR *d = opendir(root.c_str());
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != nullptr) {
        if (e->d_name[0] == '.' &&
            (e->d_name[1] == '\0' ||
             (e->d_name[1] == '.' && e->d_name[2] == '\0')))
            continue;
        std::string name = e->d_name;
        std::string full = root + "/" + name;
        struct stat st;
        if (stat(full.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (shouldSkipDir(name)) continue;
            walkSourceFiles(full, out, depth + 1);
        } else if (S_ISREG(st.st_mode) && isSourceExt(name)) {
            out.push_back(full);
        }
    }
    closedir(d);
}

bool readFile(const std::string &path, std::string &out, size_t maxBytes = 1024 * 1024)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    auto sz = (size_t)f.tellg();
    if (sz == 0 || sz > maxBytes) return false;
    f.seekg(0, std::ios::beg);
    out.resize(sz);
    f.read(&out[0], sz);
    return true;
}

std::string getInputText(TInputLine *in)
{
    if (!in) return {};
    char buf[300] = {};
    in->getData(buf);
    int len = (unsigned char)buf[0];
    return std::string(buf + 1, len);
}

ushort getCheckboxes(TCheckBoxes *cb)
{
    if (!cb) return 0;
    ushort v = 0;
    cb->getData(&v);
    return v;
}

bool matchesQuery(const std::string &name, const std::string &query,
                  bool caseSens, bool useRegex,
                  const std::regex &rx)
{
    if (useRegex) {
        try { return std::regex_search(name, rx); }
        catch (...) { return false; }
    }
    if (caseSens) return name.find(query) != std::string::npos;
    std::string a = name, b = query;
    for (auto &c : a) c = (char)tolower((unsigned char)c);
    for (auto &c : b) c = (char)tolower((unsigned char)c);
    return a.find(b) != std::string::npos;
}

constexpr int kDlgW = 100;
constexpr int kDlgH = 26;
constexpr int kInputY = 2;
constexpr int kOptsY = 4;
constexpr int kListY = 6;

} // anonymous

// ── LiveSymbolDialog ───────────────────────────────────────────────────

LiveSymbolDialog::LiveSymbolDialog(const std::string &root)
    : TWindowInit(&LiveSymbolDialog::initFrame),
      TDialog(TRect(0, 0, kDlgW, kDlgH), "Find Symbol"),
      rootPath(root),
      queryInput(nullptr), opts(nullptr), resultsList(nullptr),
      resultsSB(nullptr), items(nullptr)
{
    options |= ofCentered;

    queryInput = new TInputLine(TRect(2, kInputY, kDlgW - 2, kInputY + 1), 256);
    insert(queryInput);
    insert(new TLabel(TRect(2, kInputY - 1, kDlgW - 2, kInputY), "~Q~uery (live)", queryInput));

    opts = new TCheckBoxes(TRect(2, kOptsY, kDlgW - 2, kOptsY + 1),
        new TSItem("~C~ase",
        new TSItem("Reg~e~xp", nullptr)));
    insert(opts);

    int listBottom = kDlgH - 4;
    resultsSB = new TScrollBar(TRect(kDlgW - 3, kListY, kDlgW - 2, listBottom));
    insert(resultsSB);
    resultsList = new TListBox(TRect(2, kListY, kDlgW - 3, listBottom), 1, resultsSB);
    insert(resultsList);

    items = new TUnsortedStringCollection(50, 50);
    resultsList->newList(items);

    insert(new TButton(TRect(kDlgW/2 - 11, kDlgH - 3, kDlgW/2 - 1, kDlgH - 1),
                       "O~K~", cmOK, bfDefault));
    insert(new TButton(TRect(kDlgW/2 + 1, kDlgH - 3, kDlgW/2 + 11, kDlgH - 1),
                       "Cancel", cmCancel, bfNormal));

    selectNext(False);
    queryInput->select();

    buildIndex();
    refilter();
}

void LiveSymbolDialog::buildIndex()
{
    all.clear();
    std::vector<std::string> files;
    walkSourceFiles(rootPath, files);
    int kMax = 800;
    int scanned = 0;
    for (const auto &p : files) {
        if (scanned >= kMax) break;
        std::unique_ptr<SyntaxLexer> lex(SyntaxLexer::createForFile(p));
        if (!lex) continue;
        std::string text;
        if (!readFile(p, text)) continue;
        std::vector<SymbolInfo> syms;
        parseStructureText(lex->languageName(), text, syms);
        for (const auto &s : syms)
            all.push_back({p, s.line, (int)s.kind, s.name});
        scanned++;
    }
}

std::string LiveSymbolDialog::currentQuery() const
{
    return getInputText(queryInput);
}

ushort LiveSymbolDialog::currentOpts() const
{
    return getCheckboxes(opts);
}

void LiveSymbolDialog::refilter()
{
    std::string q = currentQuery();
    ushort o = currentOpts();
    bool caseSens = (o & 0x01) != 0;
    bool useRegex = (o & 0x02) != 0;

    std::regex rx;
    if (useRegex && !q.empty()) {
        try {
            auto flags = std::regex::ECMAScript;
            if (!caseSens) flags = flags | std::regex::icase;
            rx = std::regex(q, flags);
        } catch (...) { /* invalid regex — show nothing */ }
    }

    filtered.clear();
    if (!q.empty()) {
        for (size_t i = 0; i < all.size(); i++) {
            if (matchesQuery(all[i].name, q, caseSens, useRegex, rx))
                filtered.push_back((int)i);
            if (filtered.size() >= 500) break;
        }
    }

    auto *newItems = new TUnsortedStringCollection((ccIndex)filtered.size() + 1, 25);
    for (int idx : filtered) {
        const auto &e = all[idx];
        const char *base = strrchr(e.path.c_str(), '/');
        base = base ? base + 1 : e.path.c_str();
        char line[400];
        snprintf(line, sizeof(line), "[%-9s] %-32s  %s:%d",
                 symKindName(e.kind), e.name.c_str(), base, e.line);
        newItems->insert(newStr(line));
    }
    resultsList->newList(newItems);
    items = newItems;
    if (!filtered.empty())
        resultsList->focusItem(0);
    resultsList->drawView();
}

void LiveSymbolDialog::handleEvent(TEvent &event)
{
    bool wasEnter = (event.what == evKeyDown &&
                     event.keyDown.keyCode == kbEnter);

    // Down/Up while focused on input → move into list
    if (event.what == evKeyDown && queryInput && queryInput->state & sfFocused) {
        if (event.keyDown.keyCode == kbDown) {
            resultsList->select();
            clearEvent(event);
            return;
        }
    }

    TDialog::handleEvent(event);

    // After every event, see if query/opts changed and refilter.
    std::string q = currentQuery();
    ushort o = currentOpts();
    if (q != lastQuery || o != lastOpts) {
        lastQuery = q;
        lastOpts = o;
        refilter();
    }

    // Enter on a result → set result and close
    if (wasEnter && resultsList && resultsList->state & sfFocused) {
        int idx = resultsList->focused;
        if (idx >= 0 && idx < (int)filtered.size()) {
            const auto &e = all[filtered[idx]];
            resultPath = e.path;
            resultLine = e.line;
            resultName = e.name;
            event.what = evCommand;
            event.message.command = cmOK;
            TDialog::handleEvent(event);
        }
    }
}

// ── LiveTextSearchDialog ───────────────────────────────────────────────

LiveTextSearchDialog::LiveTextSearchDialog(const std::string &root)
    : TWindowInit(&LiveTextSearchDialog::initFrame),
      TDialog(TRect(0, 0, kDlgW, kDlgH), "Find in Files"),
      rootPath(root),
      queryInput(nullptr), opts(nullptr), resultsList(nullptr),
      resultsSB(nullptr), items(nullptr)
{
    options |= ofCentered;

    queryInput = new TInputLine(TRect(2, kInputY, kDlgW - 2, kInputY + 1), 256);
    insert(queryInput);
    insert(new TLabel(TRect(2, kInputY - 1, kDlgW - 2, kInputY),
                      "~T~ext (live)", queryInput));

    opts = new TCheckBoxes(TRect(2, kOptsY, kDlgW - 2, kOptsY + 1),
        new TSItem("~C~ase",
        new TSItem("Reg~e~xp (ERE)", nullptr)));
    insert(opts);

    int listBottom = kDlgH - 4;
    resultsSB = new TScrollBar(TRect(kDlgW - 3, kListY, kDlgW - 2, listBottom));
    insert(resultsSB);
    resultsList = new TListBox(TRect(2, kListY, kDlgW - 3, listBottom), 1, resultsSB);
    insert(resultsList);

    items = new TUnsortedStringCollection(50, 50);
    resultsList->newList(items);

    insert(new TButton(TRect(kDlgW/2 - 11, kDlgH - 3, kDlgW/2 - 1, kDlgH - 1),
                       "O~K~", cmOK, bfDefault));
    insert(new TButton(TRect(kDlgW/2 + 1, kDlgH - 3, kDlgW/2 + 11, kDlgH - 1),
                       "Cancel", cmCancel, bfNormal));

    selectNext(False);
    queryInput->select();
}

LiveTextSearchDialog::~LiveTextSearchDialog() {}

std::string LiveTextSearchDialog::currentQuery() const
{
    return getInputText(queryInput);
}

ushort LiveTextSearchDialog::currentOpts() const
{
    return getCheckboxes(opts);
}

void LiveTextSearchDialog::scheduleSearch()
{
    dirty = true;
    lastEditMs = nowMs();
}

void LiveTextSearchDialog::doSearch()
{
    hits.clear();
    std::string q = currentQuery();
    if (q.size() < 2) {
        auto *e = new TUnsortedStringCollection(1, 1);
        resultsList->newList(e);
        items = e;
        resultsList->drawView();
        return;
    }
    ushort o = currentOpts();
    bool caseSens = (o & 0x01) != 0;
    bool useRegex = (o & 0x02) != 0;

    std::string cmd = "grep -n -r --include='*.*'";
    if (useRegex) cmd += " -E"; else cmd += " -F";
    if (!caseSens) cmd += " -i";
    cmd += " --exclude-dir=.git --exclude-dir=node_modules --exclude-dir=vendor";
    cmd += " --exclude-dir=build --exclude-dir=dist";
    cmd += " -- '";
    for (char c : q) {
        if (c == '\'') cmd += "'\\''";
        else cmd += c;
    }
    cmd += "' '";
    for (char c : rootPath) {
        if (c == '\'') cmd += "'\\''";
        else cmd += c;
    }
    cmd += "' 2>/dev/null | head -200";

    FILE *fp = popen(cmd.c_str(), "r");
    if (!fp) return;
    char line[2048];
    while (fgets(line, sizeof(line), fp)) {
        std::string s(line);
        if (!s.empty() && s.back() == '\n') s.pop_back();
        // parse path:line:text
        auto p1 = s.find(':');
        if (p1 == std::string::npos) continue;
        auto p2 = s.find(':', p1 + 1);
        if (p2 == std::string::npos) continue;
        std::string path = s.substr(0, p1);
        int ln = atoi(s.c_str() + p1 + 1);
        std::string txt = s.substr(p2 + 1);
        hits.push_back({path, ln, txt});
        if (hits.size() >= 200) break;
    }
    pclose(fp);

    auto *newItems = new TUnsortedStringCollection((ccIndex)hits.size() + 1, 25);
    for (const auto &h : hits) {
        const char *base = strrchr(h.path.c_str(), '/');
        base = base ? base + 1 : h.path.c_str();
        char buf[2200];
        snprintf(buf, sizeof(buf), "%s:%d  %s", base, h.line, h.text.c_str());
        newItems->insert(newStr(buf));
    }
    resultsList->newList(newItems);
    items = newItems;
    if (!hits.empty()) resultsList->focusItem(0);
    resultsList->drawView();
}

void LiveTextSearchDialog::handleEvent(TEvent &event)
{
    bool wasEnter = (event.what == evKeyDown &&
                     event.keyDown.keyCode == kbEnter);

    if (event.what == evKeyDown && queryInput && queryInput->state & sfFocused) {
        if (event.keyDown.keyCode == kbDown) {
            resultsList->select();
            clearEvent(event);
            return;
        }
    }

    TDialog::handleEvent(event);

    std::string q = currentQuery();
    ushort o = currentOpts();
    if (q != lastQuery || o != lastOpts) {
        lastQuery = q;
        lastOpts = o;
        scheduleSearch();
    }

    // Debounce: if dirty and 250ms idle, run search.
    if (dirty && nowMs() - lastEditMs > 250) {
        dirty = false;
        doSearch();
    }

    if (wasEnter && resultsList && resultsList->state & sfFocused) {
        int idx = resultsList->focused;
        if (idx >= 0 && idx < (int)hits.size()) {
            resultPath = hits[idx].path;
            resultLine = hits[idx].line;
            event.what = evCommand;
            event.message.command = cmOK;
            TDialog::handleEvent(event);
        }
    }
}
