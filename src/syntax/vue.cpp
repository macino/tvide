#include "lexer.h"
#include <cstring>
#include <cctype>

// Vue SFC lexer delegates to HTML/JS/CSS based on <template>, <script>, <style>
LexerState VueLexer::tokenizeLine(const char *line, int length,
                                  LexerState inState,
                                  std::vector<Token> &tokens)
{
    // Detect section tags
    if (length >= 8) {
        std::string l(line, length);
        // Check for opening/closing tags of sections
        if (l.find("<template") != std::string::npos ||
            l.find("</template>") != std::string::npos ||
            l.find("<script") != std::string::npos ||
            l.find("</script>") != std::string::npos ||
            l.find("<style") != std::string::npos ||
            l.find("</style>") != std::string::npos) {
            // Highlight the whole line as tag
            HtmlLexer html;
            return html.tokenizeLine(line, length, LexerState::Normal, tokens);
        }
    }

    // Delegate based on state: use HTML lexer as default for Vue
    HtmlLexer html;
    return html.tokenizeLine(line, length, inState, tokens);
}
