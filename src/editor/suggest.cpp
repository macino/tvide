#include "editor.h"
#include "syntax/lexer.h"
#include "project/project.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>

// ── SuggestionIndex ────────────────────────────────────────────────────

SuggestionIndex &SuggestionIndex::instance()
{
    static SuggestionIndex inst;
    return inst;
}

namespace {
inline bool isWord(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '$';
}

inline bool isWordStart(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           c == '_' || c == '$';
}

bool isExcluded(const std::string &name)
{
    if (name.empty() || name[0] == '.') return true;
    static const char *skip[] = {
        "node_modules","vendor","build","dist","target","out","cmake-build-debug",
        "__pycache__","bower_components","coverage", nullptr
    };
    for (int i = 0; skip[i]; i++) if (name == skip[i]) return true;
    return false;
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
        "json","md","markdown","yml","yaml","sql","xml",
        "c","cpp","cc","cxx","h","hpp","hxx","hh",
        "java","cs","go","rs","kt","kts","swift","lua",
        "py","pyw","sh","bash","rb","twig","blade", nullptr
    };
    for (int i = 0; exts[i]; i++) if (e == exts[i]) return true;
    return false;
}
} // namespace

void SuggestionIndex::scanText(const std::string &text)
{
    int n = (int)text.size();
    int i = 0;
    while (i < n) {
        if (isWordStart(text[i])) {
            int s = i++;
            while (i < n && isWord(text[i])) i++;
            int len = i - s;
            if (len >= 2 && len <= 64)
                words_.insert(text.substr(s, len));
        } else {
            i++;
        }
    }
}

void SuggestionIndex::addLexerWords(const std::string &filename)
{
    std::unique_ptr<SyntaxLexer> lex(SyntaxLexer::createForFile(filename));
    if (!lex) return;
    // We don't have direct access to the per-language sets via the base API.
    // As a safe approximation, push the language name as a word. Specific
    // keyword tables are added through scanText of source files (each file
    // contains its own keywords) plus the per-file scan below.
    words_.insert(lex->languageName());
}

void SuggestionIndex::scanProject(const std::string &rootPath)
{
    if (rootPath.empty()) return;
    if (projectScanned_ && lastProjectRoot_ == rootPath) return;
    lastProjectRoot_ = rootPath;
    projectScanned_ = true;

    // Honour ProjectInfo exclusions if a project is open.
    std::vector<std::string> excludedAbs;
    if (ProjectManager::instance().hasProject()) {
        const auto &p = ProjectManager::instance().getProject();
        for (const auto &e : p.excludedDirs)
            excludedAbs.push_back(e);
    }

    auto isExcludedPath = [&](const std::string &full) {
        for (const auto &e : excludedAbs)
            if (!e.empty() && full.find(e) == 0) return true;
        return false;
    };

    // Recursive walker with file-count cap so we don't index forever.
    int filesScanned = 0;
    const int kMaxFiles = 500;
    const size_t kMaxBytesPerFile = 256 * 1024;

    std::vector<std::string> stack{rootPath};
    while (!stack.empty() && filesScanned < kMaxFiles) {
        std::string dir = stack.back(); stack.pop_back();
        DIR *d = opendir(dir.c_str());
        if (!d) continue;
        struct dirent *e;
        while ((e = readdir(d)) != nullptr) {
            if (e->d_name[0] == '.' &&
                (e->d_name[1] == '\0' ||
                 (e->d_name[1] == '.' && e->d_name[2] == '\0')))
                continue;
            std::string name = e->d_name;
            std::string full = dir + "/" + name;
            if (isExcludedPath(full)) continue;
            struct stat st;
            if (stat(full.c_str(), &st) != 0) continue;
            if (S_ISDIR(st.st_mode)) {
                if (isExcluded(name)) continue;
                stack.push_back(full);
            } else if (S_ISREG(st.st_mode) && isSourceExt(name)) {
                std::ifstream f(full, std::ios::binary);
                if (!f) continue;
                f.seekg(0, std::ios::end);
                size_t sz = (size_t)f.tellg();
                if (sz == 0 || sz > kMaxBytesPerFile) continue;
                f.seekg(0, std::ios::beg);
                std::string txt(sz, '\0');
                f.read(&txt[0], sz);
                scanText(txt);
                if (++filesScanned >= kMaxFiles) break;
            }
        }
        closedir(d);
    }
}

