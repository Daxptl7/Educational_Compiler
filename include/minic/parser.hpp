#pragma once
#include "minic/ast.hpp"
#include <initializer_list>
namespace minic {
class Parser {
public:
    Parser(const std::vector<Token>& tokens, DiagnosticEngine& diagnostics)
        : tokens_(tokens), diagnostics_(diagnostics) {}
    Program parse();
private:
    const std::vector<Token>& tokens_;
    DiagnosticEngine& diagnostics_;
    std::size_t current_ = 0;
    const Token& peek() const;
    const Token& take();
    bool match(TokenKind kind);
    const Token& expect(TokenKind kind);
    [[noreturn]] void fail(const std::string& message);
    void recover(std::size_t start);
    std::vector<std::unique_ptr<Stmt>> items(bool inBlock);
    std::unique_ptr<Stmt> item();
    std::unique_ptr<Stmt> block();
    std::unique_ptr<Expr> expression();
    std::unique_ptr<Expr> equality();
    std::unique_ptr<Expr> comparison();
    std::unique_ptr<Expr> term();
    std::unique_ptr<Expr> factor();
    std::unique_ptr<Expr> unary();
    std::unique_ptr<Expr> primary();
    using ParseLevel = std::unique_ptr<Expr> (Parser::*)();
    std::unique_ptr<Expr> binary(ParseLevel next, std::initializer_list<TokenKind> ops);
};
}
