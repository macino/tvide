#include "lexer.h"
#include <cstring>
#include <cctype>

const std::unordered_set<std::string> TypeScriptLexer::keywords = {
    "abstract", "any", "arguments", "as", "async", "await", "break",
    "case", "catch", "class", "const", "constructor", "continue",
    "debugger", "declare", "default", "delete", "do", "else", "enum",
    "export", "extends", "false", "finally", "for", "from", "function",
    "get", "if", "implements", "import", "in", "infer", "instanceof",
    "interface", "is", "keyof", "let", "module", "namespace", "new",
    "null", "of", "package", "private", "protected", "public", "readonly",
    "return", "satisfies", "set", "static", "super", "switch", "this",
    "throw", "true", "try", "type", "typeof", "undefined", "unique",
    "var", "void", "while", "with", "yield",
};

const std::unordered_set<std::string> TypeScriptLexer::types = {
    "Array", "Boolean", "Date", "Error", "Function", "Map", "Number",
    "Object", "Promise", "Proxy", "Record", "RegExp", "Set", "String",
    "Symbol", "WeakMap", "WeakSet", "BigInt", "Partial", "Required",
    "Readonly", "Pick", "Omit", "Exclude", "Extract", "NonNullable",
    "ReturnType", "InstanceType", "Parameters", "Awaited",
    "string", "number", "boolean", "never", "unknown",
};

LexerState TypeScriptLexer::tokenizeLine(const char *line, int length,
                                          LexerState inState,
                                          std::vector<Token> &tokens)
{
    // TypeScript lexer reuses JavaScript logic with extra keywords/types
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
                if (line[i] == '\\' && i + 1 < length) { i += 2; continue; }
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

        if (ch == '/' && i + 1 < length) {
            if (line[i+1] == '/') {
                tokens.push_back({i, length - i, TokenType::Comment});
                return state;
            }
            if (line[i+1] == '*') {
                int start = i; i += 2;
                state = LexerState::InBlockComment;
                while (i < length) {
                    if (i + 1 < length && line[i] == '*' && line[i+1] == '/') {
                        i += 2; state = LexerState::Normal; break;
                    }
                    i++;
                }
                tokens.push_back({start, i - start, TokenType::Comment});
                continue;
            }
        }

        if (ch == '`') {
            int start = i++;
            state = LexerState::InTemplateLiteral;
            while (i < length) {
                if (line[i] == '\\' && i + 1 < length) { i += 2; continue; }
                if (line[i] == '`') { i++; state = LexerState::Normal; break; }
                i++;
            }
            tokens.push_back({start, i - start, TokenType::String});
            continue;
        }

        if (ch == '"' || ch == '\'') {
            char q = ch; int start = i++;
            while (i < length && line[i] != q) {
                if (line[i] == '\\' && i + 1 < length) i++;
                i++;
            }
            if (i < length) i++;
            tokens.push_back({start, i - start, TokenType::String});
            continue;
        }

        if (isdigit(ch) || (ch == '.' && i + 1 < length && isdigit(line[i+1]))) {
            int start = i;
            while (i < length && (isalnum(line[i]) || line[i] == '.' || line[i] == '_'))
                i++;
            tokens.push_back({start, i - start, TokenType::Number});
            continue;
        }

        if (ch == '=' && i + 1 < length && line[i+1] == '>') {
            tokens.push_back({i, 2, TokenType::Keyword});
            i += 2;
            continue;
        }

        // Decorator @
        if (ch == '@') {
            int start = i++;
            while (i < length && (isalnum(line[i]) || line[i] == '_'))
                i++;
            tokens.push_back({start, i - start, TokenType::Preprocessor});
            continue;
        }

        if (isalpha(ch) || ch == '_' || ch == '$') {
            int start = i;
            while (i < length && (isalnum(line[i]) || line[i] == '_' || line[i] == '$'))
                i++;
            std::string word(line + start, i - start);
            if (keywords.count(word))
                tokens.push_back({start, i - start, TokenType::Keyword});
            else if (types.count(word))
                tokens.push_back({start, i - start, TokenType::Type});
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

        if (ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '{' || ch == '}') {
            tokens.push_back({i, 1, TokenType::Bracket}); i++; continue;
        }

        if (ch == '=' || ch == '+' || ch == '-' || ch == '*' || ch == '/' ||
            ch == '<' || ch == '>' || ch == '!' || ch == '&' || ch == '|' ||
            ch == '^' || ch == '%' || ch == '~' || ch == '?' || ch == ':' ||
            ch == '.' || ch == ',' || ch == ';') {
            tokens.push_back({i, 1, TokenType::Operator}); i++; continue;
        }

        tokens.push_back({i, 1, TokenType::Normal});
        i++;
    }

    return state;
}
