#include "lexer.h"
#include <cstring>
#include <cctype>

// Find a closing tag like </script> or </style> in a line; returns offset or -1.
static int findClosingTag(const char *line, int length, const char *tagName)
{
    int tn = (int)strlen(tagName);
    for (int i = 0; i + 2 + tn < length; i++) {
        if (line[i] == '<' && line[i+1] == '/' &&
            strncasecmp(line + i + 2, tagName, tn) == 0) {
            int j = i + 2 + tn;
            if (j < length && (line[j] == '>' || line[j] == ' ' || line[j] == '\t'))
                return i;
        }
    }
    return -1;
}

LexerState HtmlLexer::tokenizeLine(const char *line, int length,
                                   LexerState inState,
                                   std::vector<Token> &tokens)
{
    int i = 0;
    LexerState state = inState;

    // ── Embedded JS inside <script>...</script> ───────────────────────
    if (state == LexerState::InHtmlScript) {
        int closeAt = findClosingTag(line, length, "script");
        int jsEnd = (closeAt >= 0) ? closeAt : length;
        if (jsEnd > 0) {
            JavaScriptLexer js;
            js.tokenizeLine(line, jsEnd, LexerState::Normal, tokens);
        }
        if (closeAt >= 0) {
            // emit </script> as tag
            int j = closeAt;
            while (j < length && line[j] != '>') j++;
            if (j < length) j++;
            tokens.push_back({closeAt, j - closeAt, TokenType::Tag});
            state = LexerState::Normal;
            i = j;
            // fall through to normal tokenization for rest of line
        } else {
            return state;
        }
    }

    // ── Embedded CSS inside <style>...</style> ────────────────────────
    if (state == LexerState::InHtmlStyle) {
        int closeAt = findClosingTag(line, length, "style");
        int cssEnd = (closeAt >= 0) ? closeAt : length;
        if (cssEnd > 0) {
            CssLexer css;
            css.tokenizeLine(line, cssEnd, LexerState::Normal, tokens);
        }
        if (closeAt >= 0) {
            int j = closeAt;
            while (j < length && line[j] != '>') j++;
            if (j < length) j++;
            tokens.push_back({closeAt, j - closeAt, TokenType::Tag});
            state = LexerState::Normal;
            i = j;
        } else {
            return state;
        }
    }

    while (i < length) {
        // Block comment continuation
        if (state == LexerState::InBlockComment) {
            int start = i;
            while (i < length) {
                if (i + 2 < length && line[i] == '-' && line[i+1] == '-' && line[i+2] == '>') {
                    i += 3;
                    tokens.push_back({start, i - start, TokenType::Comment});
                    state = LexerState::Normal;
                    break;
                }
                i++;
            }
            if (state == LexerState::InBlockComment)
                tokens.push_back({start, i - start, TokenType::Comment});
            continue;
        }

        char ch = line[i];

        // HTML comment <!--
        if (ch == '<' && i + 3 < length &&
            line[i+1] == '!' && line[i+2] == '-' && line[i+3] == '-') {
            int start = i;
            i += 4;
            state = LexerState::InBlockComment;
            while (i < length) {
                if (i + 2 < length && line[i] == '-' && line[i+1] == '-' && line[i+2] == '>') {
                    i += 3;
                    state = LexerState::Normal;
                    break;
                }
                i++;
            }
            tokens.push_back({start, i - start, TokenType::Comment});
            continue;
        }

        // PHP block embedded in HTML
        if (ch == '<' && i + 1 < length && line[i+1] == '?') {
            int tagLen = 2;
            if (i + 2 < length && line[i+2] == '=') tagLen = 3;
            else if (i + 4 < length && strncmp(line + i + 2, "php", 3) == 0) tagLen = 5;
            tokens.push_back({i, tagLen, TokenType::PhpDelimiter});
            i += tagLen;
            continue;
        }

        if (ch == '?' && i + 1 < length && line[i+1] == '>') {
            tokens.push_back({i, 2, TokenType::PhpDelimiter});
            i += 2;
            continue;
        }

        // Tags
        if (ch == '<') {
            int tagStartIdx = i;
            tokens.push_back({i, 1, TokenType::Tag});
            i++;

            bool isClosing = false;
            if (i < length && line[i] == '/') {
                tokens.push_back({i, 1, TokenType::Tag});
                i++;
                isClosing = true;
            }

            // Tag name
            int nameStart = i;
            while (i < length && (isalnum((unsigned char)line[i]) ||
                                  line[i] == '-' || line[i] == '_'))
                i++;
            std::string tagName(line + nameStart, i - nameStart);
            std::string tagLower = tagName;
            for (auto &c : tagLower) c = (char)tolower((unsigned char)c);
            if (i > nameStart)
                tokens.push_back({nameStart, i - nameStart, TokenType::Tag});

            // Attributes
            while (i < length && line[i] != '>') {
                if (isspace((unsigned char)line[i])) {
                    tokens.push_back({i, 1, TokenType::Normal});
                    i++;
                    continue;
                }

                if (line[i] == '/' && i + 1 < length && line[i+1] == '>') {
                    tokens.push_back({i, 2, TokenType::Tag});
                    i += 2;
                    // self-closing — skip embed activation
                    tagLower.clear();
                    break;
                }

                if (line[i] == '=') {
                    tokens.push_back({i, 1, TokenType::Operator});
                    i++;
                    while (i < length && isspace((unsigned char)line[i])) {
                        tokens.push_back({i, 1, TokenType::Normal});
                        i++;
                    }
                    if (i < length && (line[i] == '"' || line[i] == '\'')) {
                        char quote = line[i];
                        int strStart = i++;
                        while (i < length && line[i] != quote) i++;
                        if (i < length) i++;
                        tokens.push_back({strStart, i - strStart, TokenType::AttrValue});
                    }
                    continue;
                }

                int attrStart = i;
                while (i < length && !isspace((unsigned char)line[i]) &&
                       line[i] != '=' && line[i] != '>' && line[i] != '/')
                    i++;
                if (i > attrStart)
                    tokens.push_back({attrStart, i - attrStart, TokenType::Attribute});
            }

            if (i < length && line[i] == '>') {
                tokens.push_back({i, 1, TokenType::Tag});
                i++;

                // Activate embed mode if this is opening <script> or <style>
                if (!isClosing && !tagLower.empty()) {
                    LexerState embed = LexerState::Normal;
                    if (tagLower == "script") embed = LexerState::InHtmlScript;
                    else if (tagLower == "style") embed = LexerState::InHtmlStyle;
                    if (embed != LexerState::Normal) {
                        // Tokenize remainder of this line in the embedded language
                        int remainStart = i;
                        int closeAt = findClosingTag(line + remainStart,
                                                     length - remainStart,
                                                     embed == LexerState::InHtmlScript ? "script" : "style");
                        int embedEnd = (closeAt >= 0) ? remainStart + closeAt : length;
                        if (embedEnd > remainStart) {
                            if (embed == LexerState::InHtmlScript) {
                                JavaScriptLexer js;
                                std::vector<Token> sub;
                                js.tokenizeLine(line + remainStart,
                                                embedEnd - remainStart,
                                                LexerState::Normal, sub);
                                for (auto &t : sub) {
                                    t.start += remainStart;
                                    tokens.push_back(t);
                                }
                            } else {
                                CssLexer css;
                                std::vector<Token> sub;
                                css.tokenizeLine(line + remainStart,
                                                 embedEnd - remainStart,
                                                 LexerState::Normal, sub);
                                for (auto &t : sub) {
                                    t.start += remainStart;
                                    tokens.push_back(t);
                                }
                            }
                        }
                        if (closeAt >= 0) {
                            int j = remainStart + closeAt;
                            while (j < length && line[j] != '>') j++;
                            if (j < length) j++;
                            tokens.push_back({remainStart + closeAt,
                                              j - (remainStart + closeAt),
                                              TokenType::Tag});
                            i = j;
                        } else {
                            state = embed;
                            i = length;
                        }
                    }
                }
            }
            (void)tagStartIdx;
            continue;
        }

        // HTML entities &...;
        if (ch == '&') {
            int start = i++;
            while (i < length && line[i] != ';' && !isspace((unsigned char)line[i]) &&
                   (i - start) < 12)
                i++;
            if (i < length && line[i] == ';')
                i++;
            tokens.push_back({start, i - start, TokenType::HtmlEntity});
            continue;
        }

        // Normal text
        tokens.push_back({i, 1, TokenType::Normal});
        i++;
    }

    return state;
}
