#include "lexer.h"
#include <cctype>
#include <cstring>

static const std::unordered_set<std::string> kDockerKeywords = {
    "FROM","RUN","CMD","LABEL","MAINTAINER","EXPOSE","ENV","ADD","COPY",
    "ENTRYPOINT","VOLUME","USER","WORKDIR","ARG","ONBUILD","STOPSIGNAL",
    "HEALTHCHECK","SHELL"
};

LexerState DockerfileLexer::tokenizeLine(const char *line, int length,
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

    bool atLineStart = true;
    while (i < length) {
        char ch = line[i];

        if (atLineStart && (ch == ' ' || ch == '\t')) { i++; continue; }

        if (atLineStart && (isupper((unsigned char)ch) || ch == '_')) {
            int start = i;
            while (i < length && (isalpha((unsigned char)line[i]) || line[i] == '_'))
                i++;
            std::string word(line + start, i - start);
            if (kDockerKeywords.count(word))
                tokens.push_back({start, i - start, TokenType::Keyword});
            else
                tokens.push_back({start, i - start, TokenType::Normal});
            atLineStart = false;
            continue;
        }
        atLineStart = false;

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

        if (ch == '$') {
            int start = i++;
            if (i < length && line[i] == '{') {
                while (i < length && line[i] != '}') i++;
                if (i < length) i++;
            } else {
                while (i < length && (isalnum((unsigned char)line[i]) || line[i] == '_'))
                    i++;
            }
            tokens.push_back({start, i - start, TokenType::Variable});
            continue;
        }

        if (isdigit((unsigned char)ch)) {
            int start = i++;
            while (i < length && (isalnum((unsigned char)line[i]) || line[i] == '.'))
                i++;
            tokens.push_back({start, i - start, TokenType::Number});
            continue;
        }

        i++;
        tokens.push_back({i - 1, 1, TokenType::Normal});
    }

    return state;
}
