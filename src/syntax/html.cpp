#include "lexer.h"
#include <cstring>
#include <cctype>

LexerState HtmlLexer::tokenizeLine(const char *line, int length,
                                   LexerState inState,
                                   std::vector<Token> &tokens)
{
    int i = 0;
    LexerState state = inState;

    while (i < length) {
        // Block comment continuation
        if (state == LexerState::InBlockComment) {
            int start = i;
            while (i < length) {
                if (i + 2 < length && line[i] == '-' && line[i+1] == '-' && line[i+2] == '>') {
                    i += 3;
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

        // HTML comment <!--
        if (ch == '<' && i + 3 < length &&
            line[i+1] == '!' && line[i+2] == '-' && line[i+3] == '-') {
            int start = i;
            i += 4;
            state = LexerState::InBlockComment;
            while (i < length) {
                if (i + 2 < length && line[i] == '-' && line[i+1] == '-' && line[i+2] == '>') {
                    i += 3;
                    state = LexerState::Normal;
                    break;
                }
                i++;
            }
            tokens.push_back({start, i - start, TokenType::Comment});
            continue;
        }

        // PHP block embedded in HTML
        if (ch == '<' && i + 1 < length && line[i+1] == '?') {
            int tagLen = 2;
            if (i + 2 < length && line[i+2] == '=') tagLen = 3;
            else if (i + 4 < length && strncmp(line + i + 2, "php", 3) == 0) tagLen = 5;
            tokens.push_back({i, tagLen, TokenType::PhpDelimiter});
            i += tagLen;
            continue;
        }

        if (ch == '?' && i + 1 < length && line[i+1] == '>') {
            tokens.push_back({i, 2, TokenType::PhpDelimiter});
            i += 2;
            continue;
        }

        // Tags
        if (ch == '<') {
            tokens.push_back({i, 1, TokenType::Tag});
            i++;

            // Closing tag slash
            if (i < length && line[i] == '/') {
                tokens.push_back({i, 1, TokenType::Tag});
                i++;
            }

            // Tag name
            int nameStart = i;
            while (i < length && (isalnum(line[i]) || line[i] == '-' || line[i] == '_'))
                i++;
            if (i > nameStart)
                tokens.push_back({nameStart, i - nameStart, TokenType::Tag});

            // Attributes
            while (i < length && line[i] != '>') {
                // Skip whitespace
                if (isspace(line[i])) {
                    tokens.push_back({i, 1, TokenType::Normal});
                    i++;
                    continue;
                }

                // Self-closing />
                if (line[i] == '/' && i + 1 < length && line[i+1] == '>') {
                    tokens.push_back({i, 2, TokenType::Tag});
                    i += 2;
                    break;
                }

                // Attribute value
                if (line[i] == '=') {
                    tokens.push_back({i, 1, TokenType::Operator});
                    i++;
                    // Skip whitespace
                    while (i < length && isspace(line[i])) {
                        tokens.push_back({i, 1, TokenType::Normal});
                        i++;
                    }
                    // Quoted string
                    if (i < length && (line[i] == '"' || line[i] == '\'')) {
                        char quote = line[i];
                        int strStart = i++;
                        while (i < length && line[i] != quote)
                            i++;
                        if (i < length) i++;
                        tokens.push_back({strStart, i - strStart, TokenType::AttrValue});
                    }
                    continue;
                }

                // Attribute name
                int attrStart = i;
                while (i < length && !isspace(line[i]) &&
                       line[i] != '=' && line[i] != '>' && line[i] != '/')
                    i++;
                if (i > attrStart)
                    tokens.push_back({attrStart, i - attrStart, TokenType::Attribute});
            }

            if (i < length && line[i] == '>') {
                tokens.push_back({i, 1, TokenType::Tag});
                i++;
            }
            continue;
        }

        // HTML entities &...;
        if (ch == '&') {
            int start = i++;
            while (i < length && line[i] != ';' && !isspace(line[i]) && (i - start) < 12)
                i++;
            if (i < length && line[i] == ';')
                i++;
            tokens.push_back({start, i - start, TokenType::HtmlEntity});
            continue;
        }

        // Normal text
        tokens.push_back({i, 1, TokenType::Normal});
        i++;
    }

    return state;
}
