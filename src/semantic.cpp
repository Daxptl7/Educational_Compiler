#include "minic/semantic.hpp"
namespace minic {
SymbolId SemanticAnalyzer::lookup(const std::string& name, Span span) {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        const auto found = scope->find(name);
        if (found != scope->end()) return found->second;
    }
    diagnostics_.error(span, "undeclared name '" + name + "'"); return unresolved;
}
void SemanticAnalyzer::expression(Expr& e) {
    switch (e.kind) {
    case ExprKind::Literal: break;
    case ExprKind::Variable: e.symbol = lookup(e.name, e.span); break;
    case ExprKind::Unary: expression(*e.left); break;
    case ExprKind::Binary: expression(*e.left); expression(*e.right); break;
    }
}
void SemanticAnalyzer::statement(Stmt& s) {
    switch (s.kind) {
    case StmtKind::Declaration:
        expression(*s.expression); // The new declaration is not yet visible.
        if (scopes_.back().count(s.name) != 0) {
            diagnostics_.error(s.nameSpan, "duplicate declaration of '" + s.name + "'");
        } else {
            s.symbol = symbols_.size();
            symbols_.push_back({s.symbol, s.name, s.nameSpan});
            scopes_.back().emplace(s.name, s.symbol);
        }
        break;
    case StmtKind::Assignment:
        s.symbol = lookup(s.name, s.nameSpan); expression(*s.expression); break;
    case StmtKind::Print: expression(*s.expression); break;
    case StmtKind::Block:
        scopes_.emplace_back();
        for (auto& child : s.items) statement(*child);
        scopes_.pop_back(); break;
    case StmtKind::If: case StmtKind::While:
        expression(*s.expression); statement(*s.body);
        if (s.alternative) statement(*s.alternative);
        break;
    }
}
std::optional<BoundProgram> SemanticAnalyzer::analyze(Program& p) {
    if (diagnostics_.hasErrors()) return std::nullopt;
    scopes_.clear(); symbols_.clear(); scopes_.emplace_back();
    for (auto& s : p.items) statement(*s);
    if (diagnostics_.hasErrors()) return std::nullopt;
    return BoundProgram{p, symbols_};
}
}
