#pragma once
#include "minic/ast.hpp"
#include <optional>
#include <unordered_map>
namespace minic {
struct Symbol { SymbolId id; std::string name; Span declaration; };
// The AST is annotated in place. This non-owning view is issued only on success.
// The original Program must outlive its BoundProgram.
struct BoundProgram { const Program& ast; std::vector<Symbol> symbols; };
class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(DiagnosticEngine& diagnostics) : diagnostics_(diagnostics) {}
    std::optional<BoundProgram> analyze(Program& program);
private:
    DiagnosticEngine& diagnostics_;
    std::vector<std::unordered_map<std::string, SymbolId>> scopes_;
    std::vector<Symbol> symbols_;
    SymbolId lookup(const std::string& name, Span span);
    void expression(Expr& expr);
    void statement(Stmt& stmt);
};
}
