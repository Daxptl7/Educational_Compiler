#include "minic/lexer.hpp"
#include "minic/parser.hpp"
#include "minic/semantic.hpp"
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
void parserTests() {
    SourceManager s("ast.mc", "int a = 1 + 2 * 3; print(10 - 3 - 2); print(!!-a); print((1 + 2) * 3); print(1 < 2 == 1); { if (a) { while (a) { a = a - 1; } } else { print(0); } }");
    DiagnosticEngine d; auto t = Lexer(s, d).scan(); auto p = Parser(t, d).parse();
    require(!d.hasErrors() && p.items.size() == 6, "nested grammar");
    std::ostringstream out; dumpAST(p, out);
    for (const auto& fragment : {"(+ 1 (* 2 3))", "(- (- 10 3) 2)", "(! (! (- a)))", "(* (+ 1 2) 3)", "(== (< 1 2) 1)", "(while a", "(else"})
        require(out.str().find(fragment) != std::string::npos, "AST structure: " + std::string(fragment));
    require(p.items[0]->expression->operatorSpan.begin == 10, "operator span retained");
    for (const auto& text : {"int a = 1", "print(1 + ); print(2);", "{ int x = 2;", "}", "if (1) print(2);", "if (1) {} else if (0) {}", "while (1) {", "int = ; ; print(; }", "print((1);", "x = (x = 2);", "", "print(+);"}) {
        SourceManager bad("parse.mc", text); DiagnosticEngine bd;
        auto bt = Lexer(bad, bd).scan(); auto bp = Parser(bt, bd).parse();
        require(std::string(text).empty() ? !bd.hasErrors() : bd.hasErrors(), "parser recovery/EOF: " + std::string(text));
    }
    SourceManager recovery("recover.mc", "int a = 1 int b = 2; print(3 + ); print(4);"); DiagnosticEngine rd;
    auto rt = Lexer(recovery, rd).scan(); auto rp = Parser(rt, rd).parse();
    require(rd.errors().size() == 2 && rp.items.size() == 2, "panic recovery preserves subsequent statements");
}
struct Frontend {
    SourceManager source;
    DiagnosticEngine diagnostics;
    std::vector<Token> tokens;
    Program ast;
    std::optional<BoundProgram> bound;
    explicit Frontend(const std::string& text) : source("test.mc", text) {
        tokens = Lexer(source, diagnostics).scan();
        if (!diagnostics.hasErrors()) ast = Parser(tokens, diagnostics).parse();
        auto result = SemanticAnalyzer(diagnostics).analyze(ast);
        if (result) bound.emplace(std::move(*result));
    }
};
void semanticTests() {
    Frontend good("int x = 10; { int x = x + 1; print(x); } print(x); while (x) { int y = x; x = x - 1; }");
    require(good.bound.has_value() && good.bound->symbols.size() == 3, "valid shadowing and loop local");
    const auto& outer = *good.ast.items[0]; const auto& inner = *good.ast.items[1]->items[0];
    require(outer.symbol != inner.symbol && inner.expression->left->symbol == outer.symbol, "initializer sees outer symbol");
    require(good.ast.items[1]->items[1]->expression->symbol == inner.symbol, "nested reference bound");
    require(good.ast.items[2]->expression->symbol == outer.symbol, "outer restored");
    for (const auto& text : {"print(x);", "x = 2;", "int x = 1; int x = 2;", "int x = x + 1;", "{int x = 1;} print(x);", "print(x); int x = 1;", "while (0) {int y = 1;} print(y);", "int while = 2;", "int x;"}) {
        Frontend bad(text);
        require(!bad.bound && bad.diagnostics.hasErrors(), "invalid names/syntax blocked: " + std::string(text));
    }
    Frontend branches("if (1) { int x = 1; } else { int x = 2; } int x = 3;");
    require(branches.bound && branches.bound->symbols.size() == 3, "branch scopes independent");
}

}
int main() {
    try { lexerTests(); parserTests(); semanticTests(); std::cout << checks << " checks passed\n"; return 0; }
    catch (const std::exception& e) { std::cerr << "FAILED: " << e.what() << "\n"; return 1; }
}
