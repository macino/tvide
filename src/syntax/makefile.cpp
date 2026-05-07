#include "lexer.h"
#include <cctype>

static const std::unordered_set<std::string> kMakeKeywords = {
    "include","-include","sinclude","define","endef","ifeq","ifneq","ifdef",
    "ifndef","else","endif","export","unexport","override","private","vpath"
};

LexerState MakefileLexer::tokenizeLine(const char *line, int length,
                                       LexerState inState,
                                       std::vector<Token> &tokens)
{
    int i = 0;
    LexerState state = inState;

    int j = 0;
    while (j < length && (line[j] == ' ' || line[j] == '\t')) j++;
    if (j < length && line[j] == '#') {
        tokens.push_back({j, length - j, TokenType::Comment});
        return state;
    }

    // Highlight target name before `:`
    int colon = -1;
    for (int k = 0; k < length; k++) {
        if (line[k] == ':') { colon = k; break; }
        if (line[k] == '#') break;
    }

    while (i < length) {
        char ch = line[i];

        if (ch == '#') {
            tokens.push_back({i, length - i, TokenType::Comment});
            return state;
        }

        if (ch == '$') {
            int start = i++;
            if (i < length && (line[i] == '(' || line[i] == '{')) {
                char open = line[i];
                char close = (open == '(') ? ')' : '}';
                i++;
                while (i < length && line[i] != close) i++;
                if (i < length) i++;
            } else if (i < length) {
                i++;
            }
            tokens.push_back({start, i - start, TokenType::Variable});
            continue;
        }

        if (ch == '"' || ch == '\'') {
            char q = ch;
            int start = i++;
            while (i < length && line[i] != q) i++;
            if (i < length) i++;
            tokens.push_back({start, i - start, TokenType::String});
            continue;
        }

        if (isalpha((unsigned char)ch) || ch == '_' || ch == '-') {
            int start = i;
            while (i < length &&
                   (isalnum((unsigned char)line[i]) || line[i] == '_' ||
                    line[i] == '-' || line[i] == '.'))
                i++;
            std::string word(line + start, i - start);
            if (kMakeKeywords.count(word))
                tokens.push_back({start, i - start, TokenType::Keyword});
            else if (colon >= 0 && start <= colon)
                tokens.push_back({start, i - start, TokenType::Function});
            else
                tokens.push_back({start, i - start, TokenType::Normal});
            continue;
        }

        if (ch == ':' || ch == '=' || ch == '?' || ch == '+' || ch == '*' ||
            ch == '<' || ch == '>' || ch == '|' || ch == '@' || ch == ';') {
            tokens.push_back({i, 1, TokenType::Operator});
            i++;
            continue;
        }

        i++;
        tokens.push_back({i - 1, 1, TokenType::Normal});
    }

    return state;
}
