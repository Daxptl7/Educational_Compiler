#include "minic/lexer.hpp"
#include <charconv>
#include <iomanip>
#include <stdexcept>
#include <unordered_map>
namespace minic {
const char* tokenName(TokenKind kind) {
    switch (kind) {
    case TokenKind::End: return "EOF";
    case TokenKind::Identifier: return "IDENT";
    case TokenKind::Integer: return "INTEGER";
    case TokenKind::Int: return "int";
    case TokenKind::Print: return "print";
    case TokenKind::If: return "if";
    case TokenKind::Else: return "else";
    case TokenKind::While: return "while";
    case TokenKind::Plus: return "+";
    case TokenKind::Minus: return "-";
    case TokenKind::Star: return "*";
    case TokenKind::Slash: return "/";
    case TokenKind::Percent: return "%";
    case TokenKind::Bang: return "!";
    case TokenKind::Equal: return "=";
    case TokenKind::EqualEqual: return "==";
    case TokenKind::BangEqual: return "!=";
    case TokenKind::Less: return "<";
    case TokenKind::LessEqual: return "<=";
    case TokenKind::Greater: return ">";
    case TokenKind::GreaterEqual: return ">=";
    case TokenKind::LeftParen: return "(";
    case TokenKind::RightParen: return ")";
    case TokenKind::LeftBrace: return "{";
    case TokenKind::RightBrace: return "}";
    case TokenKind::Semicolon: return ";";
    }
    throw std::logic_error("invalid token kind");
}
namespace {
bool digit(char c) { return c >= '0' && c <= '9'; }
bool letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v'; }
}
std::vector<Token> Lexer::scan() {
    const auto& s = source_.text();
    std::vector<Token> result;
    std::size_t i = 0;
    const std::unordered_map<std::string, TokenKind> keywords{
        {"int", TokenKind::Int}, {"print", TokenKind::Print}, {"if", TokenKind::If},
        {"else", TokenKind::Else}, {"while", TokenKind::While}};
    auto add = [&](TokenKind kind, std::size_t start, std::int64_t value = 0) {
        result.push_back({kind, s.substr(start, i - start), {start, i}, value});
    };
    while (i < s.size()) {
        if (space(s[i])) { ++i; continue; }
        const auto start = i;
        if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '/') {
            i += 2;
            while (i < s.size() && s[i] != '\n') ++i;
            continue;
        }
        if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '*') {
            i += 2;
            while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/')) ++i;
            if (i + 1 >= s.size()) {
                diagnostics_.error({start, s.size()}, "unterminated block comment");
                i = s.size();
            } else i += 2;
            continue;
        }
        if (letter(s[i])) {
            while (i < s.size() && (letter(s[i]) || digit(s[i]))) ++i;
            const auto found = keywords.find(s.substr(start, i - start));
            add(found == keywords.end() ? TokenKind::Identifier : found->second, start);
            continue;
        }
        if (digit(s[i])) {
            while (i < s.size() && digit(s[i])) ++i;
            std::int64_t value = 0;
            const auto parsed = std::from_chars(s.data() + start, s.data() + i, value);
            if (parsed.ec != std::errc{})
                diagnostics_.error({start, i}, "integer literal exceeds INT64_MAX");
            add(TokenKind::Integer, start, value);
            continue;
        }
        const char c = s[i++];
        auto paired = [&](TokenKind single, TokenKind pair) {
            if (i < s.size() && s[i] == '=') { ++i; add(pair, start); }
            else add(single, start);
        };
        switch (c) {
        case '+': add(TokenKind::Plus, start); break;
        case '-': add(TokenKind::Minus, start); break;
        case '*': add(TokenKind::Star, start); break;
        case '/': add(TokenKind::Slash, start); break;
        case '%': add(TokenKind::Percent, start); break;
        case '!': paired(TokenKind::Bang, TokenKind::BangEqual); break;
        case '=': paired(TokenKind::Equal, TokenKind::EqualEqual); break;
        case '<': paired(TokenKind::Less, TokenKind::LessEqual); break;
        case '>': paired(TokenKind::Greater, TokenKind::GreaterEqual); break;
        case '(': add(TokenKind::LeftParen, start); break;
        case ')': add(TokenKind::RightParen, start); break;
        case '{': add(TokenKind::LeftBrace, start); break;
        case '}': add(TokenKind::RightBrace, start); break;
        case ';': add(TokenKind::Semicolon, start); break;
        default: diagnostics_.error({start, i}, "invalid character"); break;
        }
    }
    result.push_back({TokenKind::End, "", {i, i}, 0});
    return result;
}
void dumpTokens(const std::vector<Token>& tokens, const SourceManager& source, std::ostream& out) {
    for (const auto& t : tokens) {
        const auto loc = source.location(t.span.begin);
        out << loc.line << ':' << loc.column << ' ' << tokenName(t.kind)
            << ' ' << std::quoted(t.lexeme) << '\n';
    }
}
}
