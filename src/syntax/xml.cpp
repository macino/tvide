#include "lexer.h"
#include <cctype>
#include <cstring>

LexerState XmlLexer::tokenizeLine(const char *line, int length,
                                  LexerState inState,
                                  std::vector<Token> &tokens)
{
    // XML is very similar to HTML
    HtmlLexer html;
    return html.tokenizeLine(line, length, inState, tokens);
}
