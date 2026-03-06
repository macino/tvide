#include "lexer.h"

LexerState PlainTextLexer::tokenizeLine(const char *, int length,
                                         LexerState,
                                         std::vector<Token> &tokens)
{
    if (length > 0)
        tokens.push_back({0, length, TokenType::Normal});
    return LexerState::Normal;
}
