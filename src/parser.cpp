#include "minic/parser.hpp"
#include <algorithm>
#include <stdexcept>
namespace minic {
namespace { struct ParseFailure {}; }
const Token& Parser::peek() const { return tokens_.at(current_); }
const Token& Parser::take() {
    const auto& t = peek();
    if (t.kind != TokenKind::End) ++current_;
    return t;
}
bool Parser::match(TokenKind kind) {
    if (peek().kind != kind) return false;
    take(); return true;
}
const Token& Parser::expect(TokenKind kind) {
    if (peek().kind != kind) fail(std::string("expected '") + tokenName(kind) + "', got '" + tokenName(peek().kind) + "'");
    return take();
}
[[noreturn]] void Parser::fail(const std::string& message) {
    diagnostics_.error(peek().span, message); throw ParseFailure{};
}
void Parser::recover(std::size_t start) {
    // Leave a closing brace for the enclosing block. Otherwise always progress.
    if (current_ == start && peek().kind != TokenKind::End && peek().kind != TokenKind::RightBrace) take();
    while (peek().kind != TokenKind::End && peek().kind != TokenKind::RightBrace) {
        if (match(TokenKind::Semicolon)) return;
        switch (peek().kind) {
        case TokenKind::Int: case TokenKind::Print: case TokenKind::If:
        case TokenKind::While: case TokenKind::LeftBrace: case TokenKind::Identifier: return;
        default: take(); break;
        }
    }
}
std::vector<std::unique_ptr<Stmt>> Parser::items(bool inBlock) {
    std::vector<std::unique_ptr<Stmt>> result;
    while (peek().kind != TokenKind::End) {
        if (peek().kind == TokenKind::RightBrace) {
            if (inBlock) break;
            diagnostics_.error(take().span, "unmatched closing brace");
            continue;
        }
        const auto start = current_;
        try { result.push_back(item()); }
        catch (ParseFailure&) { recover(start); }
    }
    return result;
}
Program Parser::parse() {
    if (tokens_.empty() || tokens_.back().kind != TokenKind::End)
        throw std::logic_error("parser requires EOF-terminated tokens");
    return {items(false)};
}
std::unique_ptr<Stmt> Parser::block() {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::Block;
    s->span.begin = expect(TokenKind::LeftBrace).span.begin;
    s->items = items(true);
    s->span.end = expect(TokenKind::RightBrace).span.end;
    return s;
}
std::unique_ptr<Stmt> Parser::item() {
    if (peek().kind == TokenKind::LeftBrace) return block();
    auto s = std::make_unique<Stmt>();
    s->span.begin = peek().span.begin;
    if (match(TokenKind::Int)) {
        s->kind = StmtKind::Declaration;
        const auto& name = expect(TokenKind::Identifier);
        s->name = name.lexeme; s->nameSpan = name.span;
        expect(TokenKind::Equal); s->expression = expression();
    } else if (peek().kind == TokenKind::Identifier) {
        s->kind = StmtKind::Assignment;
        const auto& name = take(); s->name = name.lexeme; s->nameSpan = name.span;
        expect(TokenKind::Equal); s->expression = expression();
    } else if (match(TokenKind::Print)) {
        s->kind = StmtKind::Print;
        expect(TokenKind::LeftParen); s->expression = expression(); expect(TokenKind::RightParen);
    } else if (peek().kind == TokenKind::If || peek().kind == TokenKind::While) {
        s->kind = take().kind == TokenKind::If ? StmtKind::If : StmtKind::While;
        expect(TokenKind::LeftParen); s->expression = expression(); expect(TokenKind::RightParen);
        s->body = block();
        if (s->kind == StmtKind::If && match(TokenKind::Else)) s->alternative = block();
        s->span.end = s->alternative ? s->alternative->span.end : s->body->span.end;
        return s;
    } else fail("expected declaration, assignment, print, block, if, or while");
    s->span.end = expect(TokenKind::Semicolon).span.end;
    return s;
}
std::unique_ptr<Expr> Parser::binary(ParseLevel next, std::initializer_list<TokenKind> ops) {
    auto left = (this->*next)();
    while (std::find(ops.begin(), ops.end(), peek().kind) != ops.end()) {
        const auto& op = take();
        auto node = std::make_unique<Expr>(); node->kind = ExprKind::Binary;
        node->op = op.kind; node->operatorSpan = op.span;
        node->left = std::move(left); node->right = (this->*next)();
        node->span = {node->left->span.begin, node->right->span.end}; left = std::move(node);
    }
    return left;
}
std::unique_ptr<Expr> Parser::expression() { return equality(); }
std::unique_ptr<Expr> Parser::equality() { return binary(&Parser::comparison, {TokenKind::EqualEqual, TokenKind::BangEqual}); }
std::unique_ptr<Expr> Parser::comparison() { return binary(&Parser::term, {TokenKind::Less, TokenKind::LessEqual, TokenKind::Greater, TokenKind::GreaterEqual}); }
std::unique_ptr<Expr> Parser::term() { return binary(&Parser::factor, {TokenKind::Plus, TokenKind::Minus}); }
std::unique_ptr<Expr> Parser::factor() { return binary(&Parser::unary, {TokenKind::Star, TokenKind::Slash, TokenKind::Percent}); }
std::unique_ptr<Expr> Parser::unary() {
    if (peek().kind == TokenKind::Plus || peek().kind == TokenKind::Minus || peek().kind == TokenKind::Bang) {
        const auto& op = take(); auto e = std::make_unique<Expr>(); e->kind = ExprKind::Unary;
        e->op = op.kind; e->operatorSpan = op.span; e->left = unary();
        e->span = {op.span.begin, e->left->span.end}; return e;
    }
    return primary();
}
std::unique_ptr<Expr> Parser::primary() {
    auto e = std::make_unique<Expr>();
    if (peek().kind == TokenKind::Integer) {
        const auto& t = take(); e->kind = ExprKind::Literal; e->value = t.integer; e->span = t.span;
    } else if (peek().kind == TokenKind::Identifier) {
        const auto& t = take(); e->kind = ExprKind::Variable; e->name = t.lexeme; e->span = t.span;
    } else if (match(TokenKind::LeftParen)) {
        const auto begin = tokens_[current_ - 1].span.begin;
        e = expression(); const auto end = expect(TokenKind::RightParen).span.end;
        e->span = {begin, end};
    } else fail("expected integer, identifier, or parenthesized expression");
    return e;
}
}
