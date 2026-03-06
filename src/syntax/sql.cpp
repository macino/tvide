#include "lexer.h"
#include <cctype>
#include <cstring>
#include <algorithm>

const std::unordered_set<std::string> SqlLexer::keywords = {
    "select", "from", "where", "and", "or", "not", "in", "like", "between",
    "is", "null", "join", "inner", "outer", "left", "right", "cross",
    "on", "as", "order", "by", "asc", "desc", "group", "having", "limit",
    "offset", "union", "all", "distinct", "insert", "into", "values",
    "update", "set", "delete", "create", "table", "alter", "drop",
    "index", "view", "trigger", "procedure", "function", "begin", "end",
    "if", "else", "then", "case", "when", "exists", "primary", "key",
    "foreign", "references", "constraint", "default", "auto_increment",
    "not", "unique", "check", "cascade", "restrict", "grant", "revoke",
    "commit", "rollback", "transaction", "database", "use", "show",
    "describe", "explain", "truncate", "replace", "with", "recursive",
    "returns", "declare", "cursor", "fetch", "open", "close", "return",
    // Types
    "int", "integer", "bigint", "smallint", "tinyint", "decimal", "numeric",
    "float", "double", "real", "char", "varchar", "text", "blob", "date",
    "datetime", "timestamp", "boolean", "enum", "json", "serial",
};

LexerState SqlLexer::tokenizeLine(const char *line, int length,
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

        char ch = line[i];

        // Line comment --
        if (ch == '-' && i + 1 < length && line[i+1] == '-') {
            tokens.push_back({i, length - i, TokenType::Comment});
            return state;
        }

        // Block comment
        if (ch == '/' && i + 1 < length && line[i+1] == '*') {
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

        // Strings
        if (ch == '\'' || ch == '"') {
            char q = ch; int start = i++;
            while (i < length && line[i] != q) {
                if (line[i] == '\\' && i + 1 < length) i++;
                i++;
            }
            if (i < length) i++;
            tokens.push_back({start, i - start, TokenType::String});
            continue;
        }

        // Backtick identifiers
        if (ch == '`') {
            int start = i++;
            while (i < length && line[i] != '`') i++;
            if (i < length) i++;
            tokens.push_back({start, i - start, TokenType::Variable});
            continue;
        }

        // Numbers
        if (isdigit(ch)) {
            int start = i;
            while (i < length && (isdigit(line[i]) || line[i] == '.'))
                i++;
            tokens.push_back({start, i - start, TokenType::Number});
            continue;
        }

        // Words
        if (isalpha(ch) || ch == '_') {
            int start = i;
            while (i < length && (isalnum(line[i]) || line[i] == '_'))
                i++;
            std::string word(line + start, i - start);
            // Case-insensitive keyword check
            std::string lower = word;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (keywords.count(lower))
                tokens.push_back({start, i - start, TokenType::Keyword});
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

        // Brackets, operators
        if (ch == '(' || ch == ')') {
            tokens.push_back({i, 1, TokenType::Bracket}); i++; continue;
        }
        if (ch == ',' || ch == ';' || ch == '=' || ch == '<' || ch == '>' ||
            ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '.') {
            tokens.push_back({i, 1, TokenType::Operator}); i++; continue;
        }

        tokens.push_back({i, 1, TokenType::Normal});
        i++;
    }

    return state;
}
