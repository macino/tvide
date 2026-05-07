#include "lexer.h"
#include <cctype>

const std::unordered_set<std::string> &CLikeLexer::types() const
{
    static const std::unordered_set<std::string> none;
    return none;
}

LexerState CLikeLexer::tokenizeLine(const char *line, int length,
                                    LexerState inState,
                                    std::vector<Token> &tokens)
{
    int i = 0;
    LexerState state = inState;

    if (state == LexerState::InBlockComment) {
        int start = 0;
        while (i < length) {
            if (i + 1 < length && line[i] == '*' && line[i+1] == '/') {
                i += 2;
                state = LexerState::Normal;
                break;
            }
            i++;
        }
        tokens.push_back({start, i - start, TokenType::Comment});
        if (state == LexerState::InBlockComment) return state;
    }

    if (hasPreprocessor()) {
        int j = 0;
        while (j < length && (line[j] == ' ' || line[j] == '\t')) j++;
        if (j < length && line[j] == '#') {
            tokens.push_back({j, length - j, TokenType::Preprocessor});
            return state;
        }
    }

    while (i < length) {
        char ch = line[i];

        if (ch == '/' && i + 1 < length) {
            if (line[i+1] == '/') {
                tokens.push_back({i, length - i, TokenType::Comment});
                return state;
            }
            if (line[i+1] == '*') {
                int start = i;
                i += 2;
                state = LexerState::InBlockComment;
                while (i < length) {
                    if (i + 1 < length && line[i] == '*' && line[i+1] == '/') {
                        i += 2;
                        state = LexerState::Normal;
                        break;
                    }
                    i++;
                }
                tokens.push_back({start, i - start, TokenType::Comment});
                continue;
            }
        }

        if (ch == '"' || ch == '\'') {
            char quote = ch;
            int start = i++;
            while (i < length && line[i] != quote) {
                if (line[i] == '\\' && i + 1 < length) i++;
                i++;
            }
            if (i < length) i++;
            tokens.push_back({start, i - start, TokenType::String});
            continue;
        }

        if (isdigit((unsigned char)ch) ||
            (ch == '.' && i + 1 < length && isdigit((unsigned char)line[i+1]))) {
            int start = i;
            if (ch == '0' && i + 1 < length &&
                (line[i+1] == 'x' || line[i+1] == 'X' ||
                 line[i+1] == 'b' || line[i+1] == 'B' ||
                 line[i+1] == 'o' || line[i+1] == 'O'))
                i += 2;
            while (i < length &&
                   (isalnum((unsigned char)line[i]) || line[i] == '.' ||
                    line[i] == '_' || line[i] == '\''))
                i++;
            tokens.push_back({start, i - start, TokenType::Number});
            continue;
        }

        if (isalpha((unsigned char)ch) || ch == '_') {
            int start = i;
            while (i < length &&
                   (isalnum((unsigned char)line[i]) || line[i] == '_'))
                i++;
            std::string word(line + start, i - start);
            const auto &kws = keywords();
            const auto &tys = types();
            if (kws.count(word))
                tokens.push_back({start, i - start, TokenType::Keyword});
            else if (tys.count(word))
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
            ch == '.' || ch == ',' || ch == ';' || ch == '@' || ch == '#') {
            tokens.push_back({i, 1, TokenType::Operator});
            i++;
            continue;
        }

        tokens.push_back({i, 1, TokenType::Normal});
        i++;
    }

    return state;
}

// ── Per-language keyword tables ─────────────────────────────────────────

#define DEF_LEX(Cls, KW_LIST, TY_LIST) \
const std::unordered_set<std::string> &Cls::keywords() const { \
    static const std::unordered_set<std::string> s KW_LIST; return s; } \
const std::unordered_set<std::string> &Cls::types() const { \
    static const std::unordered_set<std::string> s TY_LIST; return s; }

#define LIST(...) { __VA_ARGS__ }

DEF_LEX(CppLexer,
    LIST("alignas","alignof","and","and_eq","asm","auto","bitand","bitor","bool","break",
       "case","catch","char","char8_t","char16_t","char32_t","class","co_await","co_return","co_yield",
       "compl","concept","const","consteval","constexpr","constinit","const_cast","continue",
       "decltype","default","delete","do","double","dynamic_cast","else","enum","explicit",
       "export","extern","false","final","float","for","friend","goto","if","import","inline",
       "int","long","module","mutable","namespace","new","noexcept","not","not_eq","nullptr",
       "operator","or","or_eq","override","private","protected","public","register",
       "reinterpret_cast","requires","return","short","signed","sizeof","static",
       "static_assert","static_cast","struct","switch","template","this","thread_local",
       "throw","true","try","typedef","typeid","typename","union","unsigned","using",
       "virtual","void","volatile","wchar_t","while","xor","xor_eq"),
    LIST("size_t","ptrdiff_t","intptr_t","uintptr_t","int8_t","int16_t","int32_t","int64_t",
       "uint8_t","uint16_t","uint32_t","uint64_t","string","vector","map","set",
       "unordered_map","unordered_set","array","pair","tuple","optional","variant",
       "shared_ptr","unique_ptr","weak_ptr","function","FILE","NULL"))

