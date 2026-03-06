#include "lexer.h"
#include <cstring>
#include <cctype>

const std::unordered_set<std::string> CssLexer::properties = {
    "color", "background", "background-color", "background-image",
    "font", "font-size", "font-weight", "font-family", "font-style",
    "margin", "margin-top", "margin-right", "margin-bottom", "margin-left",
    "padding", "padding-top", "padding-right", "padding-bottom", "padding-left",
    "border", "border-radius", "border-color", "border-width", "border-style",
    "width", "height", "max-width", "max-height", "min-width", "min-height",
    "display", "position", "top", "right", "bottom", "left",
    "float", "clear", "overflow", "overflow-x", "overflow-y",
    "flex", "flex-direction", "flex-wrap", "justify-content", "align-items",
    "grid", "grid-template-columns", "grid-template-rows", "gap",
    "text-align", "text-decoration", "text-transform", "line-height",
    "opacity", "z-index", "cursor", "transition", "transform", "animation",
    "box-shadow", "box-sizing", "content", "visibility", "white-space",
};

const std::unordered_set<std::string> CssLexer::pseudoClasses = {
    "hover", "focus", "active", "visited", "first-child", "last-child",
    "nth-child", "before", "after", "not", "first-of-type", "last-of-type",
    "root", "empty", "checked", "disabled", "enabled", "placeholder",
};

LexerState CssLexer::tokenizeLine(const char *line, int length,
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

        // Selectors: . # @ and identifiers at start
        if (ch == '.' || ch == '#') {
            int start = i++;
            while (i < length && (isalnum(line[i]) || line[i] == '-' || line[i] == '_'))
                i++;
            tokens.push_back({start, i - start, TokenType::Selector});
            continue;
        }

        if (ch == '@') {
            int start = i++;
            while (i < length && (isalnum(line[i]) || line[i] == '-'))
                i++;
            tokens.push_back({start, i - start, TokenType::Preprocessor});
            continue;
        }

        // Pseudo-classes :
        if (ch == ':') {
            int start = i++;
            if (i < length && line[i] == ':') i++;
            while (i < length && (isalnum(line[i]) || line[i] == '-'))
                i++;
            tokens.push_back({start, i - start, TokenType::PseudoClass});
            continue;
        }

        // Numbers (including units)
        if (isdigit(ch)) {
            int start = i;
            while (i < length && (isalnum(line[i]) || line[i] == '.' || line[i] == '%'))
                i++;
            tokens.push_back({start, i - start, TokenType::Number});
            continue;
        }

        // Words (properties, selectors, values)
        if (isalpha(ch) || ch == '-' || ch == '_') {
            int start = i;
            while (i < length && (isalnum(line[i]) || line[i] == '-' || line[i] == '_'))
                i++;
            std::string word(line + start, i - start);
            if (properties.count(word))
                tokens.push_back({start, i - start, TokenType::Property});
            else
                tokens.push_back({start, i - start, TokenType::Normal});
            continue;
        }

        // Brackets
        if (ch == '{' || ch == '}' || ch == '(' || ch == ')' ||
            ch == '[' || ch == ']') {
            tokens.push_back({i, 1, TokenType::Bracket});
            i++;
            continue;
        }

        // Operators
        if (ch == ';' || ch == ',' || ch == '!' || ch == '>' || ch == '~' ||
            ch == '+' || ch == '=') {
            tokens.push_back({i, 1, TokenType::Operator});
            i++;
            continue;
        }

        tokens.push_back({i, 1, TokenType::Normal});
        i++;
    }

    return state;
}
