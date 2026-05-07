#include "lexer.h"
#include <cctype>

LexerState IniLexer::tokenizeLine(const char *line, int length,
                                  LexerState inState,
                                  std::vector<Token> &tokens)
{
    int i = 0;
    LexerState state = inState;

    int j = 0;
    while (j < length && (line[j] == ' ' || line[j] == '\t')) j++;
    if (j < length && (line[j] == '#' || line[j] == ';')) {
        tokens.push_back({j, length - j, TokenType::Comment});
        return state;
    }

    if (j < length && line[j] == '[') {
        int end = j + 1;
        while (end < length && line[end] != ']') end++;
        if (end < length) end++;
        tokens.push_back({j, end - j, TokenType::Heading});
        i = end;
    }

    int eqPos = -1;
    for (int k = i; k < length; k++) {
        if (line[k] == '#' || line[k] == ';') break;
        if (line[k] == '=' || line[k] == ':') { eqPos = k; break; }
    }

    while (i < length) {
        char ch = line[i];

        if (ch == '#' || ch == ';') {
            tokens.push_back({i, length - i, TokenType::Comment});
            return state;
        }

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

        if (isdigit((unsigned char)ch) ||
            (ch == '-' && i + 1 < length && isdigit((unsigned char)line[i+1]))) {
            int start = i++;
            while (i < length && (isalnum((unsigned char)line[i]) ||
                                  line[i] == '.' || line[i] == '_'))
                i++;
            tokens.push_back({start, i - start, TokenType::Number});
            continue;
        }

        if (isalpha((unsigned char)ch) || ch == '_') {
            int start = i;
            while (i < length &&
                   (isalnum((unsigned char)line[i]) || line[i] == '_' ||
                    line[i] == '-' || line[i] == '.'))
                i++;
            if (eqPos >= 0 && start < eqPos)
                tokens.push_back({start, i - start, TokenType::Key});
            else {
                std::string word(line + start, i - start);
                if (word == "true" || word == "false" || word == "null" ||
                    word == "yes" || word == "no" || word == "on" || word == "off")
                    tokens.push_back({start, i - start, TokenType::Keyword});
                else
                    tokens.push_back({start, i - start, TokenType::Value});
            }
            continue;
        }

        if (ch == '=' || ch == ':' || ch == ',' || ch == '[' || ch == ']' ||
            ch == '{' || ch == '}') {
            tokens.push_back({i, 1, TokenType::Operator});
            i++;
            continue;
        }

        i++;
        tokens.push_back({i - 1, 1, TokenType::Normal});
    }

    return state;
}
