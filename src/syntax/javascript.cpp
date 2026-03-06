#include "lexer.h"
#include <cstring>
#include <cctype>

const std::unordered_set<std::string> JavaScriptLexer::keywords = {
    "abstract", "arguments", "async", "await", "break", "case", "catch",
    "class", "const", "continue", "debugger", "default", "delete", "do",
    "else", "enum", "eval", "export", "extends", "false", "finally", "for",
    "function", "get", "if", "implements", "import", "in", "instanceof",
    "interface", "let", "new", "null", "of", "package", "private",
    "protected", "public", "return", "set", "static", "super", "switch",
    "this", "throw", "true", "try", "typeof", "undefined", "var", "void",
    "while", "with", "yield", "from", "as",
};

const std::unordered_set<std::string> JavaScriptLexer::types = {
    "Array", "Boolean", "Date", "Error", "Function", "Map", "Number",
    "Object", "Promise", "Proxy", "RegExp", "Set", "String", "Symbol",
    "WeakMap", "WeakSet", "BigInt", "Int8Array", "Uint8Array",
};

const std::unordered_set<std::string> JavaScriptLexer::builtins = {
    "console", "document", "window", "global", "process", "require",
    "module", "exports", "setTimeout", "setInterval", "clearTimeout",
    "clearInterval", "fetch", "JSON", "Math", "parseInt", "parseFloat",
    "isNaN", "isFinite", "decodeURI", "encodeURI", "alert",
};

LexerState JavaScriptLexer::tokenizeLine(const char *line, int length,
                                          LexerState inState,
                                          std::vector<Token> &tokens)
{
    int i = 0;
    LexerState state = inState;

    while (i < length) {
        if (state == LexerState::InBlockComment) {
            int start = i;
            while (i < length) {
                if (i + 1 < length && line[i] == '*' && line[i+1] == '/') {
                    i += 2;
                    tokens.push_back({start, i - start, TokenType::Comment});
                    state = LexerState::Normal;
                    break;
                }
                i++;
            }
            if (state == LexerState::InBlockComment)
                tokens.push_back({start, i - start, TokenType::Comment});
            continue;
        }

        if (state == LexerState::InTemplateLiteral) {
            int start = i;
            while (i < length) {
                if (line[i] == '\\' && i + 1 < length) {
                    i += 2;
                    continue;
                }
                if (line[i] == '`') {
                    i++;
                    tokens.push_back({start, i - start, TokenType::String});
                    state = LexerState::Normal;
                    break;
                }
                i++;
            }
            if (state == LexerState::InTemplateLiteral)
                tokens.push_back({start, i - start, TokenType::String});
            continue;
        }

        char ch = line[i];

        // Comments
        if (ch == '/' && i + 1 < length) {
            if (line[i+1] == '/') {
                tokens.push_back({i, length - i, TokenType::Comment});
                return state;
            }
            if (line[i+1] == '*') {
                int start = i;
                i += 2;
                state = LexerState::InBlockComment;
                while (i < length) {
                    if (i + 1 < length && line[i] == '*' && line[i+1] == '/') {
                        i += 2;
                        state = LexerState::Normal;
                        break;
                    }
                    i++;
                }
                tokens.push_back({start, i - start, TokenType::Comment});
                continue;
            }
        }

        // Template literals
        if (ch == '`') {
            int start = i++;
            state = LexerState::InTemplateLiteral;
            while (i < length) {
                if (line[i] == '\\' && i + 1 < length) {
                    i += 2;
                    continue;
                }
                if (line[i] == '`') {
                    i++;
                    state = LexerState::Normal;
                    break;
                }
                i++;
            }
            tokens.push_back({start, i - start, TokenType::String});
            continue;
        }

        // Strings
        if (ch == '"' || ch == '\'') {
            char quote = ch;
            int start = i++;
            while (i < length && line[i] != quote) {
                if (line[i] == '\\' && i + 1 < length) i++;
                i++;
            }
            if (i < length) i++;
            tokens.push_back({start, i - start, TokenType::String});
            continue;
        }

        // Numbers
        if (isdigit(ch) || (ch == '.' && i + 1 < length && isdigit(line[i+1]))) {
            int start = i;
            if (ch == '0' && i + 1 < length && (line[i+1] == 'x' || line[i+1] == 'X'))
                i += 2;
            while (i < length && (isalnum(line[i]) || line[i] == '.' || line[i] == '_'))
                i++;
            tokens.push_back({start, i - start, TokenType::Number});
            continue;
        }

        // Arrow function =>
        if (ch == '=' && i + 1 < length && line[i+1] == '>') {
            tokens.push_back({i, 2, TokenType::Keyword});
            i += 2;
            continue;
        }

        // Words
        if (isalpha(ch) || ch == '_' || ch == '$') {
            int start = i;
            while (i < length && (isalnum(line[i]) || line[i] == '_' || line[i] == '$'))
                i++;
            std::string word(line + start, i - start);
            if (keywords.count(word))
                tokens.push_back({start, i - start, TokenType::Keyword});
            else if (types.count(word))
                tokens.push_back({start, i - start, TokenType::Type});
            else if (builtins.count(word))
                tokens.push_back({start, i - start, TokenType::Builtin});
            else {
                int j = i;
                while (j < length && isspace(line[j])) j++;
                if (j < length && line[j] == '(')
                    tokens.push_back({start, i - start, TokenType::Function});
                else
                    tokens.push_back({start, i - start, TokenType::Normal});
            }
            continue;
        }

        // Brackets
        if (ch == '(' || ch == ')' || ch == '[' || ch == ']' ||
            ch == '{' || ch == '}') {
            tokens.push_back({i, 1, TokenType::Bracket});
            i++;
            continue;
        }

        // Operators
        if (ch == '=' || ch == '+' || ch == '-' || ch == '*' || ch == '/' ||
            ch == '<' || ch == '>' || ch == '!' || ch == '&' || ch == '|' ||
            ch == '^' || ch == '%' || ch == '~' || ch == '?' || ch == ':' ||
            ch == '.' || ch == ',' || ch == ';') {
            tokens.push_back({i, 1, TokenType::Operator});
            i++;
            continue;
        }

        tokens.push_back({i, 1, TokenType::Normal});
        i++;
    }

    return state;
}