DEF_LEX(JavaLexer,
    LIST("abstract","assert","boolean","break","byte","case","catch","char","class","const",
       "continue","default","do","double","else","enum","extends","final","finally","float",
       "for","goto","if","implements","import","instanceof","int","interface","long",
       "native","new","package","private","protected","public","record","return","sealed",
       "short","static","strictfp","super","switch","synchronized","this","throw","throws",
       "transient","try","void","volatile","while","yield","var","true","false","null",
       "non-sealed","permits"),
    LIST("String","Object","Integer","Long","Double","Float","Boolean","Byte","Short",
       "Character","List","Map","Set","ArrayList","HashMap","HashSet","Optional",
       "Stream","Collection","Iterable","Iterator","Exception","RuntimeException",
       "Thread","Runnable","System"))

DEF_LEX(CSharpLexer,
    LIST("abstract","as","async","await","base","bool","break","byte","case","catch","char",
       "checked","class","const","continue","decimal","default","delegate","do","double",
       "dynamic","else","enum","event","explicit","extern","false","finally","fixed","float",
       "for","foreach","goto","if","implicit","in","int","interface","internal","is","lock",
       "long","namespace","new","null","object","operator","out","override","params","partial",
       "private","protected","public","readonly","record","ref","return","sbyte","sealed",
       "short","sizeof","stackalloc","static","string","struct","switch","this","throw",
       "true","try","typeof","uint","ulong","unchecked","unsafe","ushort","using","var",
       "virtual","void","volatile","while","yield","get","set","init","when","where",
       "with","global","nameof","required","scoped"),
    LIST("String","Int32","Int64","Boolean","Object","Task","List","Dictionary","IEnumerable",
       "IList","IDictionary","Action","Func","DateTime","Guid","Exception","Console"))

DEF_LEX(GoLexer,
    LIST("break","case","chan","const","continue","default","defer","else","fallthrough",
       "for","func","go","goto","if","import","interface","map","package","range","return",
       "select","struct","switch","type","var","true","false","nil","iota"),
    LIST("string","int","int8","int16","int32","int64","uint","uint8","uint16","uint32",
       "uint64","uintptr","byte","rune","float32","float64","complex64","complex128",
       "bool","error","any"))

DEF_LEX(RustLexer,
    LIST("as","async","await","break","const","continue","crate","dyn","else","enum","extern",
       "false","fn","for","if","impl","in","let","loop","match","mod","move","mut","pub",
       "ref","return","Self","self","static","struct","super","trait","true","try","type",
       "unsafe","use","where","while","yield","abstract","become","box","do","final",
       "macro","override","priv","typeof","unsized","virtual"),
    LIST("String","str","Vec","Option","Result","Box","Rc","Arc","HashMap","HashSet","BTreeMap",
       "i8","i16","i32","i64","i128","isize","u8","u16","u32","u64","u128","usize",
       "f32","f64","bool","char","Cell","RefCell","Mutex","RwLock"))

DEF_LEX(KotlinLexer,
    LIST("as","break","class","continue","do","else","false","for","fun","if","in","interface",
       "is","null","object","package","return","super","this","throw","true","try","typealias",
       "typeof","val","var","when","while","by","catch","constructor","delegate","dynamic",
       "field","file","finally","get","import","init","param","property","receiver","set",
       "setparam","value","where","abstract","actual","annotation","companion","const",
       "crossinline","data","enum","expect","external","final","infix","inline","inner",
       "internal","lateinit","noinline","open","operator","out","override","private",
       "protected","public","reified","sealed","suspend","tailrec","vararg"),
    LIST("Int","Long","Short","Byte","Float","Double","Boolean","Char","String","Array",
       "List","MutableList","Map","MutableMap","Set","MutableSet","Any","Unit","Nothing",
       "Pair","Triple"))

DEF_LEX(SwiftLexer,
    LIST("associatedtype","class","deinit","enum","extension","fileprivate","func","import",
       "init","inout","internal","let","open","operator","private","precedencegroup",
       "protocol","public","rethrows","static","struct","subscript","typealias","var",
       "break","case","catch","continue","default","defer","do","else","fallthrough","for",
       "guard","if","in","repeat","return","throw","switch","where","while","as","Any",
       "false","is","nil","self","Self","super","throws","true","try","async","await",
       "actor","any","some","convenience","didSet","dynamic","final","get","indirect",
       "infix","lazy","left","mutating","none","nonmutating","optional","override","postfix",
       "prefix","required","right","set","unowned","weak","willSet"),
    LIST("Int","Int8","Int16","Int32","Int64","UInt","UInt8","UInt16","UInt32","UInt64",
       "Float","Double","Bool","String","Character","Array","Dictionary","Set","Optional",
       "Range","Result","Void"))

DEF_LEX(LuaLexer,
    LIST("and","break","do","else","elseif","end","false","for","function","goto","if","in",
       "local","nil","not","or","repeat","return","then","true","until","while"),
    LIST("string","table","math","io","os","coroutine","debug","package","require","print",
       "pairs","ipairs","tostring","tonumber","type","setmetatable","getmetatable"))
