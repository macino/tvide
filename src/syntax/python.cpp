#include "lexer.h"
#include <cctype>

static const std::unordered_set<std::string> kPyKeywords = {
    "False","None","True","and","as","assert","async","await","break","class",
    "continue","def","del","elif","else","except","finally","for","from",
    "global","if","import","in","is","lambda","nonlocal","not","or","pass",
    "raise","return","try","while","with","yield","match","case"
};
static const std::unordered_set<std::string> kPyTypes = {
    "int","float","str","bool","bytes","bytearray","list","tuple","dict",
    "set","frozenset","object","type","range","complex","memoryview"
};
static const std::unordered_set<std::string> kPyBuiltins = {
    "print","len","range","open","input","abs","all","any","bin","callable",
    "chr","compile","dir","divmod","enumerate","eval","exec","exit","filter",
    "format","getattr","hasattr","hash","hex","id","isinstance","issubclass",
    "iter","map","max","min","next","oct","ord","pow","property","repr",
    "reversed","round","setattr","slice","sorted","staticmethod","sum",
    "super","vars","zip","__init__","__main__","self","cls"
};

LexerState PythonLexer::tokenizeLine(const char *line, int length,
                                     LexerState inState,
                                     std::vector<Token> &tokens)
{
    int i = 0;
    LexerState state = inState;

    if (state == LexerState::InMultiLineString) {
        int start = 0;
        while (i + 2 < length) {
            if ((line[i] == '"' && line[i+1] == '"' && line[i+2] == '"') ||
                (line[i] == '\'' && line[i+1] == '\'' && line[i+2] == '\'')) {
                i += 3;
                state = LexerState::Normal;
                break;
            }
            i++;
        }
        if (state == LexerState::InMultiLineString) i = length;
        tokens.push_back({start, i - start, TokenType::String});
        if (state == LexerState::InMultiLineString) return state;
    }

    while (i < length) {
        char ch = line[i];

        if (ch == '#') {
            tokens.push_back({i, length - i, TokenType::Comment});
            return state;
        }

        if (i + 2 < length &&
            ((ch == '"' && line[i+1] == '"' && line[i+2] == '"') ||
             (ch == '\'' && line[i+1] == '\'' && line[i+2] == '\''))) {
            int start = i;
            char q = ch;
            i += 3;
            bool closed = false;
            while (i + 2 < length) {
                if (line[i] == q && line[i+1] == q && line[i+2] == q) {
                    i += 3;
                    closed = true;
                    break;
                }
                i++;
            }
            if (!closed) {
                state = LexerState::InMultiLineString;
                i = length;
            }
            tokens.push_back({start, i - start, TokenType::String});
            continue;
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

        if (isdigit((unsigned char)ch)) {
            int start = i++;
            while (i < length && (isalnum((unsigned char)line[i]) ||
                                  line[i] == '.' || line[i] == '_')) i++;
            tokens.push_back({start, i - start, TokenType::Number});
            continue;
        }

        if (isalpha((unsigned char)ch) || ch == '_') {
            int start = i;
            while (i < length && (isalnum((unsigned char)line[i]) || line[i] == '_'))
                i++;
            std::string word(line + start, i - start);
            if (kPyKeywords.count(word))
                tokens.push_back({start, i - start, TokenType::Keyword});
            else if (kPyTypes.count(word))
                tokens.push_back({start, i - start, TokenType::Type});
            else if (kPyBuiltins.count(word))
                tokens.push_back({start, i - start, TokenType::Builtin});
            else {
                int j = i;
                while (j < length && isspace((unsigned char)line[j])) j++;
                if (j < length && line[j] == '(')
                    tokens.push_back({start, i - start, TokenType::Function});
                else
                    tokens.push_back({start, i - start, TokenType::Normal});
            }
            continue;
        }

        if (ch == '(' || ch == ')' || ch == '[' || ch == ']' ||
            ch == '{' || ch == '}') {
            tokens.push_back({i, 1, TokenType::Bracket});
            i++;
            continue;
        }

        if (ch == '@') {
            int start = i++;
            while (i < length && (isalnum((unsigned char)line[i]) || line[i] == '_' || line[i] == '.'))
                i++;
            tokens.push_back({start, i - start, TokenType::Preprocessor});
            continue;
        }

        if (ch == '=' || ch == '+' || ch == '-' || ch == '*' || ch == '/' ||
            ch == '<' || ch == '>' || ch == '!' || ch == '&' || ch == '|' ||
            ch == '^' || ch == '%' || ch == '~' || ch == '?' || ch == ':' ||
            ch == '.' || ch == ',' || ch == ';') {
            tokens.push_back({i, 1, TokenType::Operator});
            i++;
            continue;
        }

        tokens.push_back({i, 1, TokenType::Normal});
        i++;
    }

    return state;
}
