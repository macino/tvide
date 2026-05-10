#include "lexer.h"
#include <cstring>

const std::unordered_set<std::string> PhpLexer::keywords = {
    "abstract", "and", "array", "as", "break", "callable", "case", "catch",
    "class", "clone", "const", "continue", "declare", "default", "die", "do",
    "echo", "else", "elseif", "empty", "enddeclare", "endfor", "endforeach",
    "endif", "endswitch", "endwhile", "eval", "exit", "extends", "final",
    "finally", "fn", "for", "foreach", "function", "global", "goto", "if",
    "implements", "include", "include_once", "instanceof", "insteadof",
    "interface", "isset", "list", "match", "namespace", "new", "or", "print",
    "private", "protected", "public", "readonly", "require", "require_once",
    "return", "static", "switch", "throw", "trait", "try", "unset", "use",
    "var", "while", "xor", "yield", "yield from",
    "true", "false", "null", "self", "parent", "enum",
};

const std::unordered_set<std::string> PhpLexer::types = {
    "int", "float", "string", "bool", "array", "object", "void", "mixed",
    "never", "null", "iterable", "resource", "callable",
};

const std::unordered_set<std::string> PhpLexer::builtins = {
    "strlen", "substr", "strpos", "str_replace", "explode", "implode",
    "array_push", "array_pop", "array_merge", "array_map", "array_filter",
    "array_keys", "array_values", "in_array", "count", "sizeof",
    "isset", "unset", "empty", "var_dump", "print_r", "json_encode",
    "json_decode", "file_get_contents", "file_put_contents", "preg_match",
    "preg_replace", "sprintf", "printf", "trim", "ltrim", "rtrim",
    "strtolower", "strtoupper", "ucfirst", "lcfirst", "nl2br",
    "htmlspecialchars", "htmlentities", "urlencode", "urldecode",
    "base64_encode", "base64_decode", "md5", "sha1", "date", "time",
    "strtotime", "mktime", "header", "setcookie", "session_start",
    "define", "defined", "is_array", "is_string", "is_int", "is_null",
    "class_exists", "method_exists", "property_exists",
};

