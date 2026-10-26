#include "minic/lexer.hpp"
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace minic;
namespace {
int checks = 0;
void require(bool condition, const std::string& message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
void lexerTests() {
    SourceManager s("test.mc", "name 0 int print if else while + - * / % ! = == != < <= > >= ( ) { } ;");
    DiagnosticEngine d;
    auto t = Lexer(s, d).scan();
    require(!d.hasErrors(), "all tokens accepted");
    require(t.size() == 26, "every token plus EOF");
    for (std::size_t i = 0; i < 25; ++i)
        require(static_cast<std::size_t>(t[i].kind) == i + 1, "token kind order");
    require(t.back().kind == TokenKind::End, "EOF token");
    SourceManager adjacent("test.mc", "===!==<<=>>=!! intx print_ _a9 // ok\n/* a\nb */ 42");
    DiagnosticEngine ad;
    auto a = Lexer(adjacent, ad).scan();
    const std::vector<TokenKind> expected{TokenKind::EqualEqual, TokenKind::Equal,
        TokenKind::BangEqual, TokenKind::Equal, TokenKind::Less, TokenKind::LessEqual,
        TokenKind::Greater, TokenKind::GreaterEqual, TokenKind::Bang, TokenKind::Bang,
        TokenKind::Identifier, TokenKind::Identifier, TokenKind::Identifier, TokenKind::Integer, TokenKind::End};
    require(!ad.hasErrors() && a.size() == expected.size(), "maximal munch and comments");
    for (std::size_t i = 0; i < a.size(); ++i) require(a[i].kind == expected[i], "adjacent token");
    auto loc = adjacent.location(a[13].span.begin);
    require(loc.line == 3 && loc.column == 6, "line and column after comments");
    for (const auto& text : {"9223372036854775808", "@", "/*", "/* no end*", "&|"}) {
        SourceManager bad("bad.mc", text); DiagnosticEngine bd;
        auto tokens = Lexer(bad, bd).scan();
        require(bd.hasErrors() && tokens.back().kind == TokenKind::End, "lexical error controlled");
    }
    SourceManager max("max.mc", "9223372036854775807"); DiagnosticEngine md;
    require(Lexer(max, md).scan()[0].integer == std::numeric_limits<std::int64_t>::max()
            && !md.hasErrors(), "maximum literal");
    SourceManager empty("empty.mc", ""); DiagnosticEngine ed;
    require(Lexer(empty, ed).scan().size() == 1, "empty input");
    std::ostringstream out; dumpTokens(t, s, out);
    require(out.str().find("1:1 IDENT \"name\"") == 0, "deterministic token dump");
}

}
int main() {
    try { lexerTests(); std::cout << checks << " checks passed\n"; return 0; }
    catch (const std::exception& e) { std::cerr << "FAILED: " << e.what() << "\n"; return 1; }
}