void SuggestionIndex::clear()
{
    words_.clear();
    projectScanned_ = false;
    lastProjectRoot_.clear();
}

std::vector<std::string> SuggestionIndex::suggestions(
    const std::string &prefix, size_t maxResults) const
{
    if (prefix.empty()) return {};
    std::string lp = prefix;
    for (auto &c : lp) c = (char)tolower((unsigned char)c);

    std::vector<std::string> out;
    out.reserve(64);
    for (const auto &w : words_) {
        if (w.size() < prefix.size()) continue;
        // Case-insensitive prefix compare
        bool match = true;
        for (size_t i = 0; i < prefix.size(); i++) {
            char a = (char)tolower((unsigned char)w[i]);
            if (a != lp[i]) { match = false; break; }
        }
        if (!match) continue;
        if (w == prefix) continue; // skip exact match (nothing to insert)
        out.push_back(w);
        if (out.size() >= maxResults * 2) break;
    }
    std::sort(out.begin(), out.end(),
        [](const std::string &a, const std::string &b) {
            // Shorter first (closer matches), then lexicographic
            if (a.size() != b.size()) return a.size() < b.size();
            return a < b;
        });
    if (out.size() > maxResults) out.resize(maxResults);
    return out;
}

// ── TSuggestionPopup ───────────────────────────────────────────────────

TSuggestionPopup::TSuggestionPopup(const TRect &bounds)
    : TView(bounds)
{
    options |= ofSelectable | ofTopSelect;
    eventMask = evKeyDown | evMouseDown;
    growMode = 0;
    state |= sfVisible;
    showCursor();
}

void TSuggestionPopup::setItems(const std::vector<std::string> &newItems)
{
    items = newItems;
    focused = 0;
    topItem = 0;
    drawView();
}

void TSuggestionPopup::draw()
{
    TDrawBuffer b;
    TColorAttr norm   = TColorAttr(0x70);   // light grey on dark grey  (good contrast)
    TColorAttr foc    = TColorAttr(0x1F);   // bright white on blue
    TColorAttr border = TColorAttr(0x70);

    int rows = rowsVisible();
    for (int row = 0; row < (int)size.y; row++) {
        b.moveChar(0, ' ', norm, size.x);
        int idx = topItem + row;
        if (idx >= 0 && idx < (int)items.size()) {
            const std::string &t = items[idx];
            TColorAttr c = (idx == focused) ? foc : norm;
            b.moveChar(0, ' ', c, size.x);
            int w = (int)t.size();
            if (w > size.x - 2) w = size.x - 2;
            b.moveStr(1, TStringView(t.data(), w), c);
        }
        // right border
        b.moveChar(size.x - 1, ' ', border, 1);
        writeLine(0, row, size.x, 1, b);
    }
    (void)rows;
}

void TSuggestionPopup::handleEvent(TEvent &event)
{
    TView::handleEvent(event);

    if (event.what == evKeyDown) {
        switch (event.keyDown.keyCode) {
        case kbDown:
            if (focused + 1 < (int)items.size()) {
                focused++;
                if (focused - topItem >= rowsVisible())
                    topItem = focused - rowsVisible() + 1;
                drawView();
            }
            clearEvent(event);
            return;
        case kbUp:
            if (focused > 0) {
                focused--;
                if (focused < topItem) topItem = focused;
                drawView();
            }
            clearEvent(event);
            return;
        case kbPgDn:
            focused = std::min((int)items.size() - 1, focused + rowsVisible());
            if (focused - topItem >= rowsVisible())
                topItem = focused - rowsVisible() + 1;
            drawView();
            clearEvent(event);
            return;
        case kbPgUp:
            focused = std::max(0, focused - rowsVisible());
            if (focused < topItem) topItem = focused;
            drawView();
            clearEvent(event);
            return;
        case kbEnter:
        case kbTab:
            if (focused >= 0 && focused < (int)items.size()) {
                acceptedText = items[focused];
                lastResult = ResAccepted;
            } else {
                lastResult = ResCancelled;
            }
            clearEvent(event);
            return;
        case kbEsc:
            lastResult = ResCancelled;
            clearEvent(event);
            return;
        }
        // Any other key — bubble out so editor handles it (closes popup)
    }
}
