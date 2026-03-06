#include "lexer.h"
#include <cctype>
#include <cstring>

LexerState YamlLexer::tokenizeLine(const char *line, int length,
                                   LexerState inState,
                                   std::vector<Token> &tokens)
{
    int i = 0;
    (void)inState;

    if (length == 0)
        return LexerState::Normal;

    // Comment
    if (line[0] == '#') {
        tokens.push_back({0, length, TokenType::Comment});
        return LexerState::Normal;
    }

    // Document markers
    if (length >= 3 && ((line[0] == '-' && line[1] == '-' && line[2] == '-') ||
                        (line[0] == '.' && line[1] == '.' && line[2] == '.'))) {
        tokens.push_back({0, length, TokenType::Preprocessor});
        return LexerState::Normal;
    }

    while (i < length) {
        char ch = line[i];

        // Skip leading whitespace
        if (isspace(ch)) {
            tokens.push_back({i, 1, TokenType::Normal});
            i++;
            continue;
        }

        // Key: value
        if (isalpha(ch) || ch == '_' || ch == '-') {
            int start = i;
            while (i < length && line[i] != ':' && line[i] != '#')
                i++;
            if (i < length && line[i] == ':') {
                tokens.push_back({start, i - start, TokenType::Key});
                tokens.push_back({i, 1, TokenType::Operator});
                i++;
                // Rest is value
                if (i < length) {
                    // Check for comment
                    int valStart = i;
                    while (i < length && line[i] != '#')
                        i++;
                    if (i > valStart)
                        tokens.push_back({valStart, i - valStart, TokenType::Value});
                    if (i < length && line[i] == '#')
                        tokens.push_back({i, length - i, TokenType::Comment});
                }
                continue;
            }
            tokens.push_back({start, i - start, TokenType::Normal});
            continue;
        }

        // List items
        if (ch == '-' && i + 1 < length && line[i+1] == ' ') {
            tokens.push_back({i, 1, TokenType::Keyword});
            i++;
            continue;
        }

        // Strings
        if (ch == '"' || ch == '\'') {
            char q = ch;
            int start = i++;
            while (i < length && line[i] != q) {
                if (line[i] == '\\' && i + 1 < length) i++;
                i++;
            }
            if (i < length) i++;
            tokens.push_back({start, i - start, TokenType::String});
            continue;
        }

        // Comments
        if (ch == '#') {
            tokens.push_back({i, length - i, TokenType::Comment});
            return LexerState::Normal;
        }

        // Numbers
        if (isdigit(ch)) {
            int start = i;
            while (i < length && (isalnum(line[i]) || line[i] == '.' || line[i] == '-'))
                i++;
            tokens.push_back({start, i - start, TokenType::Number});
            continue;
        }

        // Anchors & aliases
        if (ch == '&' || ch == '*') {
            int start = i++;
            while (i < length && (isalnum(line[i]) || line[i] == '_' || line[i] == '-'))
                i++;
            tokens.push_back({start, i - start, TokenType::Variable});
            continue;
        }

        tokens.push_back({i, 1, TokenType::Normal});
        i++;
    }

    return LexerState::Normal;
}
