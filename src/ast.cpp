#include "minic/ast.hpp"
namespace minic {
namespace {
void expr(const Expr& e, std::ostream& out) {
    switch (e.kind) {
    case ExprKind::Literal: out << e.value; break;
    case ExprKind::Variable: out << e.name; break;
    case ExprKind::Unary:
        out << '(' << tokenName(e.op) << ' '; expr(*e.left, out); out << ')'; break;
    case ExprKind::Binary:
        out << '(' << tokenName(e.op) << ' '; expr(*e.left, out); out << ' ';
        expr(*e.right, out); out << ')'; break;
    }
}
void stmt(const Stmt& s, std::ostream& out, std::size_t depth) {
    const std::string pad(depth * 2, ' '); out << pad;
    switch (s.kind) {
    case StmtKind::Declaration: case StmtKind::Assignment:
        out << '(' << (s.kind == StmtKind::Declaration ? "declare " : "assign ") << s.name << ' ';
        expr(*s.expression, out); out << ")\n"; break;
    case StmtKind::Print: out << "(print "; expr(*s.expression, out); out << ")\n"; break;
    case StmtKind::Block:
        out << "(block\n";
        for (const auto& child : s.items) stmt(*child, out, depth + 1);
        out << pad << ")\n"; break;
    case StmtKind::If: case StmtKind::While:
        out << '(' << (s.kind == StmtKind::If ? "if " : "while "); expr(*s.expression, out); out << '\n';
        stmt(*s.body, out, depth + 1);
        if (s.alternative) { out << pad << "  (else\n"; stmt(*s.alternative, out, depth + 2); out << pad << "  )\n"; }
        out << pad << ")\n"; break;
    }
}
}
void dumpAST(const Program& p, std::ostream& out) {
    out << "(program\n";
    for (const auto& s : p.items) stmt(*s, out, 1);
    out << ")\n";
}
}
