#include "lexer.h"
#include <cctype>
#include <cstring>
#include <memory>

LexerState MarkdownLexer::tokenizeLine(const char *line, int length,
                                        LexerState inState,
                                        std::vector<Token> &tokens)
{
    if (length == 0)
        return inState == LexerState::InMultiLineString
               ? LexerState::InMultiLineString : LexerState::Normal;

    int i = 0;

    auto langLexer = [](const std::string &lang) -> SyntaxLexer * {
        if (lang.empty()) return nullptr;
        std::string e = lang;
        for (auto &c : e) c = (char)tolower((unsigned char)c);
        if (e == "php" || e == "phtml") return new PhpLexer();
        if (e == "html" || e == "htm")  return new HtmlLexer();
        if (e == "css" || e == "scss" || e == "less") return new CssLexer();
        if (e == "js" || e == "javascript" || e == "jsx") return new JavaScriptLexer();
        if (e == "ts" || e == "typescript" || e == "tsx") return new TypeScriptLexer();
        if (e == "json") return new JsonLexer();
        if (e == "yaml" || e == "yml") return new YamlLexer();
        if (e == "sql")  return new SqlLexer();
        if (e == "xml" || e == "svg")  return new XmlLexer();
        if (e == "vue")  return new VueLexer();
        if (e == "py" || e == "python") return new PythonLexer();
        if (e == "sh" || e == "bash" || e == "shell" || e == "zsh") return new ShellLexer();
        if (e == "rb" || e == "ruby")   return new RubyLexer();
        if (e == "c" || e == "cpp" || e == "c++" || e == "cxx") return new CppLexer();
        if (e == "java")    return new JavaLexer();
        if (e == "cs" || e == "csharp") return new CSharpLexer();
        if (e == "go")      return new GoLexer();
        if (e == "rs" || e == "rust")   return new RustLexer();
        if (e == "kt" || e == "kotlin") return new KotlinLexer();
        if (e == "swift")   return new SwiftLexer();
        if (e == "lua")     return new LuaLexer();
        if (e == "dockerfile") return new DockerfileLexer();
        if (e == "make" || e == "makefile") return new MakefileLexer();
        if (e == "ini" || e == "toml" || e == "env") return new IniLexer();
        return nullptr;
    };

    // Fenced code block continuation
    if (inState == LexerState::InMultiLineString) {
        if (length >= 3 && line[0] == '`' && line[1] == '`' && line[2] == '`') {
            tokens.push_back({0, length, TokenType::Preprocessor});
            codeLang.clear();
            return LexerState::Normal;
        }
        std::unique_ptr<SyntaxLexer> sub(langLexer(codeLang));
        if (sub) {
            sub->tokenizeLine(line, length, LexerState::Normal, tokens);
        } else {
            tokens.push_back({0, length, TokenType::String});
        }
        return LexerState::InMultiLineString;
    }

    // Fenced code block start
    if (length >= 3 && line[0] == '`' && line[1] == '`' && line[2] == '`') {
        // Parse language tag after the fence
        codeLang.clear();
        int p = 3;
        while (p < length && (isalnum((unsigned char)line[p]) ||
                              line[p] == '+' || line[p] == '-' ||
                              line[p] == '_' || line[p] == '#'))
            codeLang.push_back(line[p++]);
        tokens.push_back({0, length, TokenType::Preprocessor});
        return LexerState::InMultiLineString;
    }

    // Headings: # ... ######
    if (line[0] == '#') {
        tokens.push_back({0, length, TokenType::Heading});
        return LexerState::Normal;
    }

    // Blockquote: > text
    if (line[0] == '>') {
        // Mark the > prefix as keyword, rest as comment (quoted text)
        int prefixLen = 1;
        while (prefixLen < length && (line[prefixLen] == '>' || line[prefixLen] == ' '))
            prefixLen++;
        tokens.push_back({0, prefixLen, TokenType::Keyword});
        if (prefixLen < length)
            tokens.push_back({prefixLen, length - prefixLen, TokenType::Comment});
        return LexerState::Normal;
    }

    // Horizontal rule (---, ***, ___)
    if (length >= 3 && (line[0] == '-' || line[0] == '*' || line[0] == '_')) {
        bool isRule = true;
        int count = 0;
        for (int j = 0; j < length; j++) {
            if (line[j] == line[0]) count++;
            else if (!isspace(line[j])) { isRule = false; break; }
        }
        if (isRule && count >= 3) {
            tokens.push_back({0, length, TokenType::Operator});
            return LexerState::Normal;
        }
    }

    // Check for unordered list item at start: - , * , +
    int contentStart = 0;
    while (contentStart < length && line[contentStart] == ' ') contentStart++;
    if (contentStart < length) {
        char ch0 = line[contentStart];
        // Unordered list marker
        if ((ch0 == '-' || ch0 == '*' || ch0 == '+') &&
            contentStart + 1 < length && line[contentStart + 1] == ' ') {
            tokens.push_back({contentStart, 2, TokenType::Keyword});
            i = contentStart + 2;
        }
        // Numbered list: 1. 2. etc.
        else if (isdigit(ch0)) {
            int numEnd = contentStart;
            while (numEnd < length && isdigit(line[numEnd])) numEnd++;
            if (numEnd < length && line[numEnd] == '.' &&
                numEnd + 1 < length && line[numEnd + 1] == ' ') {
                tokens.push_back({contentStart, numEnd - contentStart + 2, TokenType::Keyword});
                i = numEnd + 2;
            }
        }
        // Leading whitespace before list marker
        if (contentStart > 0 && i == 0) {
            // Not a list item — i stays at 0
        } else if (contentStart > 0 && i > 0) {
            tokens.push_back({0, contentStart, TokenType::Normal});
        }
    }

    // | Table row detection
    if (i == 0 && length > 0 && line[0] == '|') {
        // Separator row: |---|---|
        bool isSep = true;
        for (int j = 0; j < length; j++) {
            if (line[j] != '|' && line[j] != '-' && line[j] != ':' && line[j] != ' ') {
                isSep = false;
                break;
            }
        }
        if (isSep) {
            tokens.push_back({0, length, TokenType::Operator});
            return LexerState::Normal;
        }
        // Data row: highlight | as operator
        while (i < length) {
            if (line[i] == '|') {
                tokens.push_back({i, 1, TokenType::Operator});
                i++;
            } else {
                int start = i;
                while (i < length && line[i] != '|') i++;
                tokens.push_back({start, i - start, TokenType::Normal});
            }
        }
        return LexerState::Normal;
    }

    // Process inline formatting
    while (i < length) {
        char ch = line[i];

        // Strikethrough ~~...~~
        if (ch == '~' && i + 1 < length && line[i+1] == '~') {
            int start = i;
            i += 2;
            while (i + 1 < length && !(line[i] == '~' && line[i+1] == '~'))
                i++;
            if (i + 1 < length) i += 2;
            tokens.push_back({start, i - start, TokenType::Comment});
            continue;
        }

        // Bold **...** or __...__
        if ((ch == '*' || ch == '_') && i + 1 < length && line[i+1] == ch) {
            char marker = ch;
            int start = i;
            i += 2;
            while (i + 1 < length && !(line[i] == marker && line[i+1] == marker))
                i++;
            if (i + 1 < length) i += 2;
            tokens.push_back({start, i - start, TokenType::Bold});
            continue;
        }

        // Italic *...* or _..._  (single marker, not at word boundary for _)
        if ((ch == '*' || ch == '_') && i + 1 < length && line[i+1] != ' ') {
            char marker = ch;
            int start = i;
            i++;
            while (i < length && line[i] != marker) i++;
            if (i < length) i++;
            // Only mark as italic if we found the closing marker
            if (i > start + 1) {
                tokens.push_back({start, i - start, TokenType::Variable});
                continue;
            }
        }

        // Inline code `...`
        if (ch == '`') {
            int start = i++;
            while (i < length && line[i] != '`') i++;
            if (i < length) i++;
            tokens.push_back({start, i - start, TokenType::String});
            continue;
        }

        // Images ![alt](url)
        if (ch == '!' && i + 1 < length && line[i+1] == '[') {
            int start = i;
            i += 2;
            while (i < length && line[i] != ']') i++;
            if (i < length) i++;
            if (i < length && line[i] == '(') {
                i++;
                while (i < length && line[i] != ')') i++;
                if (i < length) i++;
            }
            tokens.push_back({start, i - start, TokenType::Tag});
            continue;
        }

        // Links [text](url) or [text][ref]
        if (ch == '[') {
            int start = i++;
            while (i < length && line[i] != ']') i++;
            if (i < length) i++;
            if (i < length && (line[i] == '(' || line[i] == '[')) {
                char closer = line[i] == '(' ? ')' : ']';
                i++;
                while (i < length && line[i] != closer) i++;
                if (i < length) i++;
            }
            tokens.push_back({start, i - start, TokenType::Link});
            continue;
        }

        tokens.push_back({i, 1, TokenType::Normal});
        i++;
    }

    return LexerState::Normal;
}
