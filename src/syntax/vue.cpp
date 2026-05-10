#include "lexer.h"

// Vue SFC: HtmlLexer already handles <template>, <script>, <style> sections,
// switching to JS/CSS for embedded content.
LexerState VueLexer::tokenizeLine(const char *line, int length,
                                  LexerState inState,
                                  std::vector<Token> &tokens)
{
    HtmlLexer html;
    return html.tokenizeLine(line, length, inState, tokens);
}