LexerState PhpLexer::tokenizeLine(const char *line, int length,
                                  LexerState inState,
                                  std::vector<Token> &tokens)
{
    int i = 0;
    LexerState state = inState;

    // ── Outside <?php : run HtmlLexer until next <? or <?php ─────────
    if (state == LexerState::InPhpHtml) {
        // Find next <?
        int phpAt = -1;
        for (int k = 0; k + 1 < length; k++) {
            if (line[k] == '<' && line[k+1] == '?') { phpAt = k; break; }
        }
        int htmlEnd = (phpAt >= 0) ? phpAt : length;
        if (htmlEnd > 0) {
            HtmlLexer html;
            std::vector<Token> sub;
            html.tokenizeLine(line, htmlEnd, LexerState::Normal, sub);
            for (auto &t : sub) tokens.push_back(t);
        }
        if (phpAt < 0)
            return state; // still in HTML
        i = phpAt;
        state = LexerState::Normal;
        // fall through to PHP tokenization
    }

    while (i < length) {
        // Handle incoming block comment state
        if (state == LexerState::InBlockComment) {
            int start = i;
            while (i < length) {
                if (i + 1 < length && line[i] == '*' && line[i + 1] == '/') {
                    i += 2;
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

        // Handle incoming string state
        if (state == LexerState::InString) {
            int start = i;
            while (i < length) {
                if (line[i] == '\\' && i + 1 < length) {
                    i += 2;
                    continue;
                }
                if (line[i] == '"') {
                    i++;
                    tokens.push_back({start, i - start, TokenType::String});
                    state = LexerState::Normal;
                    break;
                }
                i++;
            }
            if (state == LexerState::InString)
                tokens.push_back({start, i - start, TokenType::String});
            continue;
        }

        char ch = line[i];

        // PHP open/close tags
        if (ch == '<' && i + 1 < length && line[i + 1] == '?') {
            int tagLen = 2;
            if (i + 2 < length && line[i + 2] == '=')
                tagLen = 3;
            else if (i + 4 < length && strncmp(line + i + 2, "php", 3) == 0)
                tagLen = 5;
            tokens.push_back({i, tagLen, TokenType::PhpDelimiter});
            i += tagLen;
            continue;
        }
        if (ch == '?' && i + 1 < length && line[i + 1] == '>') {
            tokens.push_back({i, 2, TokenType::PhpDelimiter});
            i += 2;
            // After ?> the rest of the file is HTML until next <?php.
            // Tokenize remainder with HtmlLexer and stay in PhpHtml state.
            int phpAt = -1;
            for (int k = i; k + 1 < length; k++) {
                if (line[k] == '<' && line[k+1] == '?') { phpAt = k; break; }
            }
            int htmlEnd = (phpAt >= 0) ? phpAt : length;
            if (htmlEnd > i) {
                HtmlLexer html;
                std::vector<Token> sub;
                html.tokenizeLine(line + i, htmlEnd - i, LexerState::Normal, sub);
                for (auto &t : sub) {
                    t.start += i;
                    tokens.push_back(t);
                }
            }
            if (phpAt < 0) {
                state = LexerState::InPhpHtml;
                return state;
            }
            i = phpAt;
            continue;
        }

        // Line comment //
        if (ch == '/' && i + 1 < length && line[i + 1] == '/') {
            tokens.push_back({i, length - i, TokenType::Comment});
            return state;
        }

        // Line comment #
        if (ch == '#') {
            tokens.push_back({i, length - i, TokenType::Comment});
            return state;
        }

        // Block comment /*
        if (ch == '/' && i + 1 < length && line[i + 1] == '*') {
            int start = i;
            i += 2;
            state = LexerState::InBlockComment;
            while (i < length) {
                if (i + 1 < length && line[i] == '*' && line[i + 1] == '/') {
                    i += 2;
                    state = LexerState::Normal;
                    break;
                }
                i++;
            }
            tokens.push_back({start, i - start, TokenType::Comment});
            continue;
        }

        // Strings
        if (ch == '"') {
            int start = i++;
            while (i < length) {
                if (line[i] == '\\' && i + 1 < length) {
                    i += 2;
                    continue;
                }
                if (line[i] == '"') {
                    i++;
                    break;
                }
                i++;
            }
            tokens.push_back({start, i - start, TokenType::String});
            continue;
        }

        if (ch == '\'') {
            int start = i++;
            while (i < length) {
                if (line[i] == '\\' && i + 1 < length) {
                    i += 2;
                    continue;
                }
                if (line[i] == '\'') {
                    i++;
                    break;
                }
                i++;
            }
            tokens.push_back({start, i - start, TokenType::String});
            continue;
        }

        // Variables $
        if (ch == '$' && i + 1 < length && isWordChar(line[i + 1])) {
            int start = i++;
            while (i < length && isWordChar(line[i]))
                i++;
            tokens.push_back({start, i - start, TokenType::Variable});
            continue;
        }

        // Numbers
        if (ch >= '0' && ch <= '9') {
            int start = i;
            while (i < length && (isWordChar(line[i]) || line[i] == '.'))
                i++;
            tokens.push_back({start, i - start, TokenType::Number});
            continue;
        }

        // Words (keywords, types, builtins, functions)
        if (isWordChar(ch)) {
            int start = i;
            int matchLen;
            if (matchKeyword(line, i, length, keywords, matchLen)) {
                tokens.push_back({i, matchLen, TokenType::Keyword});
                i += matchLen;
            } else if (matchKeyword(line, i, length, types, matchLen)) {
                tokens.push_back({i, matchLen, TokenType::Type});
                i += matchLen;
            } else if (matchKeyword(line, i, length, builtins, matchLen)) {
                tokens.push_back({i, matchLen, TokenType::Builtin});
                i += matchLen;
            } else {
                while (i < length && isWordChar(line[i]))
                    i++;
                // Check if followed by ( → function call
                int j = i;
                while (j < length && (line[j] == ' ' || line[j] == '\t'))
                    j++;
                if (j < length && line[j] == '(')
                    tokens.push_back({start, i - start, TokenType::Function});
                else
                    tokens.push_back({start, i - start, TokenType::Normal});
            }
            continue;
        }

        // Brackets
        if (ch == '(' || ch == ')' || ch == '[' || ch == ']' ||
            ch == '{' || ch == '}') {
            tokens.push_back({i, 1, TokenType::Bracket});
            i++;
            continue;
        }

        // Operators
        if (ch == '=' || ch == '+' || ch == '-' || ch == '*' || ch == '/' ||
            ch == '<' || ch == '>' || ch == '!' || ch == '&' || ch == '|' ||
            ch == '^' || ch == '%' || ch == '~' || ch == '?' || ch == ':' ||
            ch == '.' || ch == ',') {
            tokens.push_back({i, 1, TokenType::Operator});
            i++;
            continue;
        }

        // Whitespace/other
        tokens.push_back({i, 1, TokenType::Normal});
        i++;
    }

    return state;
}
