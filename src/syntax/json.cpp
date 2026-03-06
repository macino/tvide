#include "lexer.h"
#include <cctype>

LexerState JsonLexer::tokenizeLine(const char *line, int length,
                                   LexerState inState,
                                   std::vector<Token> &tokens)
{
    int i = 0;
    (void)inState;

    while (i < length) {
        char ch = line[i];

        // Whitespace
        if (isspace(ch)) {
            tokens.push_back({i, 1, TokenType::Normal});
            i++;
            continue;
        }

        // Strings (keys or values)
        if (ch == '"') {
            int start = i++;
            while (i < length && line[i] != '"') {
                if (line[i] == '\\' && i + 1 < length) i++;
                i++;
            }
            if (i < length) i++;
            // Determine if key (followed by ':')
            int j = i;
            while (j < length && isspace(line[j])) j++;
            if (j < length && line[j] == ':')
                tokens.push_back({start, i - start, TokenType::Key});
            else
                tokens.push_back({start, i - start, TokenType::String});
            continue;
        }

        // Numbers
        if (isdigit(ch) || ch == '-') {
            int start = i;
            if (ch == '-') i++;
            while (i < length && (isdigit(line[i]) || line[i] == '.' ||
                   line[i] == 'e' || line[i] == 'E' || line[i] == '+' || line[i] == '-'))
                i++;
            tokens.push_back({start, i - start, TokenType::Number});
            continue;
        }

        // true, false, null
        if (isalpha(ch)) {
            int start = i;
            while (i < length && isalpha(line[i])) i++;
            std::string word(line + start, i - start);
            if (word == "true" || word == "false" || word == "null")
                tokens.push_back({start, i - start, TokenType::Keyword});
            else
                tokens.push_back({start, i - start, TokenType::Normal});
            continue;
        }

        // Brackets, colon, comma
        if (ch == '{' || ch == '}' || ch == '[' || ch == ']') {
            tokens.push_back({i, 1, TokenType::Bracket});
            i++;
            continue;
        }

        if (ch == ':' || ch == ',') {
            tokens.push_back({i, 1, TokenType::Operator});
            i++;
            continue;
        }

        tokens.push_back({i, 1, TokenType::Normal});
        i++;
    }

    return LexerState::Normal;
}
