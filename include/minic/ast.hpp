#pragma once
#include "minic/lexer.hpp"
#include <limits>
#include <memory>
namespace minic {
using SymbolId = std::size_t;
constexpr SymbolId unresolved = std::numeric_limits<SymbolId>::max();
enum class ExprKind { Literal, Variable, Unary, Binary };
struct Expr {
    ExprKind kind = ExprKind::Literal;
    Span span, operatorSpan;
    std::int64_t value = 0;
    std::string name;
    SymbolId symbol = unresolved;
    TokenKind op = TokenKind::End;
    std::unique_ptr<Expr> left, right;
};
enum class StmtKind { Declaration, Assignment, Print, Block, If, While };
struct Stmt {
    StmtKind kind = StmtKind::Block;
    Span span, nameSpan;
    std::string name;
    SymbolId symbol = unresolved;
    std::unique_ptr<Expr> expression;
    std::vector<std::unique_ptr<Stmt>> items;
    std::unique_ptr<Stmt> body, alternative;
};
struct Program { std::vector<std::unique_ptr<Stmt>> items; };
void dumpAST(const Program& program, std::ostream& out);
}
