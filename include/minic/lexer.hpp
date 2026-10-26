#pragma once
#include "minic/source.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace minic {
enum class TokenKind {
    End, Identifier, Integer, Int, Print, If, Else, While,
    Plus, Minus, Star, Slash, Percent, Bang, Equal,
    EqualEqual, BangEqual, Less, LessEqual, Greater, GreaterEqual,
    LeftParen, RightParen, LeftBrace, RightBrace, Semicolon
};
struct Token {
    TokenKind kind;
    std::string lexeme;
    Span span;
    std::int64_t integer = 0;
};
const char* tokenName(TokenKind kind);
class Lexer {
public:
    Lexer(const SourceManager& source, DiagnosticEngine& diagnostics)
        : source_(source), diagnostics_(diagnostics) {}
    std::vector<Token> scan();
private:
    const SourceManager& source_;
    DiagnosticEngine& diagnostics_;
};
void dumpTokens(const std::vector<Token>& tokens, const SourceManager& source, std::ostream& out);
}
