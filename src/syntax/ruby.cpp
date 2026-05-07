#include "lexer.h"
#include <cctype>

static const std::unordered_set<std::string> kRbKeywords = {
    "BEGIN","END","alias","and","begin","break","case","class","def","defined?",
    "do","else","elsif","end","ensure","false","for","if","in","module","next",
    "nil","not","or","redo","rescue","retry","return","self","super","then",
    "true","undef","unless","until","when","while","yield","require","require_relative",
    "include","extend","attr_reader","attr_writer","attr_accessor","private",
    "protected","public"
};
static const std::unordered_set<std::string> kRbTypes = {
    "Array","Hash","String","Integer","Float","Symbol","Range","Regexp",
    "Proc","Lambda","Class","Module","Object","Numeric","Comparable","Enumerable"
};

LexerState RubyLexer::tokenizeLine(const char *line, int length,
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
                if (line[i] == '\\' && i + 1 < length) i++;
                i++;
            }
            if (i < length) i++;
            tokens.push_back({start, i - start, TokenType::String});
            continue;
        }

        if (ch == ':' && i + 1 < length &&
            (isalpha((unsigned char)line[i+1]) || line[i+1] == '_')) {
            int start = i++;
            while (i < length && (isalnum((unsigned char)line[i]) || line[i] == '_'))
                i++;
            tokens.push_back({start, i - start, TokenType::Builtin});
            continue;
        }

        if (ch == '@' || ch == '$') {
            int start = i++;
            if (ch == '@' && i < length && line[i] == '@') i++;
            while (i < length && (isalnum((unsigned char)line[i]) || line[i] == '_'))
                i++;
            tokens.push_back({start, i - start, TokenType::Variable});
            continue;
        }

        if (isdigit((unsigned char)ch)) {
            int start = i++;
            while (i < length && (isalnum((unsigned char)line[i]) || line[i] == '.' || line[i] == '_'))
                i++;
            tokens.push_back({start, i - start, TokenType::Number});
            continue;
        }

        if (isalpha((unsigned char)ch) || ch == '_') {
            int start = i;
            while (i < length && (isalnum((unsigned char)line[i]) || line[i] == '_'))
                i++;
            if (i < length && (line[i] == '?' || line[i] == '!')) i++;
            std::string word(line + start, i - start);
            if (kRbKeywords.count(word))
                tokens.push_back({start, i - start, TokenType::Keyword});
            else if (kRbTypes.count(word))
                tokens.push_back({start, i - start, TokenType::Type});
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
