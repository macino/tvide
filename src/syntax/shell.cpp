#include "lexer.h"
#include <cctype>

static const std::unordered_set<std::string> kShellKeywords = {
    "if","then","else","elif","fi","case","esac","for","while","until","do",
    "done","in","function","select","time","coproc","break","continue","return",
    "exit","local","declare","typeset","readonly","export","unset","shift",
    "trap","source"
};
static const std::unordered_set<std::string> kShellBuiltins = {
    "echo","printf","read","cd","pwd","pushd","popd","dirs","alias","unalias",
    "set","unset","let","eval","exec","test","true","false","kill","jobs","bg",
    "fg","wait","sleep","sudo","env","which","type","command","builtin","help",
    "history","umask","ulimit","mkdir","rm","ls","cp","mv","ln","cat","grep",
    "sed","awk","find","sort","uniq","head","tail","tr","cut","wc","xargs",
    "tar","gzip","gunzip","ssh","scp","curl","wget","git","make","docker",
    "npm","yarn","pip","python","node"
};

LexerState ShellLexer::tokenizeLine(const char *line, int length,
                                    LexerState inState,
                                    std::vector<Token> &tokens)
{
    int i = 0;
    LexerState state = inState;

    while (i < length) {
        char ch = line[i];

        if (ch == '#') {
            tokens.push_back({i, length - i, TokenType::Comment});
            return state;
        }

        if (ch == '"' || ch == '\'') {
            char q = ch;
            int start = i++;
            while (i < length && line[i] != q) {
                if (q == '"' && line[i] == '\\' && i + 1 < length) i++;
                i++;
            }
            if (i < length) i++;
            tokens.push_back({start, i - start, TokenType::String});
            continue;
        }

        if (ch == '`') {
            int start = i++;
            while (i < length && line[i] != '`') {
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
            } else if (i < length && line[i] == '(') {
                int depth = 1;
                i++;
                while (i < length && depth > 0) {
                    if (line[i] == '(') depth++;
                    else if (line[i] == ')') depth--;
                    i++;
                }
            } else {
                while (i < length &&
                       (isalnum((unsigned char)line[i]) || line[i] == '_'))
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

        if (isalpha((unsigned char)ch) || ch == '_') {
            int start = i;
            while (i < length &&
                   (isalnum((unsigned char)line[i]) || line[i] == '_' || line[i] == '-'))
                i++;
            std::string word(line + start, i - start);
            if (kShellKeywords.count(word))
                tokens.push_back({start, i - start, TokenType::Keyword});
            else if (kShellBuiltins.count(word))
                tokens.push_back({start, i - start, TokenType::Builtin});
            else
                tokens.push_back({start, i - start, TokenType::Normal});
            continue;
        }

        if (ch == '(' || ch == ')' || ch == '[' || ch == ']' ||
            ch == '{' || ch == '}') {
            tokens.push_back({i, 1, TokenType::Bracket});
            i++;
            continue;
        }

        if (ch == '|' || ch == '&' || ch == '<' || ch == '>' || ch == ';' ||
            ch == '=' || ch == '!' || ch == '*' || ch == '?' || ch == '+' ||
            ch == '-' || ch == '/' || ch == '.' || ch == ',' || ch == ':') {
            tokens.push_back({i, 1, TokenType::Operator});
            i++;
            continue;
        }

        tokens.push_back({i, 1, TokenType::Normal});
        i++;
    }

    return state;
}
